//////////////////////////////////////////////////////////////////////////////////
// 
// Author 			:	Praveen Kumar Pendyala
// **Modified the code of HW example for SIRC from MSR written bu Ken Eguro
//
// Create Date		:  05/27/13
// Modify Date		:	06/05/13
// Module Name		:  simpleTestModuleOne 
// Project Name	: 	SIRC_HW
// Target Devices	: 	Xilinx Vertix 5, XUPV5 110T
// Tool versions	: 	13.2 ISE
//
// Description: 
//	This module receives 128-bit challenge data from PC. Evaluates the PUFs response and sends it back to PC.
//
//	Bugs :
//	- While writing back to memory the first element is written twice (i.e., to memory addresses 0 and 1). Temporarily fix by writing 1 extra bit and also reading 1 extra but in software.
//////////////////////////////////////////////////////////////////////////////////

`timescale 1ns / 1ps
`default_nettype none

//This module demonstrates how a user can read from the parameter register file,
//	read from the input memory buffer, and write to the output memory buffer.
//We also show the basics of how the user's circuit should interact with
// userRunValue and userRunClear.
module simpleTestModuleOne #(
	//************ Input and output block memory parameters
	//The user's circuit communicates with the input and output memories as N-byte chunks
	//This should be some power of 2 >= 1.
	parameter INMEM_BYTE_WIDTH = 1,
	parameter OUTMEM_BYTE_WIDTH = 1,
	
	//How many N-byte words does the user's circuit use?
	parameter INMEM_ADDRESS_WIDTH = 17,
	parameter OUTMEM_ADDRESS_WIDTH = 13
)(
	input		wire 					clk,
	input		wire 					reset,
																														//A user application can only check the status of the run register and reset it to zero
	input		wire 					userRunValue,																//Read run register value
	output	reg					userRunClear,																//Reset run register
	
	//Parameter register file connections
	output 	reg															register32CmdReq,					//Parameter register handshaking request signal - assert to perform read or write
	input		wire 															register32CmdAck,					//Parameter register handshaking acknowledgment signal - when the req and ack ar both true fore 1 clock cycle, the request has been accepted
	output 	wire 		[31:0]											register32WriteData,				//Parameter register write data
	output 	reg		[7:0]												register32Address,				//Parameter register address
	output	wire 															register32WriteEn,				//When we put in a request command, are we doing a read or write?
	input 	wire 															register32ReadDataValid,		//After a read request is accepted, this line indicates that the read has returned and that the data is ready
	input 	wire 		[31:0]											register32ReadData,				//Parameter register read data

	//Input memory connections
	output 	reg															inputMemoryReadReq,				//Input memory handshaking request signal - assert to begin a read request
	input		wire 															inputMemoryReadAck,				//Input memory handshaking acknowledgement signal - when the req and ack are both true for 1 clock cycle, the request has been accepted
	output	reg		[(INMEM_ADDRESS_WIDTH - 1):0] 			inputMemoryReadAdd,				//Input memory read address - can be set the same cycle that the req line is asserted
	input 	wire 															inputMemoryReadDataValid,		//After a read request is accepted, this line indicates that the read has returned and that the data is ready
	input		wire 		[((INMEM_BYTE_WIDTH * 8) - 1):0] 		inputMemoryReadData,				//Input memory read data
	
	//Output memory connections
	output 	reg															outputMemoryWriteReq,			//Output memory handshaking request signal - assert to begin a write request
	input 	wire 															outputMemoryWriteAck,			//Output memory handshaking acknowledgement signal - when the req and ack are both true for 1 clock cycle, the request has been accepted
	output	reg		[(OUTMEM_ADDRESS_WIDTH - 1):0] 			outputMemoryWriteAdd,			//Output memory write address - can be set the same cycle that the req line is asserted
	output	reg		[((OUTMEM_BYTE_WIDTH * 8) - 1):0]		outputMemoryWriteData,			//Output memory write data
	output 	wire 		[(OUTMEM_BYTE_WIDTH - 1):0]				outputMemoryWriteByteMask,		//Allows byte-wise writes when multibyte words are used - each of the OUTMEM_USER_BYTE_WIDTH line can be 0 (do not write byte) or 1 (write byte)

	//8 optional LEDs for visual feedback & debugging
	output	reg 	[7:0]	LED
);
	//FSM states
	localparam  IDLE = 0;							// Waiting
	localparam  READING_IN_PARAMETERS = 1;	// Get values from the reg32 parameters
	localparam  READ = 2;							// Run (read from input, compute and write to output)
	localparam  WAIT_READ = 3;
	localparam  COMPUTE = 4;
	localparam  WRITE = 5;

	//Signal declarations
	//State registers
	reg [2:0] currState;
	
	//Challenge and Response holding registers as 2D matrices
	//SIRC sends each byte as 8-bit long so we are using 2D-arrays
	reg	[7:0]	challenge [0:15];
	reg	[7:0]	response	[0:1];

	//Challenge and Response holding variables as a single dimensional 128 bit arrays
	//The above redundant declaration could be avoided by appropriate conditions while reading or writing back
	//We use this because verilog doesn't support passing multidimensional arrays to other modules
	wire [127:0] challengeReg;
	wire [15:0]	responseReg;
	
	//Buffer to hold the responses in two runs. Will be merged into a single response finally.
	reg [31:0]	responseRegBuffer;
	
	
	//Flattening 2D arrays to 1D
	//Endianness has been adjusted in other modules and/or while building reponse 2D array
	assign challengeReg = {
		challenge[15],challenge[14],challenge[13],challenge[12],challenge[11],challenge[10],challenge[9],challenge[8],
		challenge[7],challenge[6],challenge[5],challenge[4],challenge[3],challenge[2],challenge[1],challenge[0]};
	
	//Endianness has been adjusted in other modules and/or while building challenge 1D array
	always @(*) begin
		{response[0], response[1]} <= responseReg;//responseRegBuffer[15:0] + responseRegBuffer[31:16];
	end
	
	//Counter
	reg paramCount;
	
	//Message parameters
	reg [31:0] A;
	reg [31:0] B;

	// We don't write to the register file and we only write whole bytes to the output memory
	assign register32WriteData = 32'd0;
	assign register32WriteEn = 0;
	assign outputMemoryWriteByteMask = {OUTMEM_BYTE_WIDTH{1'b1}};
	
	//Variables for execution
	reg inputDone;
	reg [5:0] memCount;	//Will be used while reading from memory to 128 bit challenge regs. Similarly while writing back from response regs
	reg [4:0] regCount;
	reg [7:0] bitCount;
	
	//PUF execution variables
	reg challenge_ready;
	wire response_ready;
	
	//Will be used to set the output of adder before issuing trigger so that both pos and neg edges can be detected
	reg oBeforeTrig;
	//There will 2 runs on PUF for each execution - 1. For posedge detection, 2. negedge detection
	//reg pufRunCount;


	initial begin
		currState = IDLE;
		A = 0;
		B = 0;
		
		userRunClear = 0;
		
		register32Address = 0;
		
		inputMemoryReadReq = 0;
		inputMemoryReadAdd = 0;
	
		outputMemoryWriteReq = 0;
		outputMemoryWriteAdd = 0;
		outputMemoryWriteData = 0;
		
		paramCount = 0;
		
		inputDone = 0;
	end
	

	always @(posedge clk) begin
		if(reset) begin
			currState <= IDLE;
			
			userRunClear <= 0;
			
			register32Address <= 0;
			
			inputMemoryReadReq <= 0;
			inputMemoryReadAdd <= 0;
			
			outputMemoryWriteReq <= 0;
			outputMemoryWriteAdd <= 0;
			outputMemoryWriteData <= 0;
			
			paramCount <= 0;
			
			inputDone <= 0;
			
		end
		else begin
			case(currState)
				IDLE: begin
					//Stop trying to clear the userRunRegister
					userRunClear <= 0;
					inputMemoryReadReq <= 0;
					LED <= 8'b00000000;
					
					//Wait till the run register goes high
					if(userRunValue == 1 && userRunClear != 1) begin
						//Start reading from the register file
						currState <= READING_IN_PARAMETERS;
						register32Address <= 0;
						register32CmdReq <= 1;
						paramCount <= 0;
					end
				end
				READING_IN_PARAMETERS: begin
					//We need to read 2 values from the parameter register file.
					//If the register file accepted the read, increment the address
					if(register32CmdAck == 1 && register32CmdReq == 1) begin
						register32Address <= register32Address + 1;
					end
					
					//If we just accepted a read from address 1, stop requesting reads
					if(register32CmdAck == 1 && register32Address == 8'd1)begin
						register32CmdReq <= 0;
					end
	
					//If a read came back, shift in the value from the register file
					if(register32ReadDataValid) begin
							A <= B;
							B <= register32ReadData;
							paramCount <= 1;
							
							//The above block act as a shift register for operands A and B
							if(paramCount == 1)begin
								//Start requesting input data and execution
								currState <= READ;
								inputMemoryReadReq <= 1;
								inputMemoryReadAdd <= 0;
								outputMemoryWriteAdd <= 0;
								inputDone <= 0;
								memCount <= 0;
							end
					end
				end
				READ: begin
					//Read for length of length obtained from params
					if(inputDone == 0) begin
						inputMemoryReadReq <= 1;
					end
					else begin
						inputMemoryReadReq <= 0;
					end
					
					//If the input memory accepted the last read, we can increment the address
					if(inputMemoryReadReq == 1 && inputMemoryReadAck == 1 && inputMemoryReadAdd != 15)begin
						inputMemoryReadAdd <= inputMemoryReadAdd + 1;
						currState <= WAIT_READ;
					end
					else if(inputMemoryReadReq == 1 && inputMemoryReadAck == 1 && inputMemoryReadAdd == 15)begin
						inputDone <= 1;
						LED[0] <= 1;
						currState <= WAIT_READ;
					end	
				end
				
				WAIT_READ: begin
					if (inputMemoryReadDataValid == 1) begin

						if(memCount <= 15) begin
							challenge[memCount] <= inputMemoryReadData;
							memCount <= memCount+1;
							currState <= READ;
						end
						else begin
							currState <= COMPUTE;
							regCount <= 0;
							bitCount <= 0;
							oBeforeTrig <= 0; //For detecting posedges on first run
							//pufRunCount <= 0;
						end	
					end				
				end
				
				COMPUTE: begin				
					if(regCount == 0) begin
						challenge_ready <= 1;
						regCount <= regCount+1;
					end
					else begin
						challenge_ready <= 0;
					end
					
					if(response_ready == 1) begin
						currState <= WRITE;
						memCount <= 0;
						regCount <= 0;
						outputMemoryWriteAdd <= 0;
					/* New model to be tried out later
						if(pufRunCount != 1) begin
							regCount <= 0;
							oBeforeTrig <= 1;
							pufRunCount <= 1;
							//Do another computation. For negedge detection this time
							currState <= COMPUTE;
							responseRegBuffer[15:0] <= responseReg[15:0];
						end
						else begin
							responseRegBuffer[31:16] <= responseReg[15:0];
							currState <= WRITE;
							memCount <= 0;
							regCount <= 0;
							outputMemoryWriteAdd <= 0;
						end
						*/
						
					end
					
				end
				
				WRITE: begin
					outputMemoryWriteReq <= 1;
						
					if(outputMemoryWriteAdd <= 1) begin
						outputMemoryWriteData <= response[outputMemoryWriteAdd];
					end

					//If we just wrote a value to the output memory this cycle, increment the address
					//NOTE : Due to bug described above we write on bit more by using length instead of lengthMinus1
					if(outputMemoryWriteReq == 1  && outputMemoryWriteAck == 1 && outputMemoryWriteAdd != 2) begin
						outputMemoryWriteAdd <= outputMemoryWriteAdd + 1;
						memCount <= memCount+1;
						currState <= WRITE;
					end	

					//Stop writing and go back to IDLE state if writing reached length of data
					if(outputMemoryWriteReq == 1  && outputMemoryWriteAck == 1 && outputMemoryWriteAdd == 2) begin
						outputMemoryWriteReq <= 0;
						currState <= IDLE;
						userRunClear <= 1;
					end				
				end				
				
			endcase
		end
   end
	
//This maps the received data among PUF modules and also issues the approriate RUN signals
mapping #(
		.IN_WIDTH(128),
		.OUT_WIDTH(16)
	) mp (
		.clk(clk),
		.reset(reset),
		.trigger(challenge_ready),
		.dataIn(challengeReg),
		.done(response_ready),
		.dataOut(responseReg),
		.opA(A[15:0]),
		.opB(B[15:0])
		//.oBeforeTrig(oBeforeTrig)
		//.flag(flag5)
	);
	
endmodule
