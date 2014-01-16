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

#include "sirc_internal.h"

//******
//******The following default values can be changed using the get/setParameter methods.
//******

//******The following constants must match the setup of the hardware-side we emulate.
//How many bytes are in the input data buffer?
//The default value is 128KB
#define MAXINPUTDATABYTEADDRESS (1024 * 128)

//How many bytes are in the output data buffer?
//We are using an 8KB output buffer
#define MAXOUTPUTDATABYTEADDRESS (1024 * 8)

//******These are constants that can be used to tune the performance of the API
//This is the number of packets we queue up on the completion port.
//Raising this number can reduce dropped packets and improve receive bandwidth, at the expense of a 
// somwhat larger memory footprint for the API.
//Raising it might be a good idea if we expect the CPU load of the system to 
// be high when we are transmitting and receiving with the FPGA, or if the
// speed of the CPU itself is somewhat marginal.
#define NUMOUTSTANDINGREADS 400

//This is the number of writes we might send out before checking to see if any were acknowledged
//Raising this number somewhat can improve transmission bandwidth, at the expense of a somewhat
//	larger memory footprint for the API.
//This number should be larger than NUMOUTSTANDINGREADS
#define NUMOUTSTANDINGWRITES 250

//******
//******Other (internal) constants.
//******

//******These are constants that should only be changed if the network protocol changes
//(ie we want to support jumbo frames and have updated the hardware-side API controller
//to accomodate that. Recompilation will be required, or new APIs in PacketDriver.
//This is the maximum packet size (entire packet including header)
#define MAXPACKETSIZE 1514

//This is the maximum packet payload size (entire packet minus header)
//Should be between 10 and 1500 for normal packets
#define MAXPACKETDATASIZE (MAXPACKETSIZE-14)

//This should be the maximum packet data size minus 9 for the write command, start address and length
#define MAXWRITESIZE (MAXPACKETDATASIZE - 9)
//This should be the maximum packet data size minus 5 for the read command and start address
#define MAXREADSIZE (MAXPACKETDATASIZE - 5)


#ifdef DEBUG
#define PRINTF(x) printf x
#else
#define PRINTF(x)
#endif
#ifdef BIGDEBUG	
#define BIGDEBUG_ONLY(x) x
#else
#define BIGDEBUG_ONLY(x)
#endif
#pragma intrinsic(_byteswap_ulong,_byteswap_ushort) //jic. And btw why don't we define BYTE_ORDER

//PUBLIC FUNCTIONS
//Constructor for the class
//Return with an error code if anything goes wrong.
SIRC_DLL_LINKAGE SRV_SIRC::SRV_SIRC(uint32_t **registerFile, uint8_t **inputBuffer, uint8_t **outputBuffer,
             uint32_t driverVersion,
             wchar_t *nicName)
{
	setLastError(0);
	//Make connection to NIC driver
    PacketDriver = OpenPacketDriver(nicName,driverVersion,false);
    if (!PacketDriver) {
        setLastError(FAILDRIVERPRESENT);
		return;
	}

    //See what MAC address we have
	PacketDriver->GetMacAddress(My_MACAddress);

    //See how many outstanding packets we can have
    if (!PacketDriver->GetMaxOutstanding(&maxOutstandingReads,
                                         &maxOutstandingWrites)) {
        setLastError(FAILVMNSDRIVERACTIVE);//should not happen really
        return;
    }
    //Unlimited?
    //With the V3 interface there is a limit to how many packets we can have posted.
    //In some cases its 128, in others just 32.
    if (maxOutstandingReads == 0)
        maxOutstandingReads = NUMOUTSTANDINGREADS;
    if (maxOutstandingWrites == 0)
        maxOutstandingWrites = NUMOUTSTANDINGWRITES;

    maxInputDataBytes  = MAXINPUTDATABYTEADDRESS;
    maxOutputDataBytes = MAXOUTPUTDATABYTEADDRESS;

    //Make these optional so the user can better control them (and their sizes)
    if (*registerFile == NULL)
        *registerFile = (uint32_t *) malloc(256 * sizeof(uint32_t));
    if (*inputBuffer == NULL)
        *inputBuffer = (uint8_t *) malloc(maxInputDataBytes * sizeof(uint8_t));
    if (*outputBuffer == NULL)
        *outputBuffer = (uint8_t *) malloc(maxOutputDataBytes * sizeof(uint8_t));

	if(!*registerFile || !*inputBuffer || !*outputBuffer){
		setLastError(FAILMEMALLOC);
		return;
	}

	regFileP = *registerFile;
	inputBufP = *inputBuffer;
	outputBufP = *outputBuffer;

	memset(WriteAndRunHostMACAddress,0,6);

	//Queue up a bunch of receives
	//We want to keep this full, so every time we read
	// one out we should add one back.
	for(UINT32 i = 0; i < maxOutstandingReads; i++){
		if(!addReceive()){
			return;
		}
	}
    PRINTF(("MaxPosted: %u reads %u writes\n",maxOutstandingReads,maxOutstandingWrites));	
    printf("My MAC Address is %02x", My_MACAddress[0]);
    for (int i = 1; i < 6; i++)
        printf(":%02x", My_MACAddress[i]);
    printf("\n");
	return;
}

SRV_SIRC::~SRV_SIRC(){
	delete PacketDriver;
}

//Dynamic parameters
//Retrieve the active set of parameters and limits for this instance
BOOL SRV_SIRC::getParameters(SIRC_SERVER::PARAMETERS *outParameters, uint32_t maxOutLength)
{
    SIRC_SERVER::PARAMETERS params;

    params.myVersion            = SIRC_PARAMETERS_CURRENT_VERSION;
    params.maxInputDataBytes    = maxInputDataBytes;
    params.maxOutputDataBytes   = maxOutputDataBytes;
    params.maxOutstandingReads  = maxOutstandingReads;
    params.maxOutstandingWrites = maxOutstandingWrites;

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
BOOL SRV_SIRC::setParameters(const SIRC_SERVER::PARAMETERS *inParameters, uint32_t length)
{
    //Sometimes you got to know what you are doing.
    if ((length < sizeof(*inParameters)) ||
        (inParameters->myVersion < SIRC_PARAMETERS_CURRENT_VERSION)){
        setLastError(INVALIDLENGTH);
        return false;
    }

    //Check with packet protocol for maxOutstanding<>
    uint32_t maxReads, maxWrites;
    if (!PacketDriver->GetMaxOutstanding(&maxReads,
                                         &maxWrites)) {
        setLastError(FAILVMNSDRIVERACTIVE);//should not happen really
        return false;
    }
    //Unlimited? Or not changing.
    if ((maxReads == 0) || (maxReads == inParameters->maxOutstandingReads))
        maxOutstandingReads  = inParameters->maxOutstandingReads;
    else {
        setLastError(INVALIDLENGTH);
        return false;
    }

    if ((maxWrites == 0) || (maxWrites == inParameters->maxOutstandingWrites))
        maxOutstandingWrites  = inParameters->maxOutstandingWrites;
    else {
        setLastError(INVALIDLENGTH);
        return false;
    }

    
    maxInputDataBytes    = inParameters->maxInputDataBytes;
    maxOutputDataBytes   = inParameters->maxOutputDataBytes;

    setLastError(0);
    return true;
}


//Process all incoming commands until the execute command is received
BOOL SRV_SIRC::processCommands(bool *writeAndExecute){
	PACKET *        Packet;
    PACKET_MODE     Mode;

	setLastError(0);
	bool execute = false;

	//Keep pulling packets until we get an execute command
	while(1){
        //
        //  Wait for an I/O to complete.
        //
        
        Packet = NULL;
        Mode = PacketDriver->GetNextCompletedPacket(&Packet,INFINITE);

        if (Mode == PacketModeInvalid)
            return false;

		BIGDEBUG_ONLY(printf("***********%c-Completion on packet @ %p  [%p]\n", 
               (Packet->Mode == PacketModeReceiving) ? 'R' : 'T',
                             Packet, Packet->Buffer));

        if (Packet->Mode == PacketModeReceiving ){

            if(!processPacket(Packet, &execute, writeAndExecute)){
                return false;
            }

            //When we complete a packet recieve, we might have to do something.
            // For example, add another receive to the queue.
            if(!addReceive(Packet)){
                return false;
            }

            if(execute){
                //If this was an execute packet, return
                return true;
            }
        }
        else{
            assert((Packet->Mode == PacketModeTransmitting) ||
                   (Packet->Mode == PacketModeTransmittingBuffer));
            PacketDriver->FreePacket(Packet,false);
        }
	}

	return true;
}

//Read the addresses from 0 to length back to the host
BOOL SRV_SIRC::sendReadBacks(uint32_t length){
	uint32_t startAddress = 0;
	uint32_t currLength;

	while(length > 0){
		if(length > MAXREADSIZE - 4){
			currLength = MAXREADSIZE - 4;
		}
		else{
			currLength = length;
		}	
	
		if(!createReadBackPacketAndTransmit(startAddress, currLength, length)){
			return false;
		}
	
		startAddress += currLength;
		length -= currLength;
	}
	return true;
}

//PRIVATE FUNCTIONS

//This function queues a receive on the network port
//Return true on success, return false w/error code on failure
inline BOOL SRV_SIRC::addReceive(PACKET *Packet){
    
    if (Packet)
        Packet->Length = MAXPACKETSIZE;//recycle
    else
        Packet = PacketDriver->AllocatePacket(NULL,MAXPACKETSIZE,true);

	if(!Packet){
		setLastError(FAILMEMALLOC);
		return false;
	}

	BIGDEBUG_ONLY(printf("***********Adding receive packet @ %p  [%p]\n", Packet, Packet->Buffer));

    PacketDriver->PostReceivePacket(Packet);

    return true;
}

//This function will return true if the packet was processed successfully and the ack
//	was sent successfully
BOOL SRV_SIRC::processPacket(PACKET* Packet, bool *execute, bool *writeAndExecute){
	assert(Packet != NULL);

	uint8_t *message;
	uint16_t length;

	message = Packet->Buffer;

	//Make sure it is directed at specifically us, not just a broadcast packet
    if (memcmp(message,My_MACAddress,6) != 0)
		return true;

	length = message[12] * 256 + message[13];
	if(length == 0){
		return sendErrorMessage(RECEIVE_ERROR_PACKET_LENGTH, message);
	}

	switch(message[14]){
		case 'r':
			if(!checkReadPacket(message)){
				return false;
			}
			break;
		case 'w':
			if(!checkWritePacket(message)){
				return false;
			}
			break;
		case 'y':
			if(!checkRegReadPacket(message)){
				return false;
			}
			break;
		case 'k':
			if(!checkRegWritePacket(message, execute)){
				return false;
			}
			break;
		case 'g':
			if(!checkWriteAndRunPacket(message, execute, writeAndExecute)){
				return false;
			}
			break;
		case 'm':
			if(!checkResetPacket(message)){
				return false;
			}
			break;
		default:
			//if(!sendErrorMessage(RECEIVE_ERROR_COMMAND, message)){
				//return false;
			//}
			break;
	}
	return true;
}

//Send a message in response to the provided source message with the included error number
BOOL SRV_SIRC::sendErrorMessage(int8_t errorNumber, uint8_t *sourceMessage){

	//The packet will be 2 bytes long
	if (!allocateAndFillPacket(sourceMessage + 6, 2))
        return false;

	//Set the first byte to 'e'
	currentBuffer[0] = 'e';
	currentBuffer[1] = errorNumber;

	if(addTransmit(currentPacket))
        return true;
    PRINTF(("Write not sent!\n"));
    setLastError(INVALIDERRORTRANSMIT);
    return false;
}

BOOL SRV_SIRC::allocateAndFillPacket(uint8_t *sourceMAC, uint16_t length){
	//Get a packet to put this message in.
	currentPacket = PacketDriver->AllocatePacket(NULL,MAXPACKETSIZE,false);
	if(!currentPacket){
		setLastError(FAILMEMALLOC);
		return false;
	}

	currentPacket->Mode = PacketModeTransmitting;

	//Get the beginning of the packet payload (header is 14 bytes)
	currentBuffer = &(currentPacket->Buffer[14]);

	assert(length <= MAXPACKETDATASIZE);

	//The length of the frame will be the length of the payload plus 6 + 6 + 2 (dest MAC,
	//  source MAC, and payload length)
	currentPacket->nBytesAvail = length + 14;

	//Set the destination and source addresses of the packet (0-5 and 6-11)
	memcpy(currentPacket->Buffer, sourceMAC, 6);
	memcpy(currentPacket->Buffer + 6, My_MACAddress, 6);

	//The payload length field (bytes 12 and 13) does not include the length of the header
#if defined(_MSC_VER) //other compilers might not
    *(uint16_t*)(currentPacket->Buffer+12) = _byteswap_ushort(length);
#else
	currentPacket->Buffer[12] = (length) >> 8;
	currentPacket->Buffer[13] = (length) % 256;
#endif

    return true;
}



//This function adds a transmit to the output queue and sends the message
//Return true if the send goes OK, return false if not.
//Don't bother with an error code, the function that calls this will take care of that.
inline BOOL SRV_SIRC::addTransmit(PACKET* Packet){
	HRESULT Result;

	BIGDEBUG_ONLY(printf("***********Transmitting packet @ %p  [%p]\n", Packet, Packet->Buffer));

    Result = PacketDriver->PostTransmitPacket(Packet);

	if(Result != S_OK && Result != ERROR_IO_PENDING){
		PRINTF(("Bad transmission packet!\n"));
		return false;
	}
	return true;
}

BOOL SRV_SIRC::checkResetPacket(uint8_t *sourceMessage){
	assert(sourceMessage != NULL);

	uint16_t length;

	length = sourceMessage[12] * 256 + sourceMessage[13];
	//Is this reset command the right length?
	if(length == 1){
        //Send the appropriate read values back
        return sendResetAck(sourceMessage);
    }

    return sendErrorMessage(RECEIVE_ERROR_RESET_LENGTH, sourceMessage);
}

BOOL SRV_SIRC::sendResetAck(uint8_t *sourceMessage){

	//The packet will be 1 bytes long
	if (!allocateAndFillPacket(sourceMessage + 6, 1))
        return false;

	//Set the byte to 'm'
	currentBuffer[0] = 'm';

	if(addTransmit(currentPacket))
        return true;
    PRINTF(("Reset Ack not sent!\n"));
    setLastError(INVALIDRESETTRANSMIT);
    return false;
}

BOOL SRV_SIRC::checkRegWritePacket(uint8_t *sourceMessage, bool *execute){
	assert(sourceMessage != NULL);

	uint16_t length;

	length = sourceMessage[12] * 256 + sourceMessage[13];
	//Is this reg write command the right length?
	if(length == 6){
        //Perform the write and send the appropriate ack values back
        return sendRegWriteAck(sourceMessage, execute);
    }

    return sendErrorMessage(RECEIVE_ERROR_REG32_WRITE_LENGTH, sourceMessage);
}

BOOL SRV_SIRC::sendRegWriteAck(uint8_t *sourceMessage, bool *execute){

	uint8_t regAddress = sourceMessage[15];

	uint32_t value = ((uint32_t) sourceMessage[16] << 24) + ((uint32_t) sourceMessage[17] << 16)+
		((uint32_t) sourceMessage[18] << 8) + ((uint32_t) sourceMessage[19]);
		
	regFileP[regAddress] = value;

	if(regAddress == 255 && value == 1){
		*execute = true;
	}

	//The packet will be 6 bytes long
	if (!allocateAndFillPacket(sourceMessage + 6, 6))
        return false;

	memcpy(currentBuffer, &(sourceMessage[14]), 6);

	if(addTransmit(currentPacket))
        return true;
    PRINTF(("Reset Ack not sent!\n"));
    setLastError(INVALIDPARAMWRITETRANSMIT);
    return false;
}

BOOL SRV_SIRC::checkWritePacket(uint8_t *sourceMessage){
	assert(sourceMessage != NULL);

	uint16_t packetLength;

	uint32_t startAddress;
	uint32_t writeLength;

	packetLength = ((uint16_t)sourceMessage[12] << 8) + ((uint16_t) sourceMessage[13]);

	//Is this write command the wrong length?
	if(packetLength < 10){
		return sendErrorMessage(RECEIVE_ERROR_WRITE_LENGTH, sourceMessage);
	}

	startAddress = ((uint32_t) sourceMessage[15] << 24) + ((uint32_t) sourceMessage[16] << 16)+
		((uint32_t) sourceMessage[17] << 8) + ((uint32_t) sourceMessage[18]);

	writeLength = ((uint32_t) sourceMessage[19] << 24) + ((uint32_t) sourceMessage[20] << 16)+
		((uint32_t) sourceMessage[21] << 8) + ((uint32_t) sourceMessage[22]);

	if(packetLength-9 != writeLength){
		return sendErrorMessage(RECEIVE_ERROR_WRITE_LENGTH, sourceMessage);
	}

	if(startAddress + writeLength > maxInputDataBytes){
		return sendErrorMessage(RECEIVE_ERROR_WRITE_LENGTH, sourceMessage);
	}

	//Perform the write
	memcpy(inputBufP + startAddress, sourceMessage + 23, writeLength);
	
	//Send the appropriate ack values back
	return sendWriteAck(sourceMessage);
}

BOOL SRV_SIRC::sendWriteAck(uint8_t *sourceMessage){

	//The packet will be 9 bytes long
	if (!allocateAndFillPacket(sourceMessage + 6, 9))
        return false;

	memcpy(currentBuffer, &(sourceMessage[14]), 9);

	if(addTransmit(currentPacket))
        return true;
    PRINTF(("Reset Ack not sent!\n"));
    setLastError(INVALIDWRITETRANSMIT);
    return false;
}

BOOL SRV_SIRC::checkWriteAndRunPacket(uint8_t *sourceMessage, bool *execute, bool *writeAndExecute){
	assert(sourceMessage != NULL);

	uint16_t packetLength;

	uint32_t startAddress;
	uint32_t writeLength;

	packetLength = ((uint16_t)sourceMessage[12] << 8) + ((uint16_t) sourceMessage[13]);

	//Is this write command the wrong length?
	if(packetLength < 10){
		return sendErrorMessage(RECEIVE_ERROR_WRITE_AND_EXECUTE_LENGTH, sourceMessage);
	}

	startAddress = ((uint32_t) sourceMessage[15] << 24) + ((uint32_t) sourceMessage[16] << 16)+
		((uint32_t) sourceMessage[17] << 8) + ((uint32_t) sourceMessage[18]);

	writeLength = ((uint32_t) sourceMessage[19] << 24) + ((uint32_t) sourceMessage[20] << 16)+
		((uint32_t) sourceMessage[21] << 8) + ((uint32_t) sourceMessage[22]);

	if(packetLength-9 != writeLength){
		return sendErrorMessage(RECEIVE_ERROR_WRITE_AND_EXECUTE_LENGTH, sourceMessage);
	}

	if(startAddress + writeLength > maxInputDataBytes){
		return sendErrorMessage(RECEIVE_ERROR_WRITE_AND_EXECUTE_LENGTH, sourceMessage);
	}

	//Perform the write
	memcpy(inputBufP + startAddress, sourceMessage + 23, writeLength);
	
	//There is no ack right now, since the readback is the ack.
	//However, we should save the MAC address of the host
	memcpy(WriteAndRunHostMACAddress, sourceMessage + 6, 6);

	regFileP[255] = 1;

	*execute = true;
	*writeAndExecute = true;
	return true;
}

BOOL SRV_SIRC::checkRegReadPacket(uint8_t *sourceMessage){
	assert(sourceMessage != NULL);

	uint16_t length;

	length = sourceMessage[12] * 256 + sourceMessage[13];
	//Is this reg read command the wrong length?
	if(length == 2){
        //Perform the read and send the appropriate ack values back
        return sendRegReadAck(sourceMessage);
    }

    return sendErrorMessage(RECEIVE_ERROR_REG32_READ_LENGTH, sourceMessage);
}

BOOL SRV_SIRC::sendRegReadAck(uint8_t *sourceMessage){

	//The packet will be 6 bytes long
	if (!allocateAndFillPacket(sourceMessage + 6, 6))
        return false;

	memcpy(currentBuffer, &(sourceMessage[14]), 2);

	uint32_t regValue = regFileP[sourceMessage[15]];
#if defined(_MSC_VER) //other compilers might not
    *(uint32_t*)(currentBuffer+2) = _byteswap_ulong(regValue);
#else
	currentBuffer[2] = (regValue >> 24) % 256;
	currentBuffer[3] = (regValue >> 16) % 256;
	currentBuffer[4] = (regValue >> 8) % 256;
	currentBuffer[5] = (regValue) % 256;
#endif

	if(addTransmit(currentPacket))
        return true;
    PRINTF(("Reset Ack not sent!\n"));
    setLastError(INVALIDPARAMREADTRANSMIT);
    return false;
}


BOOL SRV_SIRC::checkReadPacket(uint8_t *sourceMessage){
	assert(sourceMessage != NULL);

	uint16_t packetLength;

	uint32_t startAddress;
	uint32_t readLength;

	packetLength = ((uint16_t)sourceMessage[12] << 8) + ((uint16_t) sourceMessage[13]);

	//Is this read command the wrong length?
	if(packetLength != 9){
		return sendErrorMessage(RECEIVE_ERROR_READ_LENGTH, sourceMessage);
	}

	startAddress = ((uint32_t) sourceMessage[15] << 24) + ((uint32_t) sourceMessage[16] << 16)+
		((uint32_t) sourceMessage[17] << 8) + ((uint32_t) sourceMessage[18]);

	readLength = ((uint32_t) sourceMessage[19] << 24) + ((uint32_t) sourceMessage[20] << 16)+
		((uint32_t) sourceMessage[21] << 8) + ((uint32_t) sourceMessage[22]);

	if(startAddress + readLength > maxOutputDataBytes){
		return sendErrorMessage(RECEIVE_ERROR_READ_LENGTH, sourceMessage);
	}

	//Send the appropriate read values back
	return sendReadAcks(sourceMessage, startAddress, readLength);
}

BOOL SRV_SIRC::sendReadAcks(uint8_t *sourceMessage, uint32_t startAddress, uint32_t readLength){
	uint32_t currLength;

	while(readLength > 0){
		if(readLength > MAXREADSIZE){
			currLength = MAXREADSIZE;
		}
		else{
			currLength = readLength;
		}	
	
		if(!createReadPacketAndTransmit(sourceMessage, startAddress, currLength)){
			return false;
		}
	
		startAddress += currLength;
		readLength -= currLength;
	}
	return true;
}

BOOL SRV_SIRC::createReadPacketAndTransmit(uint8_t *sourceMessage, uint32_t startAddress, uint32_t readLength){

	//The packet will be readLength + 5 bytes long
	if (!allocateAndFillPacket(sourceMessage + 6, readLength + 5))
        return false;

	currentBuffer[0] = 'r';
#if defined(_MSC_VER) //other compilers might not
    *(uint32_t*)(currentBuffer+1) = _byteswap_ulong(startAddress);
#else
	currentBuffer[1] = (startAddress >> 24) % 256;
	currentBuffer[2] = (startAddress >> 16) % 256;
	currentBuffer[3] = (startAddress >> 8) % 256;
	currentBuffer[4] = (startAddress) % 256;
#endif

	memcpy(currentBuffer + 5, outputBufP + startAddress, readLength);

	if(addTransmit(currentPacket))
        return true;
    PRINTF(("Reset Ack not sent!\n"));
    setLastError(INVALIDREADTRANSMIT);
    return false;
}

BOOL SRV_SIRC::createReadBackPacketAndTransmit(uint32_t startAddress, uint32_t readLength, uint32_t remainingLength){

	//The packet will be readLength + 9 bytes long
	if (!allocateAndFillPacket(WriteAndRunHostMACAddress, readLength + 9))
        return false;

	currentBuffer[0] = 'g';
#if defined(_MSC_VER) //other compilers might not
    *(uint32_t*)(currentBuffer+1) = _byteswap_ulong(startAddress);
    *(uint32_t*)(currentBuffer+5) = _byteswap_ulong(remainingLength);
#else
	currentBuffer[1] = (startAddress >> 24) % 256;
	currentBuffer[2] = (startAddress >> 16) % 256;
	currentBuffer[3] = (startAddress >> 8) % 256;
	currentBuffer[4] = (startAddress) % 256;

	currentBuffer[5] = (remainingLength >> 24) % 256;
	currentBuffer[6] = (remainingLength >> 16) % 256;
	currentBuffer[7] = (remainingLength >> 8) % 256;
	currentBuffer[8] = (remainingLength) % 256;
#endif

	memcpy(currentBuffer + 9, outputBufP + startAddress, readLength);

	if(addTransmit(currentPacket))
        return true;
    PRINTF(("Reset Ack not sent!\n"));
    setLastError(INVALIDREADTRANSMIT);
    return false;
}


//void SRV_SIRC::printPacket(PACKET* packet){
//	unsigned int i;
//	
//	printf("Packet contents:\n");
//	printf("\t%18s%18s%6s[Data]\n\t", "[Dest Address]", "[Src Address]","[Len]");
//	for(i = 0; i < packet->nBytesAvail; i++){
//		printf("%02x ", packet->Buffer[i]);	
//	}
//	printf("\n");
//}
