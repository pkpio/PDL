 `timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company:
// Engineer:
//
// Create Date:    14:47:20 09/15/2007
// Design Name:
// Module Name:    puf
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

module pdl_puf #(
    parameter PDL_SWITCH_LENGTH = 63 // Number of PDL based switchs in a PUF. Actual value is param_value + 1
)(	
   input [PDL_SWITCH_LENGTH:0] s_tp, s_btm,
   input  s1, s2, reset,
   output o
   );


	 wire [PDL_SWITCH_LENGTH:0] i1,i2;
	 wire puf_out;//q_buf, puf_out;


(* KEEP_HIERARCHY="TRUE" *)
pdl_based_switch sarray [PDL_SWITCH_LENGTH:0] (
	.i1({s1,i1[PDL_SWITCH_LENGTH:1]}),
	.i2({s2,i2[PDL_SWITCH_LENGTH:1]}),
    .select_tp(s_tp[PDL_SWITCH_LENGTH:0]),
    .select_btm(s_btm[PDL_SWITCH_LENGTH:0]),
    .o1(i1[PDL_SWITCH_LENGTH:0]),
    .o2(i2[PDL_SWITCH_LENGTH:0])
    );

FDC FDC1 (.Q (puf_out),
          .C (i2[0]),
          .CLR (reset),
          .D (i1[0])
          );


(* BEL ="D6LUT" *) (* LOCK_PINS = "all" *)
LUT1 #(
	.INIT(2'b10) // Specify LUT Contents
) LUT1_inst_2 (
	.O(o), // LUT general output
	.I0(puf_out) // LUT input
);


endmodule
