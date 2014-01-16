// Title: PCIE_SIRC class definition
// 
// Description: Read and write to input/output buffers and parameter register file.  Also, 
// start execution, wait until execution is completed and (maybe) reconfigure the
// device.
//
// Based on ENIC code from MSR Giano
//
// Copyright: Microsoft 2011
//
// Author: Ken Eguro
//
// Created: 10/23/09 
//
// Version: 1.00
// 
// 
// Changelog: 
//
//----------------------------------------------------------------------------

#ifndef DEFINEPCIESIRCH
#define DEFINEPCIESIRCH 1

#include "sirc.h"

class PCIE_SIRC : public SIRC {
public:
	//Constructor for the class
	//Check error code with getLastError() to make certain constructor
	// succeeded fully.
	SIRC_DLL_LINKAGE __stdcall PCIE_SIRC(int desiredInstance = -1);

	//Destructor for the class
    __stdcall ~PCIE_SIRC();

	//Send a block of data to an input buffer on the FPGA
	// startAddress: local address on FPGA input buffer to begin writing at
	// length: # of bytes to write
	// buffer: data to be sent to FPGA
	//Returns true if write is successful.
	//If write fails for any reason, returns false.
	// Check error code with getLastError().
	BOOL __stdcall sendWrite(uint32_t startAddress, uint32_t length, uint8_t *buffer);

	//Read a block of data from the output buffer of the FPGA
	// startAddress: local address on FPGA output buffer to begin reading from
	// length: # of bytes to read
	// buffer: data received from FPGA  
	//Returns true if read is successful.
	//If read fails for any reason, returns false.
	// Check error code with getLastError().
	BOOL __stdcall sendRead(uint32_t startAddress, uint32_t length, uint8_t *buffer);

	//Send a 32-bit value from the PC to the parameter register file on the FPGA
	// regNumber: register to which value should be sent (between 0 and 254)
	// value: value to be written
	//Returns true if write is successful.
	//If write fails for any reason, returns false.
	// Check error code with getLastError()
	BOOL __stdcall sendParamRegisterWrite(uint8_t regNumber, uint32_t value);

	//Read a 32-bit value from the parameter register file on the FPGA back to the PC
	// regNumber: register to which value should be read (between 0 and 254)
	// value: value received from FPGA
	//Returns true if read is successful.
	//If read fails for any reason, returns false.
	// Check error code with getLastError().
	BOOL __stdcall sendParamRegisterRead(uint8_t regNumber, uint32_t *value);

	//Raise execution signal on FPGA
	//Returns true if signal is raised.
	//If signal is not raised for any reason, returns false.
	// Check error code with getLastError()
	BOOL __stdcall sendRun();

	//Wait until execution signal on FPGA is lowered
	// maxWaitTimeInMsec: # of milliseconds to wait until timeout (from 1 to 4M sec).
	//Returns true if signal is lowered.
	//If function fails for any reason, returns false.
	// Check error code with getLastError().
	BOOL __stdcall waitDone(uint32_t maxWaitTimeInMsec);

	//Send a soft reset to the user circuit (useful when debugging new applications
	//	and the circuit refuses to give back control to the host PC)
	//Returns true if the soft reset is accepted
	//If the reset command is refused for any reason, returns false.
	// Check error code with getLastError()
	BOOL __stdcall sendReset();

	//Send a block of data to the FPGA, raise the execution signal, wait for the execution
	// signal to be lowered, then read back up to N values of results
	// startAddress: local address on FPGA input buffer to begin writing at
	// length: # of bytes to write
	// inData: data to be sent to FPGA
	// maxWaitTimeInMsec: # of milliseconds to wait until execution timeout
	// outData: readback data buffer (if function returns successfully)
	// maxOutLength: maximum length of outData buffer provided
	// outputLength: number of bytes actually returned (if function returns successfully)
	// Returns true if entire process is successful.
	// If function fails for any reason, returns false.
	//  Check error code with getLastError().
	//  error != FAILCAPACITY: see normal error list
	//  error == FAILCAPACITY: Output was larger than provided buffer.  Rather than the number of
	//			bytes actually returned, the outputLength variable will contain the TOTAL number bytes the
	//			function wanted to return (he number of bytes actually returned will be maxOutLength).
	//			If this occurs, user should read back bytes {maxOutLength, outputLength - 1} manually
	//			with a subsequent sendRead command.
	BOOL __stdcall sendWriteAndRun(uint32_t startAddress, uint32_t inLength, uint8_t *inData, 
		uint32_t maxWaitTimeinMsec, uint8_t *outData, uint32_t maxOutLength, 
		uint32_t *outputLength);

    //Retrieve the active set of parameters and limits for this instance
    BOOL __stdcall getParameters(SIRC::PARAMETERS *outParameters, uint32_t maxOutLength);

    //Modify the active set of parameters and limits for this instance
    BOOL __stdcall setParameters(const SIRC::PARAMETERS *inParameters, uint32_t length);

private:
	HANDLE hFile;
	uint8_t *wholeWordBuffer;
    uint32_t maxInputDataBytes;
    uint32_t maxOutputDataBytes;

	void PrintError(char *pszRoutineName, char *pszComment);

	bool FindDevice(int instance);
};

#endif //DEFINEPCIESIRCH
