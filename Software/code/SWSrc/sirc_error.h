//----------------------------------------------------------------------------
//
// SIRC error definitions
// 
// Copyright: Microsoft 2011
//
// Author: Ken Eguro
//
// Created: 2/12/11 
//
// Version: 1.00
// 
// 
// Changelog: 
//
//----------------------------------------------------------------------------

#ifndef DEFINESIRCERRORH
#define DEFINESIRCERRORH 1

//******These error codes we expect to be returned from the SIRC client functions from time to time.
//		These occur if the user presents invalid data, if the user's machine is not configured correctly,
//			if there is something wrong with the connection between the PC and the FPGA, or if there is 
//			something wrong with the FPGA itself.

//Conditions that might occur when the constructor is called
//Check to see if the Virtual Machine Network Services, or PCIe, or .. whatever device driver is installed
#define FAILDRIVERPRESENT -1
//Check to see if the Virtual Machine Network Services driver is active on any adapter
#define FAILVMNSDRIVERACTIVE -2
//The Virtual Machine Network Services driver can't seem to be set to the correct MAC filtering
#define FAILVMNSDRIVERFILTER -3
//The Virtual Machine Network Services driver can't seem to get a MAC address
#define FAILVMNSDRIVERMACADD -4
//The Virtual Machine Network Services driver can't seem to get a completion port
#define FAILVMNSDRIVERCOMPLETION -5
//The ETH_SIRC constructor was fed an invalid FPGA target MAC address
#define INVALIDFPGAMACADDRESS -6
//The ETH_SIRC constructor could not contact the target FPGA - check the network cable and target MAC address
#define FAILINITIALCONTACT -7

//Conditions that might occur any time a communication function is called
//We have run out of free memory and an allocation failed
#define FAILMEMALLOC -8
//The last command was called with an invalid buffer
//For sendWriteAndRun it could refer to either inData or outData
#define INVALIDBUFFER -9
//The last command was called with an invalid start/register/SystemACE address
#define INVALIDADDRESS -10
//The last command was called with an invalid length
//For sendWriteAndRun it could refer to either inLength or maxOutLength
#define INVALIDLENGTH -11

//Valid for sendWrite, sendParamRegisterWrite, and sendWriteAndRun
//We didn't seem to get a timely acknowledgment from the write command we sent, even after retries
//If this is returned after the sendWriteAndRun command, the user should 
// re-evaulate the expected runtime of their circuit and possibly increase the timeout.
//If that does not work, there is likely a physical problem.  Make sure that the sendWrite
// function works.
#define FAILWRITEACK -12

//Valid for sendRead, sendParamRegisterRead
//We didn't seem to get a timely acknowledgment from the read command we sent, even after retries
#define FAILREADACK -13

//Valid for sendWriteAndRun
//The output buffer was too small for the amount of results data the circuit
//		produced.
//If this occurs, the returned outputLength will not be the number of bytes returned, but rather the 
//		total number of bytes the execution phase wanted to return (the number of bytes actually returned 
//		will be maxOutLength).  The user can then simply read the “overflow” bytes from addresses 
//		{maxOutLength, outputLength-1} manually with a subsequent sendRead command.
#define FAILWRITEANDRUNCAPACITY -14

//Valid for sendWriteAndRun
//The write and execute phases occured correctly, but we had to retry the readback phase too many times.
//		The returned outputLength will not be the number of bytes returned, but rather the 
//		total number of bytes the execution phase wanted to return.  The state of outData is unknown,
//		but some data has been partially written.
//		In theory, the user could use this information to try a subsequent call to sendRead from {0, outputLength-1}.
//		This option may be attractive if calling sendWriteAndRun is not easy.  For example, 
//		if inData and outData point to overlapping addresses in the same array, it may be simpler to try and 
//		re-read outData rather than recreating inData so that execution can be attempted again.
#define FAILWRITEANDRUNREADACK -15

//Valid for waitDone
//The waitDone request was not acknowledged.  This is different than merely timing out,
// we asked the FPGA about the done signal and got no response.
#define FAILWAITACK -16

//Valid for waitDone
//The waitDone request was acknowledged, but it did not lower within the allotted time.
//Either something is wrong with the circuit on the FPGA or the user should
// re-evaulate the expected runtime of their circuit and possibly increase the timeout.
//If a subseqent call to waitDone doesn't return true, sendReset is always an option.
#define FAILDONE -17

//Valid for sendReset
//The sendReset command was not acknowledged, even after retries
#define FAILRESETACK -18

//Valid for sendConfiguration
//The sendConfiguration command was not acknowledged, even after retries
#define FAILCONFIGURATIONACK -19

//Valid for sendConfiguration
//After sending the sendConfiguration command we cannot re-contact the FPGA
#define FAILCONFIGURATIONRETURN -20

//Valid for sendConfiguration using iMPACT
//The function for programming via iMPACT was called, but one of the constants was not defined
//Check to make sure that IMPACT, PATHTOIMPACT, PATHTOIMPACTTEMPLATEBATCHFILE, PATHTOIMPACTPROGRAMMINGBATCHFILE, 
//		PATHTOIMPACTPROGRAMMINGOUTPUTFILE, and IMPACTSUCCESSPHRASE are defined below.
#define FAILIMPACTCONSTANTDEFINE -21

//Valid for sendConfiguration using iMPACT
//Could not find iMPACT executable
#define FAILPATHTOIMPACT -22

//Valid for sendConfiguration using iMPACT
//Could not find iMPACT batch template command file
#define FAILPATHTOIMPACTTEMPLATEBATCHFILE -23

//Valid for sendConfiguration using iMPACT
//There is not exactly 1 instance of BITSTREAMFILENAME in the batch template command file
#define FAILIMPACTTEMPLATEBATCHFILE -24

//Valid for sendConfiguration using iMPACT
//Could not open iMPACT batch programming command file for writing
#define FAILPATHTOIMPACTPROGRAMMINGBATCHFILE -25

//Valid for sendConfiguration using iMPACT
//Could not open iMPACT output file either for reading or writing
#define FAILPATHTOIMPACTPROGRAMMINGSOUTPUTFILE -26

//Valid for sendConfiguration using iMPACT
//Function was not passed a valid bitstream path
#define FAILCONFIGURATIONBITSTREAM 27

//Valid for sendConfiguration using iMPACT
//iMPACT did not program the FPGA successfully
//Check the impact output file for details.
#define FAILCONFIGURATIONIMPACT -28

//Valid for sendSystemACERegisterRead - currently unused
//The sendSystemACERegisterRead was not acknowledged
#define FAILSYSACEREADACK -29

//Valid for sendSystemACERegisterWrite - currently unused
//The sendSystemACERegisterWrite was not acknowledged
#define FAILSYSACEWRITEACK -30

//******These error codes should not be returned.  If they do, something is wrong in the API code.
//		Please send me mail with details regarding the conditions under which this occurred.
#define FAILVMNSCOMPLETION -100
#define INVALIDWRITETRANSMIT -101
#define INVALIDREADTRANSMIT -102
#define INVALIDPARAMWRITETRANSMIT -103
#define INVALIDPARAMREADTRANSMIT -104
#define FAILREADMISSING -105
#define INVALIDRESETTRANSMIT -106
#define INVALIDSYSACECONFIGTRANSMIT -107
#define INVALIDSYSACEREADTRANSMIT -108
#define INVALIDSYSACEWRITETRANSMIT -109
#define INVALIDWRITEANDRUNTRANSMIT -110
#define INVALIDWRITEANDRUNRECIEVE -111
#define INVALIDERRORTRANSMIT -112

//******These error codes we expect to be returned from the SIRC server to a client, in an error reply packet.
//		These occur if the client presents invalid data, if the user's machine is not configured correctly,
//			if there is something wrong with the connection between the PC and the FPGA, or if there is 
//			something wrong with the FPGA itself.

#define RECEIVE_ERROR_PACKET_LENGTH 0				// This error occurs if we get a packet too short to be a complete command
#define RECEIVE_ERROR_COMMAND 1						// This error occurs if we get an invalid command byte
#define RECEIVE_ERROR_READ_LENGTH 2					// This error occurs when we get a read command, but it's not the correct length packet
#define RECEIVE_ERROR_READ_RUNNING 3				// This error occurs when we get a read command, but the user application is still running
#define RECEIVE_ERROR_WRITE_LENGTH 4				// This error occurs when we get a write command, but it's not the correct length packet
#define RECEIVE_ERROR_WRITE_RUNNING 5				// This error occurs when we get a write command, but the user application is still running
#define RECEIVE_ERROR_WRITE_AND_EXECUTE_RUNNING 6	// This error occurs when we get a write and execute command, but the user application is still running
#define RECEIVE_ERROR_WRITE_AND_EXECUTE_LENGTH 7	// This error occurs when we get a write and execute command, but it's not the correct length packet
#define RECEIVE_ERROR_REG32_READ_LENGTH 8			// This error occurs when we get a reg32 read command, but it's not the correct length packet
#define RECEIVE_ERROR_REG32_READ_RUNNING 9			// This error occurs when we get a reg32 read command, but the user application is still running
#define RECEIVE_ERROR_REG32_WRITE_LENGTH 10			// This error occurs when we get a reg32 write command, but it's not the correct length packet
#define RECEIVE_ERROR_REG32_WRITE_RUNNING 11		// This error occurs when we get a reg32 write command, but the user application is still running
#define RECEIVE_ERROR_SYSACE_CONFIG_LENGTH 12		// This error occurs when we get a SystemACE configure command, but it's not the correct length packet
#define RECEIVE_ERROR_SYSACE_CONFIG_RUNNING 13		// This error occurs when we get a SystemACE configure command, but the user application is still running
#define RECEIVE_ERROR_SYSACE_CONFIG_ADDRESS 14		// This error occurs when we get a SystemACE configure command, but the address is not [0-7]
#define RECEIVE_ERROR_SA_REG_WRITE_LENGTH 15		// This error occurs when we get a SystemACE reg write command, but it's not the correct length packet
#define RECEIVE_ERROR_SA_REG_WRITE_RUNNING 16		// This error occurs when we get a SystemACE reg write command, but the user application is still running
#define RECEIVE_ERROR_SA_REG_WRITE_ADDRESS 17		// This error occurs when we get a SystemACE reg write command, but the address is not [0-47]
#define RECEIVE_ERROR_SA_REG_READ_LENGTH 18			// This error occurs when we get a SystemACE reg read command, but it's not the correct length packet
#define RECEIVE_ERROR_SA_REG_READ_RUNNING 19		// This error occurs when we get a SystemACE reg read command, but the user application is still running
#define RECEIVE_ERROR_SA_REG_READ_ADDRESS 20		// This error occurs when we get a SystemACE reg read command, but the address is not [0-47]
#define RECEIVE_ERROR_RESET_LENGTH 21				// This error occurs when we get a soft reset command, but it's not the correct length packet

#endif //DEFINESIRCERRORH

