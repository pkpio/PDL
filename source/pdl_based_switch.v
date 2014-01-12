`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date:    10:47:58 09/15/2007 
// Design Name: 
// Module Name:    swtichBlock 
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

module pdl_based_switch(i1, i2, select_tp, select_btm, o1, o2);

    input i1, i2;
    input select_tp, select_btm;	 
    output o1, o2;

	(* KEEP_HIERARCHY="TRUE" *) pdl_block pdl_top (.i(i1), .o(o1), .t(select_tp));
	(* KEEP_HIERARCHY="TRUE" *) pdl_block pdl_bottom (.i(i2), .o(o2), .t(select_btm));
//	 end

endmodule