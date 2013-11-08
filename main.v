`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company:
// Engineer: Praveen Kumar Pendyala
//
// Create Date:    19:14:05 11/07/2013
// Design Name:
// Module Name:    main
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
module main(
    input In,
    output Out
    );

	 reg [124:0] C = 124'hAAAAAAAA;

	 PDL pdl_1(
		 .I(In),		//Input signal to the PDL module
		 .C(C),			//Control bits for each LUT in the PDL
		 .O(Out)		//Output of the PDL line
	 );


endmodule
