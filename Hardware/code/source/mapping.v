//////////////////////////////////////////////////////////////////////////////////
//
// Author 			:	Praveen Kumar Pendyala
// Create Date		:   05/27/13
// Modify Date		:	16/01/14
// Module Name		:   mapping
// Project Name     :   PDL
// Target Devices	: 	Xilinx Vertix 5, XUPV5 110T
// Tool versions	: 	13.2 ISE
//
// Description:
// This module maps the data received by the SircHandler (from PC) to the AluPuf.
// Issues appropriate trigger signals to start puf execution.
// Maintain state of the PUF operation - Idle or execute
//
//////////////////////////////////////////////////////////////////////////////////

`timescale 1ns / 1ps
`default_nettype none

module mapping #(
	parameter IN_WIDTH = 128,
	parameter OUT_WIDTH = 16
)(
	input wire clk,
	input wire reset,
	input wire trigger,
	input wire [IN_WIDTH-1:0] dataIn,
	input wire [15:0] opA,
	input wire [15:0] opB,
	output reg done,
	output reg [OUT_WIDTH-1:0] dataOut
	);

	wire [15:0] response;
	reg startPUF;
	reg PUFreset;
	reg [4:0] countWait;
	integer ind;
	reg [15:0] sum;

	//FSM States
	localparam IDLE = 0;
	localparam COMPUTE = 1;

	//State Register
	reg mp_state;

	reg [IN_WIDTH-1:0] buffer;

	always @ (posedge clk) begin
		if (reset) begin
			done <= 0;
			dataOut <= 0;
			mp_state <= IDLE;
			startPUF <= 0;
			countWait <= 0;
			PUFreset <=1;
		end

		else begin
			case(mp_state)
				IDLE: begin
					done <= 0;
					sum <= 0;
					PUFreset <= 0;
					countWait <=0;
					startPUF <=0;
					if(trigger == 1)
						mp_state <= COMPUTE;
						buffer <= dataIn;
				end

				COMPUTE: begin
					startPUF <=1;
					countWait <= countWait + 1;
					if (countWait == 15) begin //wait for 10 clock cycles
						startPUF<=0;
						dataOut <= response;
						done <= 1;
						mp_state <= IDLE;
						PUFreset <=1;
					end
				end
			endcase
		end
	end

AluPuf alupuf (
	.CHALLENGE(buffer),
	.RESPONSE(response),
	.trigger(startPUF),
	.reset(PUFreset),
	.a(opA),
	.b(opB)
);

endmodule
