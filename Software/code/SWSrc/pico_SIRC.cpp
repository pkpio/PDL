// Title: PICO_SIRC class definition
// 
// Description: 
// Host software to send and receive packets over Pico channel
// Read and write to input/output buffers and parameter register file.  Also, 
// start execution, wait until execution is completed and (maybe) reconfigure the
// device.
//
// Copyright: Microsoft 2011
//
// Author: Ken Eguro
//
// Created: 10/23/10 
//
// Version: 1.00
// 
// 
// Changelog: 
//
//----------------------------------------------------------------------------

#include "sirc_internal.h"

#include "picoDrv.h"
#include "pico_channel.h"

//Memory offset of the input side of the memory (it's all one memory, but the hardware writes to different locations)
#define INPUT_OFFSET 0x00000000

//Memory offset of the output side of the memory (it's all one memory, but the hardware writes to different locations)
#define OUTPUT_OFFSET 0x08000000

//Memory offset of parameter register file
#define PARAMETER_REG_OFFSET 0xF0000000

//Define which Pico channel we will be using
#define CHANNEL_NO 10


SIRC_DLL_LINKAGE PICO_SIRC::PICO_SIRC(int desiredInstance, const char *configurationBitFile)
{
    if (desiredInstance < 0)
        desiredInstance = CHANNEL_NO;
    channel = new cPicoChannel_Exx(desiredInstance,NULL,configurationBitFile);
    if ((erC = channel->GetError(errorString,sizeof errorString)) < 0) {
        PrintError("cPicoChannel_Exx", errorString);
        setLastError( FAILDRIVERPRESENT);
    }
}

PICO_SIRC::~PICO_SIRC()
{
    delete channel;
}

//Dynamic parameters
//Retrieve the active set of parameters and limits for this instance
BOOL PICO_SIRC::getParameters(SIRC::PARAMETERS *outParameters, uint32_t maxOutLength)
{
    SIRC::PARAMETERS params;

    params.myVersion            = SIRC_PARAMETERS_CURRENT_VERSION;
    params.maxInputDataBytes    = 0; // unlimited(?)
    params.maxOutputDataBytes   = 0;
    params.writeTimeout         = 0;
    params.readTimeout          = 0;
    params.maxRetries           = 0;
    params.maxOutstandingReads  = 0;
    params.maxOutstandingWrites = 0;

    if (maxOutLength >= sizeof(*outParameters)) {
        *outParameters = params;
        setLastError(0);
        return true;
    }
    //Wants to know version or partial (or error)
    memcpy(outParameters,&params,maxOutLength);
    setLastError(INVALIDLENGTH);
    return false;
}

//Modify the active set of parameters and limits for this instance
BOOL PICO_SIRC::setParameters(const SIRC::PARAMETERS *inParameters, uint32_t length)
{
    //Sometimes you got to know what you are doing.
    if ((length < sizeof(*inParameters)) ||
        (inParameters->myVersion < SIRC_PARAMETERS_CURRENT_VERSION)){
        setLastError(INVALIDLENGTH);
        return false;
    }

    //Ignored: maxOutstandingReads  = inParameters->maxOutstandingReads;
    //Ignored: maxOutstandingWrites  = inParameters->maxOutstandingWrites;
    //Ignored: maxInputDataBytes    = inParameters->maxInputDataBytes;
    //Ignored: maxOutputDataBytes   = inParameters->maxOutputDataBytes;
    //Ignored: writeTimeout         = inParameters->writeTimeout;
    //Ignored: readTimeout          = inParameters->readTimeout;
    //Ignored: maxRetries           = inParameters->maxRetries;

    setLastError(0);
    return true;
}


void PICO_SIRC::PrintError(char *pszRoutineName, char *pszComment)
{
#ifdef BIGDEBUG
	LPTSTR lpMessage;

	// Convert the error code into a string
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		0,					// Default language
		(LPTSTR)&lpMessage,
		0,
		NULL
	);

	printf( "PICO_SIRC->%s() Error %s: 0x%X - %S\n", pszRoutineName, pszComment, GetLastError(), lpMessage );
	LocalFree( lpMessage );
#endif
}

BOOL PICO_SIRC::sendRead(uint32_t startAddress, uint32_t length, uint8_t *readBackData)
{
    int nRead = channel->Read(readBackData, (int) length, (int) startAddress);
    // NB: Possibly might return less than requested?
    if (nRead <= 0) {
        erC = channel->GetError(errorString,sizeof errorString);
        PrintError( "Read", errorString );
        setLastError( INVALIDREADTRANSMIT);
        return false;
    }
    return true;
}

BOOL PICO_SIRC::sendWrite(uint32_t startAddress, uint32_t length, uint8_t *buffer)
{
    int nWritten = channel->Write(buffer, (int) length, (int) startAddress);
    // NB: Possibly might return less than requested?
    if (nWritten <= 0) {
        erC = channel->GetError(errorString,sizeof errorString);
        PrintError( "Write", errorString );
        setLastError( INVALIDWRITETRANSMIT);
        return false;
    }
    return true;
}

BOOL PICO_SIRC::sendParamRegisterRead(uint8_t regNumber, uint32_t *value)
{
    // BUGBUG I dont think this is right
    int r = channel->ReadStatus(regNumber,value);
    if (r != 0) {
        erC = channel->GetError(errorString,sizeof errorString);
        PrintError( "ParamRead", errorString );
        setLastError( INVALIDPARAMREADTRANSMIT);
        return false;
    }
    return true;
}

BOOL PICO_SIRC::sendParamRegisterWrite(uint8_t regNumber, uint32_t value)
{
    // BUGBUG I dont think this is right
    int r = channel->WriteControl(regNumber,value);
    if (r != 0) {
        erC = channel->GetError(errorString,sizeof errorString);
        PrintError( "ParamWrite", errorString );
        setLastError( INVALIDPARAMWRITETRANSMIT);
        return false;
    }
    return true;
}

BOOL PICO_SIRC::sendRun()
{
	//printf("Sending Run\n");
    // BUGBUG I dont think this is right
	return (sendParamRegisterWrite(255, 1));
}

BOOL PICO_SIRC::waitDone(uint32_t maxWaitTimeInMsec)
{
	unsigned int value;

	//printf("Waiting for Done\n");

	uint32_t currTime = GetTickCount();
	uint32_t endTime = currTime + maxWaitTimeInMsec;

	setLastError( 0);

	//Wait for the system to finish execution
    do {
        // BUGBUG I dont think this is right
		sendParamRegisterRead(255, &value);
		if(value == 0){
			return true;
		}
		currTime = GetTickCount();
    } while(endTime > currTime);

    setLastError( FAILDONE);
	return false;
}

//Send a soft reset to the user circuit (useful when debugging new applications
//	and the circuit refuses to give back control to the host PC)
//Returns true if the soft reset is accepted
//If the reset command is refused for any reason, returns false.
// Check error code with getLastError()
BOOL PICO_SIRC::sendReset()
{
    // BUGBUG not implemented
    setLastError( FAILRESETACK);
    return false;
}


//Send a block of data to the FPGA, raise the execution signal, wait for the execution
// signal to be lowered, then read back up to values of results
// startAddress: local address on FPGA input buffer to begin writing at
// length: # of bytes to write
// inData: data to be sent to FPGA
// maxWaitTime: # of seconds to wait until execution timeout
// outData: readback data buffer (if function returns successfully)
// maxOutLength: maximum length of outData buffer provided
// outputLength: number of bytes actually returned (if function returns successfully)
// Returns true if entire process is successful.
// If function fails for any reason, returns false.
//  Check error code with getLastError().
//  error == FAILCAPACITY: The output was larger than provided buffer.  Rather than the number of
//			bytes actually returned, the outputLength variable will contain the TOTAL number bytes the
//			function wanted to return (the number of bytes actually returned will be maxOutLength).
//			If this occurs, user should read back bytes {maxOutLength, outputLength - 1} manually
//			with a subsequent sendRead command.
//  error == FAILREADACK: The write and execution phases completed correctly, but we retried
//			the readback phase too many times.  In this case, like the FAILCAPICITY error, outputLength
//			will contain the TOTAL number bytes the	function wanted to return.  The state of outData is unknown,
//			but some data has been partially written.  The user could try calling sendRead
//			from {0, outputLength-1} manually if re-calling sendWriteAndRun is not easy
//			(for example, if inData and outData overlapped).
//  error == anything else: see normal error list
BOOL PICO_SIRC::sendWriteAndRun(uint32_t startAddress, uint32_t inLength, uint8_t *inData, 
							  uint32_t maxWaitTimeInMsec, uint8_t *outData, uint32_t maxOutLength, 
							  uint32_t *outputLength)
{
	setLastError( 0);

	//Check the input parameters
	if(!inData){
		setLastError( INVALIDBUFFER);
		return false;
	}
	if(startAddress > OUTPUT_OFFSET){
		setLastError( INVALIDADDRESS);
		return false;
	}
	if(inLength == 0 || startAddress + inLength > OUTPUT_OFFSET){
		setLastError( INVALIDLENGTH);
		return false;
	}

	//Check the output parameters
	if(!outData){
		setLastError( INVALIDBUFFER);
		return false;
	}
	if(maxOutLength == 0 || maxOutLength > OUTPUT_OFFSET){
		setLastError( INVALIDLENGTH);
		return false;
	}

	//Send the data to the FPGA
    if (!sendWrite(startAddress,inLength,inData)){
        return false;
    }

    //Send the run cmd
    if (!sendRun()){
        return false;
    }

    //Wait till done
    if (!waitDone( maxWaitTimeInMsec)){
        return false;
    }

    //Read back data
    //BUGBUG what about partial results??
    if (!sendRead(0,maxOutLength,outData)){
        return false;
    }
    *outputLength = maxOutLength;

    //and done
    return true;
}

