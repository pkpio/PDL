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

#ifndef DEFINESRVSIRCH
#define DEFINESRVSIRCH 1

#include "sirc_server.h"

class SRV_SIRC : public SIRC_SERVER{
public:
	//Constructor for the class
    // registerFile, inputBuffer, and outputBuffer can be null and will be allocated.
    // driverVersion and nicName are optional
	// Check error code with getLastError() to make certain constructor
	// succeeded fully.
	SIRC_DLL_LINKAGE __stdcall SRV_SIRC(uint32_t **registerFile, uint8_t **inputBuffer, uint8_t **outputBuffer,
             uint32_t driverVersion = 0,
             wchar_t *nicName = NULL);
	//Destructor for the class
    __stdcall ~SRV_SIRC();

	//Process all incoming commands until the execute command is received
	BOOL __stdcall processCommands(bool *writeAndExecute);

	//Send the contents of the output buffer back to the host
	BOOL __stdcall sendReadBacks(uint32_t length);

	void __stdcall resetRunRegister(){
		regFileP[255] = 0;
	}

    //Retrieve the active set of parameters and limits for this instance
    BOOL __stdcall getParameters(SIRC_SERVER::PARAMETERS *outParameters, uint32_t maxOutLength);

    //Modify the active set of parameters and limits for this instance
    BOOL __stdcall setParameters(const SIRC_SERVER::PARAMETERS *inParameters, uint32_t inLength);

private:
	uint32_t *regFileP;
	uint8_t *inputBufP;
	uint8_t *outputBufP;

	PACKET_DRIVER *PacketDriver;

	uint8_t WriteAndRunHostMACAddress[6];
    uint8_t My_MACAddress[6];
	
    // Used while composing packets (locals in disguise)
    PACKET *currentPacket;
	uint8_t *currentBuffer;

	std::list <PACKET *>::iterator packetIter;

    //How many can we have anyways?
    uint32_t maxOutstandingReads;
    uint32_t maxOutstandingWrites;
    uint32_t maxInputDataBytes;
    uint32_t maxOutputDataBytes;

	inline BOOL addReceive(PACKET *Packet = NULL);
	inline BOOL addTransmit(PACKET* Packet);

	BOOL processPacket(PACKET* Packet, bool *execute, bool *writeAndExecute);

	BOOL sendErrorMessage(int8_t errorNumber, uint8_t *sourceMessage);

    inline BOOL allocateAndFillPacket(uint8_t *sourceMAC, uint16_t length);
	
	BOOL checkResetPacket(uint8_t *sourceMessage);
	BOOL sendResetAck(uint8_t *sourceMessage);

	BOOL checkRegWritePacket(uint8_t *sourceMessage, bool *execute);
	BOOL sendRegWriteAck(uint8_t *sourceMessage, bool *execute);

	BOOL checkWritePacket(uint8_t *sourceMessage);
	BOOL sendWriteAck(uint8_t *sourceMessage);

	BOOL checkWriteAndRunPacket(uint8_t *sourceMessage, bool *execute, bool *writeAndExecute);

	BOOL checkRegReadPacket(uint8_t *sourceMessage);
	BOOL sendRegReadAck(uint8_t *sourceMessage);

	BOOL checkReadPacket(uint8_t *sourceMessage);
	BOOL sendReadAcks(uint8_t *sourceMessage, uint32_t startAddress, uint32_t readLength);
	BOOL createReadPacketAndTransmit(uint8_t *sourceMessage, uint32_t startAddress, uint32_t readLength);

	BOOL createReadBackPacketAndTransmit(uint32_t startAddress, uint32_t readLength, uint32_t remainingLength);

};

#endif //DEFINESRVSIRCH
