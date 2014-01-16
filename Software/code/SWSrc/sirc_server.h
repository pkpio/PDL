//----------------------------------------------------------------------------
//
// ETH_SOFT class definition
// 
// Accept read and write and execute commands from the ETH_SIRC class
//
// Based on ENIC code from MSR Giano
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

#ifndef DEFINESIRCSERVERH
#define DEFINESIRCSERVERH 1

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

class SIRC_SERVER {
public:
	//Constructor for the base class
	// Check error code with getLastError() to make certain constructor
	// succeeded fully.
    __stdcall SIRC_SERVER(void)
    {
        lastError = 0;
    }

	//No Destructor for the base class
	virtual __stdcall ~SIRC_SERVER(){};

	//Process all incoming commands until the execute command is received
	virtual BOOL __stdcall processCommands(bool *writeAndExecute) = 0;

	//Send the contents of the output buffer back to the host
	virtual BOOL __stdcall sendReadBacks(uint32_t length) = 0;

	virtual void __stdcall resetRunRegister(void) = 0;

    //Dynamically adjustable parameters and limits
    typedef struct {
        uint32_t myVersion;
#define SIRC_PARAMETERS_CURRENT_VERSION 1
        uint32_t maxInputDataBytes;
        uint32_t maxOutputDataBytes;
        uint32_t maxOutstandingReads;       //NB: In some cases these two can only be lowered.
        uint32_t maxOutstandingWrites;      //NB2: 0 means unlimited.
    } PARAMETERS;

    //Retrieve the active set of parameters and limits for this instance
    virtual BOOL __stdcall getParameters(SIRC_SERVER::PARAMETERS *outParameters, uint32_t maxOutLength) = 0;

    //Modify the active set of parameters and limits for this instance
    virtual BOOL __stdcall setParameters(const SIRC_SERVER::PARAMETERS *inParameters, uint32_t inLength) = 0;

	//Retrieve the last error code.  Any value < 0 indicates a problem.
	//A value === 0 indicates no error.
	//See function prototype description above for further explaination.
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

extern SIRC_DLL_LINKAGE SIRC_SERVER * __stdcall openSircServer(
    uint32_t **registerFile,
    uint8_t **inputBuffer,
    uint8_t **outputBuffer,
    uint32_t driverVersion = 0,
    wchar_t *nicName = NULL);

#endif //DEFINESIRCSERVERH
