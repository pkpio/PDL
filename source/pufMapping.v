`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date:    11:06:00 06/14/2010 
// Design Name: 
// Module Name:    system 
// Project Name: 
// Target Devices: 
// Tool versions: 
// Description: 
//
// Dependencies: 
//
// Revision: 
// Revision 0.01 - File Created
// Additional Comments: 
//
//////////////////////////////////////////////////////////////////////////////////
module pufMapping(CHALLENGE, RESPONSE, trigger, reset, a, b);

output [15:0] RESPONSE;
input [127:0] CHALLENGE;
input trigger;
input reset;
input wire [15:0] a;
input wire [15:0] b;

wire [127:0] CHALLENGE;
wire [15:0] RESPONSE;

(* KEEP = "TRUE" *)
reg [15:0] a1;
reg [15:0] a2;
reg [15:0] b1;
reg [15:0] b2;

wire [15:0] c1;
wire [15:0] c2;

always @ (posedge trigger) begin
	a1 <= a;
	a2 <= a;
	b1 <= b;
	b2 <= b;
end

(* KEEP="TRUE" *)
assign c1 = (trigger==1)?(a1+b1):0;
assign c2 = (trigger==1)?(a2+b2):0;

(* KEEP_HIERARCHY="TRUE" *)
PDL_PUF puf1 (.s_tp(CHALLENGE[63:0]), .s_btm(CHALLENGE[127:64]), .s1(c1[0]), .s2(c2[0]), .reset(reset), .o(RESPONSE[0]));
PDL_PUF puf2 (.s_tp(CHALLENGE[63:0]), .s_btm(CHALLENGE[127:64]), .s1(c1[1]), .s2(c2[1]), .reset(reset), .o(RESPONSE[1]));
PDL_PUF puf3 (.s_tp(CHALLENGE[63:0]), .s_btm(CHALLENGE[127:64]), .s1(c1[2]), .s2(c2[2]), .reset(reset), .o(RESPONSE[2]));
PDL_PUF puf4 (.s_tp(CHALLENGE[63:0]), .s_btm(CHALLENGE[127:64]), .s1(c1[3]), .s2(c2[3]), .reset(reset), .o(RESPONSE[3]));
PDL_PUF puf5 (.s_tp(CHALLENGE[63:0]), .s_btm(CHALLENGE[127:64]), .s1(c1[4]), .s2(c2[4]), .reset(reset), .o(RESPONSE[4]));
PDL_PUF puf6 (.s_tp(CHALLENGE[63:0]), .s_btm(CHALLENGE[127:64]), .s1(c1[5]), .s2(c2[5]), .reset(reset), .o(RESPONSE[5]));
PDL_PUF puf7 (.s_tp(CHALLENGE[63:0]), .s_btm(CHALLENGE[127:64]), .s1(c1[6]), .s2(c2[6]), .reset(reset), .o(RESPONSE[6]));
PDL_PUF puf8 (.s_tp(CHALLENGE[63:0]), .s_btm(CHALLENGE[127:64]), .s1(c1[7]), .s2(c2[7]), .reset(reset), .o(RESPONSE[7]));
PDL_PUF puf9 (.s_tp(CHALLENGE[63:0]), .s_btm(CHALLENGE[127:64]), .s1(c1[8]), .s2(c2[8]), .reset(reset), .o(RESPONSE[8]));
PDL_PUF puf10 (.s_tp(CHALLENGE[63:0]), .s_btm(CHALLENGE[127:64]), .s1(c1[9]), .s2(c2[9]), .reset(reset), .o(RESPONSE[9]));
PDL_PUF puf11 (.s_tp(CHALLENGE[63:0]), .s_btm(CHALLENGE[127:64]), .s1(c1[10]), .s2(c2[10]), .reset(reset), .o(RESPONSE[10]));
PDL_PUF puf12 (.s_tp(CHALLENGE[63:0]), .s_btm(CHALLENGE[127:64]), .s1(c1[11]), .s2(c2[11]), .reset(reset), .o(RESPONSE[11]));
PDL_PUF puf13 (.s_tp(CHALLENGE[63:0]), .s_btm(CHALLENGE[127:64]), .s1(c1[12]), .s2(c2[12]), .reset(reset), .o(RESPONSE[12]));
PDL_PUF puf14 (.s_tp(CHALLENGE[63:0]), .s_btm(CHALLENGE[127:64]), .s1(c1[13]), .s2(c2[13]), .reset(reset), .o(RESPONSE[13]));
PDL_PUF puf15 (.s_tp(CHALLENGE[63:0]), .s_btm(CHALLENGE[127:64]), .s1(c1[14]), .s2(c2[14]), .reset(reset), .o(RESPONSE[14]));
PDL_PUF puf16 (.s_tp(CHALLENGE[63:0]), .s_btm(CHALLENGE[127:64]), .s1(c1[15]), .s2(c2[15]), .reset(reset), .o(RESPONSE[15]));


endmodule
  
