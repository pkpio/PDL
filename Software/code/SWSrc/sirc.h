// Title: SIRC base class definition
// 
// Description: Read and write to input/output buffers and parameter register file,
// start execution, wait until execution is completed.
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

#ifndef DEFINESIRCH
#define DEFINESIRCH 1

#ifndef SIRC_DLL_LINKAGE
#define SIRC_DLL_LINKAGE /* Auto-selected based on .lib file chosen */
#endif

#ifndef _PRECISE_TYPES_ALREADY_DEFINED
#define _PRECISE_TYPES_ALREADY_DEFINED 1
//unsigned byte
typedef unsigned char uint8_t;

//signed byte
typedef signed char int8_t;

//16-bit word
typedef unsigned short uint16_t;

//32-bit word
typedef unsigned int uint32_t;
#endif

class SIRC
{
public:
	//Constructor for the base class
    __stdcall SIRC(void)
    {
        lastError = 0;
    }

	//No Destructor for the base class
	virtual __stdcall ~SIRC(){}

	//Send a block of data to an input buffer on the FPGA
	// startAddress: local address on FPGA input buffer to begin writing at
	// length: # of bytes to write
	// buffer: data to be sent to FPGA
	//Returns true if write is successful.
	//If write fails for any reason, returns false.
	// Check error code with getLastError().
	virtual BOOL __stdcall sendWrite(uint32_t startAddress, uint32_t length, uint8_t *buffer) = 0;

	//Read a block of data from the output buffer of the FPGA
	// startAddress: local address on FPGA output buffer to begin reading from
	// length: # of bytes to read
	// buffer: data received from FPGA  
	//Returns true if read is successful.
	//If read fails for any reason, returns false.
	// Check error code with getLastError().
	virtual BOOL __stdcall sendRead(uint32_t startAddress, uint32_t length, uint8_t *buffer) = 0;

	//Send a 32-bit value from the PC to the parameter register file on the FPGA
	// regNumber: register to which value should be sent (between 0 and 254)
	// value: value to be written
	//Returns true if write is successful.
	//If write fails for any reason, returns false.
	// Check error code with getLastError()
	virtual BOOL __stdcall sendParamRegisterWrite(uint8_t regNumber, uint32_t value) = 0;

	//Read a 32-bit value from the parameter register file on the FPGA back to the PC
	// regNumber: register to which value should be read (between 0 and 254)
	// value: value received from FPGA
	//Returns true if read is successful.
	//If read fails for any reason, returns false.
	// Check error code with getLastError().
	virtual BOOL __stdcall sendParamRegisterRead(uint8_t regNumber, uint32_t *value) = 0;

	//Raise execution signal on FPGA
	//Returns true if signal is raised.
	//If signal is not raised for any reason, returns false.
	// Check error code with getLastError()
	virtual BOOL __stdcall sendRun() = 0;

	//Wait until execution signal on FPGA is lowered
	// maxWaitTimeInMsec: # of milliseconds to wait until timeout (from 1 to 4M sec).
	//Returns true if signal is lowered.
	//If function fails for any reason, returns false.
	// Check error code with getLastError().
	virtual BOOL __stdcall waitDone(uint32_t maxWaitTimeInMsec) = 0;

	//Send a soft reset to the user circuit (useful when debugging new applications
	//	and the circuit refuses to give back control to the host PC)
	//Returns true if the soft reset is accepted
	//If the reset command is refused for any reason, returns false.
	// Check error code with getLastError()
	virtual BOOL __stdcall sendReset() = 0;

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
	virtual BOOL __stdcall sendWriteAndRun(uint32_t startAddress, uint32_t inLength, uint8_t *inData, 
		uint32_t maxWaitTimeinMsec, uint8_t *outData, uint32_t maxOutLength, 
		uint32_t *outputLength) = 0;

    //Dynamically adjustable parameters and limits
    typedef struct {
        uint32_t myVersion;
#define SIRC_PARAMETERS_CURRENT_VERSION 1
        uint32_t maxInputDataBytes;         //Should match hw-side buffer
        uint32_t maxOutputDataBytes;        //Should match hw-side buffer
        uint32_t writeTimeout;              //..before we give up
        uint32_t readTimeout;               //..before we give up
        uint32_t maxRetries;                //..before we give up
        uint32_t maxOutstandingReads;       //NB: In some cases these two can only be lowered.
        uint32_t maxOutstandingWrites;      //NB2: 0 means unlimited.
    } PARAMETERS;

    //Retrieve the active set of parameters and limits for this instance
    virtual BOOL __stdcall getParameters(SIRC::PARAMETERS *outParameters, uint32_t maxOutLength) = 0;

    //Modify the active set of parameters and limits for this instance
    virtual BOOL __stdcall setParameters(const SIRC::PARAMETERS *inParameters, uint32_t length) = 0;

	//Retrieve the last error code.  Any value < 0 indicates a problem.
	// A value === 0 indicates no error.
	// See function prototype description above for further explanation.
	inline int8_t __stdcall getLastError(){
		return(lastError);
	}

	//Set the last error code.  Any value < 0 indicates a problem.
	inline void __stdcall setLastError(int8_t code){
		lastError = code;
	}

private:
	int8_t lastError;
};

//Open the first valid SIRC interface
extern SIRC_DLL_LINKAGE SIRC * __stdcall openSirc(uint8_t *FPGA_ID, uint32_t driverVersion);

#endif //DEFINESIRCH
