// Title: ETH_SIRC class definition
// 
// Description: Read and write to input/output buffers and parameter register file.  Also, 
// start execution, wait until execution is completed and (maybe) reconfigure the device.
//
// Based on ENIC code from MSR Giano
//
// Copyright: Microsoft 2009
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

#ifndef DEFINEETHSIRCH
#define DEFINEETHSIRCH 1

#include "sirc.h"

//#define DEBUG
//#define BIGDEBUG

class ETH_SIRC : public SIRC {
public:
	//Constructor for the class
	//FPGA_ID: 6 byte array containing the MAC adddress of the destination FPGA
	//	This should be arranged big-endian (MSB of MAC address is array[0]).
	//	It has been done this way since most people like to read left to right {MSB, .. , LSB}
	//Check error code with getLastError() to make certain constructor
	// succeeded fully.
	SIRC_DLL_LINKAGE __stdcall ETH_SIRC(uint8_t *FPGA_ID, uint32_t driverVersion, wchar_t *nicName);

	//Destructor for the class
    __stdcall ~ETH_SIRC();

	//Wait until execution signal on FPGA is lowered
	// maxWaitTimeInMsec: # of milliseconds to wait until timeout (from 1 to 4M sec).
	//Returns true if signal is lowered.
	//If function fails for any reason, returns false.
	// Check error code with getLastError().
	BOOL __stdcall waitDone(uint32_t maxWaitTimeInMsec);

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
	BOOL __stdcall sendRun(void);

	//Send a soft reset to the user circuit (useful when debugging new applications
	//	and the circuit refuses to give back control to the host PC)
	//Returns true if the soft reset is accepted
	//If the reset command is refused for any reason, returns false.
	// Check error code with getLastError()
	BOOL __stdcall sendReset(void);

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
	PACKET_DRIVER *PacketDriver;
    struct {
        uint8_t FPGA_MACAddress[6];
        uint8_t My_MACAddress[6];
    } ethHeader;
	
	std::list <PACKET *> outstandingPackets;
	//How many outstanding packets do we have?
	//We could just do outstandingPackets.size(), but that might be slow
	int outstandingTransmits;
	std::list <PACKET *>::iterator packetIter;

    //How many can we have anyways?
    uint32_t maxOutstandingReads;
    uint32_t maxOutstandingWrites;
    uint32_t writeTimeout;
    uint32_t readTimeout;
    uint32_t maxRetries;
    uint32_t maxInputDataBytes;
    uint32_t maxOutputDataBytes;

	//These are the parameters used when we are doing reads.
	std::list <uint32_t> outstandingReadStartAddresses;
	std::list <uint32_t> outstandingReadLengths;

	std::list <uint32_t>::iterator startAddressIter;
	std::list <uint32_t>::iterator lengthIter;

	//Over the current set of read requests, have we seen the
	// need for any resends?
	BOOL noResends;

	// Have we seen any response from the write & run command?
	BOOL noResponse;

	// Have we had a capacity problem for the response to this write & run command?
	BOOL okCapacity;

    // Used while composing packets (locals in disguise)
    PACKET *currentPacket;
	uint8_t *currentBuffer;

#ifdef DEBUG
	int writeResends;
	int readResends;
	int paramWriteResends;
	int paramReadResends;
	int resetResends;
	int writeAndRunResends;
#endif

    inline BOOL allocateAndFillPacket(uint16_t length);
    inline void setLengthAndAddress(uint32_t length, uint32_t address);
    inline void setValueField(uint32_t value);
    inline BOOL sendCurrentPacket(int8_t errorCode, BOOL flushOutstanding, char *packetName = NULL);

	inline BOOL addReceive(PACKET *Packet = NULL);
	inline BOOL addTransmit(PACKET* Packet);
	void emptyOutstandingPackets(void);
    BOOL bailOut(int8_t errorCode);

    BOOL receiveGenericAck(uint32_t timeOut, uint32_t *arg2, BOOL (ETH_SIRC::*checkFunction)(PACKET*,uint32_t *),int errorCode);
    BOOL checkSimpleResponse(PACKET *packet, uint8_t commandCode, uint8_t length);
    BOOL checkResponseWithValue(PACKET *packet, uint32_t *value, uint8_t commandCode);
    BOOL ETH_SIRC::resendOutstandingPackets(int errorCode, char *callerName = NULL, int *counter = NULL);


	BOOL createWriteRequestBackAndTransmit(uint32_t startAddress, uint32_t length, uint8_t *buffer, BOOL flushQueue);
	BOOL receiveWriteAcks(void);
	BOOL checkWriteAck(PACKET* packet);

	BOOL createReadRequestBackAndTransmit(uint32_t startAddress, uint32_t length);
	BOOL createReadRequestCurrentIterLocation(uint32_t startAddress, uint32_t length);
	BOOL receiveReadResponses(uint32_t initialStartAddress, uint8_t *buffer);
	BOOL checkReadData(PACKET* packet, uint32_t* currAddress, uint32_t* currLength,  
		uint8_t* buffer, uint32_t initialStartAddress);

	BOOL createParamWriteRequestBackAndTransmit(uint8_t regNumber, uint32_t value);
	inline BOOL receiveParamWriteAck(void)
    {
        return receiveGenericAck(writeTimeout,NULL,&ETH_SIRC::checkParamWriteAck, FAILWRITEACK);
    }
	BOOL checkParamWriteAck(PACKET* packet, uint32_t *unused);

	BOOL createParamReadRequestBackAndTransmit(uint8_t regNumber);
	inline BOOL receiveParamReadResponse(uint32_t *value, uint32_t maxWaitTimeInMsec)
    {
        return receiveGenericAck(maxWaitTimeInMsec,value,&ETH_SIRC::checkParamReadData, FAILREADACK);
    }
	BOOL checkParamReadData(PACKET* packet, uint32_t *value);

	BOOL createWriteAndRunRequestBackAndTransmit(uint32_t startAddress, uint32_t length, uint8_t *buffer);
	BOOL receiveWriteAndRunAcks(uint32_t maxWaitTimeInMsec, uint32_t maxOutLength, uint8_t *buffer, uint32_t *outputLength);
	BOOL checkWriteAndRunData(PACKET* packet, uint32_t* currAddress, uint32_t* currLength,  
									uint8_t* buffer, uint32_t *outputLength, uint32_t maxOutLength);

	BOOL createResetRequestAndTransmit(void);
	inline BOOL receiveResetAck(void)
    {
        return receiveGenericAck(writeTimeout,NULL,&ETH_SIRC::checkResetAck, FAILRESETACK);
    }
	BOOL checkResetAck(PACKET* packet, uint32_t *unused);

	void incrementCurrIterLocation(void);
	void removeReadRequestCurrentIterLocation(void);
	inline void markPacketAcked(PACKET* packet);

	void printPacket(PACKET* packet);

};

#endif //DEFINEETHSIRCH
