`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company:
// Engineer: Praveen Kumar Pendyala
//
// Create Date:    15:43:15 11/07/2013
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
module PDL(
	input 			I,		//Input signal to the PDL module
    input [124:0]	C,		//Configuration bits
    output 			O		//Output of the PDL line
    );

	 (* KEEP = "TRUE" *) wire [23:0] Olut; //Output of LUTs

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_0 (
		.O(Olut[0]), // LUT general output
		.I0(I), // LUT input
		.I1(C[0]), // LUT input
		.I2(C[1]), // LUT input
		.I3(C[2]), // LUT input
		.I4(C[3]), // LUT input
		.I5(C[4]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_1 (
		.O(Olut[1]), // LUT general output
		.I0(Olut[0]), // LUT input
		.I1(C[5]), // LUT input
		.I2(C[6]), // LUT input
		.I3(C[7]), // LUT input
		.I4(C[8]), // LUT input
		.I5(C[9]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_2 (
		.O(Olut[2]), // LUT general output
		.I0(Olut[1]), // LUT input
		.I1(C[10]), // LUT input
		.I2(C[11]), // LUT input
		.I3(C[12]), // LUT input
		.I4(C[13]), // LUT input
		.I5(C[14]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_3 (
		.O(Olut[3]), // LUT general output
		.I0(Olut[2]), // LUT input
		.I1(C[15]), // LUT input
		.I2(C[16]), // LUT input
		.I3(C[17]), // LUT input
		.I4(C[18]), // LUT input
		.I5(C[19]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_4 (
		.O(Olut[4]), // LUT general output
		.I0(Olut[3]), // LUT input
		.I1(C[20]), // LUT input
		.I2(C[21]), // LUT input
		.I3(C[22]), // LUT input
		.I4(C[23]), // LUT input
		.I5(C[24]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_5 (
		.O(Olut[5]), // LUT general output
		.I0(Olut[4]), // LUT input
		.I1(C[25]), // LUT input
		.I2(C[26]), // LUT input
		.I3(C[27]), // LUT input
		.I4(C[28]), // LUT input
		.I5(C[29]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_6 (
		.O(Olut[6]), // LUT general output
		.I0(Olut[5]), // LUT input
		.I1(C[30]), // LUT input
		.I2(C[31]), // LUT input
		.I3(C[32]), // LUT input
		.I4(C[33]), // LUT input
		.I5(C[34]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_7 (
		.O(Olut[7]), // LUT general output
		.I0(Olut[6]), // LUT input
		.I1(C[35]), // LUT input
		.I2(C[36]), // LUT input
		.I3(C[37]), // LUT input
		.I4(C[38]), // LUT input
		.I5(C[39]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_8 (
		.O(Olut[8]), // LUT general output
		.I0(Olut[7]), // LUT input
		.I1(C[40]), // LUT input
		.I2(C[41]), // LUT input
		.I3(C[42]), // LUT input
		.I4(C[43]), // LUT input
		.I5(C[44]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_9 (
		.O(Olut[9]), // LUT general output
		.I0(Olut[8]), // LUT input
		.I1(C[45]), // LUT input
		.I2(C[46]), // LUT input
		.I3(C[47]), // LUT input
		.I4(C[48]), // LUT input
		.I5(C[49]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_10 (
		.O(Olut[10]), // LUT general output
		.I0(Olut[9]), // LUT input
		.I1(C[50]), // LUT input
		.I2(C[51]), // LUT input
		.I3(C[52]), // LUT input
		.I4(C[53]), // LUT input
		.I5(C[54]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_11 (
		.O(Olut[11]), // LUT general output
		.I0(Olut[10]), // LUT input
		.I1(C[55]), // LUT input
		.I2(C[56]), // LUT input
		.I3(C[57]), // LUT input
		.I4(C[58]), // LUT input
		.I5(C[59]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_12 (
		.O(Olut[12]), // LUT general output
		.I0(Olut[11]), // LUT input
		.I1(C[60]), // LUT input
		.I2(C[61]), // LUT input
		.I3(C[62]), // LUT input
		.I4(C[63]), // LUT input
		.I5(C[64]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_13 (
		.O(Olut[13]), // LUT general output
		.I0(Olut[12]), // LUT input
		.I1(C[65]), // LUT input
		.I2(C[66]), // LUT input
		.I3(C[67]), // LUT input
		.I4(C[68]), // LUT input
		.I5(C[69]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_14 (
		.O(Olut[14]), // LUT general output
		.I0(Olut[13]), // LUT input
		.I1(C[70]), // LUT input
		.I2(C[71]), // LUT input
		.I3(C[72]), // LUT input
		.I4(C[73]), // LUT input
		.I5(C[74]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_15 (
		.O(Olut[15]), // LUT general output
		.I0(Olut[14]), // LUT input
		.I1(C[75]), // LUT input
		.I2(C[76]), // LUT input
		.I3(C[77]), // LUT input
		.I4(C[78]), // LUT input
		.I5(C[79]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_16 (
		.O(Olut[16]), // LUT general output
		.I0(Olut[15]), // LUT input
		.I1(C[80]), // LUT input
		.I2(C[81]), // LUT input
		.I3(C[82]), // LUT input
		.I4(C[83]), // LUT input
		.I5(C[84]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_17 (
		.O(Olut[17]), // LUT general output
		.I0(Olut[16]), // LUT input
		.I1(C[85]), // LUT input
		.I2(C[86]), // LUT input
		.I3(C[87]), // LUT input
		.I4(C[88]), // LUT input
		.I5(C[89]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_18 (
		.O(Olut[18]), // LUT general output
		.I0(Olut[17]), // LUT input
		.I1(C[90]), // LUT input
		.I2(C[91]), // LUT input
		.I3(C[92]), // LUT input
		.I4(C[93]), // LUT input
		.I5(C[94]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_19 (
		.O(Olut[19]), // LUT general output
		.I0(Olut[18]), // LUT input
		.I1(C[95]), // LUT input
		.I2(C[96]), // LUT input
		.I3(C[97]), // LUT input
		.I4(C[98]), // LUT input
		.I5(C[99]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_20 (
		.O(Olut[20]), // LUT general output
		.I0(Olut[19]), // LUT input
		.I1(C[100]), // LUT input
		.I2(C[101]), // LUT input
		.I3(C[102]), // LUT input
		.I4(C[103]), // LUT input
		.I5(C[104]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_21 (
		.O(Olut[21]), // LUT general output
		.I0(Olut[20]), // LUT input
		.I1(C[105]), // LUT input
		.I2(C[106]), // LUT input
		.I3(C[107]), // LUT input
		.I4(C[108]), // LUT input
		.I5(C[109]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_22 (
		.O(Olut[22]), // LUT general output
		.I0(Olut[21]), // LUT input
		.I1(C[110]), // LUT input
		.I2(C[111]), // LUT input
		.I3(C[112]), // LUT input
		.I4(C[113]), // LUT input
		.I5(C[114]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_23 (
		.O(Olut[23]), // LUT general output
		.I0(Olut[22]), // LUT input
		.I1(C[115]), // LUT input
		.I2(C[116]), // LUT input
		.I3(C[117]), // LUT input
		.I4(C[118]), // LUT input
		.I5(C[119]) // LUT input
	);

	 LUT6 #(
		.INIT(64'hAAAAAAAAAAAAAAAA) // Specify LUT Contents
	) LUT6_inst_24 (
		.O(O), // LUT general output
		.I0(Olut[23]), // LUT input
		.I1(C[120]), // LUT input
		.I2(C[121]), // LUT input
		.I3(C[122]), // LUT input
		.I4(C[123]), // LUT input
		.I5(C[124]) // LUT input
	);


endmodule
