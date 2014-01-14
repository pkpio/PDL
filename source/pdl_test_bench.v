`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Engineer:       Praveen Kumar Pendyala
//
// Create Date:    11:34:00 06/14/2010
// Design Name:    PDL
// Module Name:    test_bench
// Project Name:   PDL
// Target Devices: Virtex 5 lx
// Tool versions:  Xilinix ISE 13.3
// Description:    This is a test bench for pdl - programmable delay logic based puf.
// This module instantiates 16 instances of a pdl based puf. The working of the puf
// can be found in the pdl_puf module. The purpose of 16 instances is to demonstrate
// the placement of pdl switch in all/most of the available space on the FPGA device.
//
// Dependencies:
//
// Revision:
// Revision 0.02 - Modified as a publishable document
// Revision 0.01 - File Created
//
// Additional Comments: This is a standalone pdl test bench which takes inputs from
// switches on FPGA and displays output on LED. However, a better use case version
// can be found at https://github.com/praveendath92/dual_core_PUF_with_PDL_and_ethernet_SW
//
// Only one instance is being tested currently. You may include others with proper pin connections
//
//////////////////////////////////////////////////////////////////////////////////


//////////// DONT FORGET TO WRITE ABOUT EQUIVALENT REGISTER REMOVAL 

module pdl_test_bench #(
    parameter PDL_SWITCH_LENGTH = 63 // Number of PDL based switchs in a PUF. Actual value is param_value + 1
)(
    input reset,               // For system reset
    input challenge_top,       // Configuration bit for top pdl line
    input challenge_bottom,    // Configuration bit for bottom pdl line
    input trigger,             // Trigger to start execution
    output response_bit        // Response from the puf on to an LED. Only one random puf instance (out of 16) is considered.
    );

//wire [15:0] RESPONSE;          // Response bits from all PUFs.
wire [63:0] CHALLENGE_UP;      // An array of bits filled with the bit value of challenge_top
wire [63:0] CHALLENGE_DOWN;    // An array of bits filled with the bit value of challenge_top

//(* KEEP = "TRUE" *)
//reg [15:0] sig;                 // The signal value will be stored in 15 local registers close to the respective PUF lines of issuing trigger.

//(* KEEP = "TRUE" *)wire [15:0] c1;                 // for connecting sig with pdl line when trigger is one or else ground.
//(* KEEP = "TRUE" *)wire [15:0] c2;                 // for connecting sig with pdl line when trigger is one or else ground.

(* KEEP = "TRUE" *)wire c1;
(* KEEP = "TRUE" *)wire c2;

// Build 64-bit challenges per pdl line. In this test bench we will be using same value for all bits in a line.
assign CHALLENGE_UP = (challenge_top==1)?64'hffff:64'h0000;
assign CHALLENGE_DOWN = (challenge_bottom==1)?64'hffff:64'h0000;

// Testing over LED
//assign response_bit = RESPONSE[0]; // Only one instance is being used.

(* KEEP = "TRUE" *)reg signal;

always @ (posedge trigger) begin
	signal <= 1;
end

//Make two lines from same signal for racing along pdl lines.
assign c1 = (trigger==1)?signal:0;
assign c2 = (trigger==1)?signal:0;

(* KEEP_HIERARCHY="TRUE" *)
pdl_puf puf1 (
	.s_tp(CHALLENGE_UP[63:0]),
	.s_btm(CHALLENGE_DOWN[63:0]),
	.s1(c1),
	.s2(c2),
	.reset(reset),
	.o(response_bit)
	);

// Only one instance of the PUF is being using to demonstrate. The rest are just to show placement and are not being tested.
//pdl_puf puf2 (.s_tp(CHALLENGE_UP[63:0]), .s_btm(CHALLENGE_DOWN[63:0]), .s1(c1[0]), .s2(c2[0]), .reset(reset), .o(RESPONSE[0]));
//pdl_puf puf2 (.s_tp(CHALLENGE_UP[63:0]), .s_btm(CHALLENGE_DOWN[63:0]), .s1(c1[1]), .s2(c2[1]), .reset(reset), .o(RESPONSE[1]));
//pdl_puf puf3 (.s_tp(CHALLENGE_UP[63:0]), .s_btm(CHALLENGE_DOWN[63:0]), .s1(c1[2]), .s2(c2[2]), .reset(reset), .o(RESPONSE[2]));
//pdl_puf puf4 (.s_tp(CHALLENGE_UP[63:0]), .s_btm(CHALLENGE_DOWN[63:0]), .s1(c1[3]), .s2(c2[3]), .reset(reset), .o(RESPONSE[3]));
//pdl_puf puf5 (.s_tp(CHALLENGE_UP[63:0]), .s_btm(CHALLENGE_DOWN[63:0]), .s1(c1[4]), .s2(c2[4]), .reset(reset), .o(RESPONSE[4]));
//pdl_puf puf6 (.s_tp(CHALLENGE_UP[63:0]), .s_btm(CHALLENGE_DOWN[63:0]), .s1(c1[5]), .s2(c2[5]), .reset(reset), .o(RESPONSE[5]));
//pdl_puf puf7 (.s_tp(CHALLENGE_UP[63:0]), .s_btm(CHALLENGE_DOWN[63:0]), .s1(c1[6]), .s2(c2[6]), .reset(reset), .o(RESPONSE[6]));
//pdl_puf puf8 (.s_tp(CHALLENGE_UP[63:0]), .s_btm(CHALLENGE_DOWN[63:0]), .s1(c1[7]), .s2(c2[7]), .reset(reset), .o(RESPONSE[7]));
//pdl_puf puf9 (.s_tp(CHALLENGE_UP[63:0]), .s_btm(CHALLENGE_DOWN[63:0]), .s1(c1[8]), .s2(c2[8]), .reset(reset), .o(RESPONSE[8]));
//pdl_puf puf10 (.s_tp(CHALLENGE_UP[63:0]), .s_btm(CHALLENGE_DOWN[63:0]), .s1(c1[9]), .s2(c2[9]), .reset(reset), .o(RESPONSE[9]));
//pdl_puf puf11 (.s_tp(CHALLENGE_UP[63:0]), .s_btm(CHALLENGE_DOWN[63:0]), .s1(c1[10]), .s2(c2[10]), .reset(reset), .o(RESPONSE[10]));
//pdl_puf puf12 (.s_tp(CHALLENGE_UP[63:0]), .s_btm(CHALLENGE_DOWN[63:0]), .s1(c1[11]), .s2(c2[11]), .reset(reset), .o(RESPONSE[11]));
//pdl_puf puf13 (.s_tp(CHALLENGE_UP[63:0]), .s_btm(CHALLENGE_DOWN[63:0]), .s1(c1[12]), .s2(c2[12]), .reset(reset), .o(RESPONSE[12]));
//pdl_puf puf14 (.s_tp(CHALLENGE_UP[63:0]), .s_btm(CHALLENGE_DOWN[63:0]), .s1(c1[13]), .s2(c2[13]), .reset(reset), .o(RESPONSE[13]));
//pdl_puf puf15 (.s_tp(CHALLENGE_UP[63:0]), .s_btm(CHALLENGE_DOWN[63:0]), .s1(c1[14]), .s2(c2[14]), .reset(reset), .o(RESPONSE[14]));
//pdl_puf puf16 (.s_tp(CHALLENGE_UP[63:0]), .s_btm(CHALLENGE_DOWN[63:0]), .s1(c1[15]), .s2(c2[15]), .reset(reset), .o(RESPONSE[15]));

endmodule

