// Title: ETH_SIRC class definition
// 
// Description: Read and write to input/output buffers and parameter register file.  Also, 
// start execution, wait until execution is completed and reconfigure the
// device via SystemACE.
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

#include "sirc_internal.h"

//******
//******The following default values can be changed using the get/setParameter methods.
//******

//******The following constants must match the setup of the hardware-side interface.
//How many bytes are in the input data buffer?
//The default value is 128KB
#define MAXINPUTDATABYTEADDRESS (1024 * 128)

//How many bytes are in the output data buffer?
//The default value is 8KB
#define MAXOUTPUTDATABYTEADDRESS (1024 * 8)

//******The following constants control the (re)transmission protocol failure cases.
//Maximum number of times we will try to re-send a given request or command before giving up.
//If the other factors are set correctly and there is no hardware problem in the system
// we should not need to increase this.
//Applies to sendWrite, sendRead, sendParamRegisterWrite, sendParamRegisterRead, sendRun
// and sendWriteAndRun.
//Does not apply to waitDone (has own explicit timeout).
#define MAXRETRIES 3

//Number of milliseconds we will wait between valid write acks before declaring 
// that an outstanding write packet has not been successful.
//Notice, the entire write does not have to be completed in this time, but we should
// not have to wait more than N milliseconds before we see the first ack, nor more than
// N milliseconds between acks.
//This number should not be reduced below 1000.
//Applies to sendWrite and sendParamRegisterWrite
//Also applies during write phase of sendWriteAndRun
#define WRITETIMEOUT 2000

//Number of milliseconds we will wait between valid read responses before declaring 
// that an outstanding read request has not been successful.
//Notice, the entire read does not have to be completed in this time, but we should
// not have to wait more than N milliseconds before we see the first response, nor more than
// N milliseconds between responses.
//This number should not be reduced below 1000.
//Applies to sendRead and sendParamRegisterRead
//Also applies during readback phase of sendWriteAndRun
#define READTIMEOUT 2000

//******These are constants that can be used to tune the performance of the API
//This is the number of packets we queue up on the completion port.
//Raising this number can reduce dropped packets and improve receive bandwidth, at the expense of a 
// somwhat larger memory footprint for the API.
//Raising it might be a good idea if we expect the CPU load of the system to 
// be high when we are transmitting and receiving with the FPGA, or if the
// speed of the CPU itself is somewhat marginal.
//Note that the underlying packet interface might put a cap on this.
#define NUMOUTSTANDINGREADS 400

//This is the number of writes we might send out before checking to see if any were acknowledged
//Raising this number somewhat can improve transmission bandwidth, at the expense of a somewhat
//	larger memory footprint for the API.
//Note that the underlying packet interface might put a cap on this.
//This number should be smaller than NUMOUTSTANDINGREADS
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
#define DEBUG_ONLY(x) x
#define DEBUG_ONLY_1ARG(x) ,x
#define DEBUG_ONLY_2ARGS(x,y) ,x,y
#else
#define PRINTF(x)
#define DEBUG_ONLY(x)
#define DEBUG_ONLY_1ARG(x)
#define DEBUG_ONLY_2ARGS(x,y)
#endif
#ifdef BIGDEBUG	
#define BIGDEBUG_ONLY(x) x
#else
#define BIGDEBUG_ONLY(x)
#endif

//PUBLIC FUNCTIONS
//Constructor for the class
//FPGA_ID: 6 byte array containing the MAC adddress of the destination FPGA
//Return with an error code if anything goes wrong.
SIRC_DLL_LINKAGE ETH_SIRC::ETH_SIRC(uint8_t *FPGA_ID, uint32_t driverVersion, wchar_t *nicName){
	setLastError(0);
	//Make connection to NIC driver
    PacketDriver = OpenPacketDriver(nicName,driverVersion,false);
    if (!PacketDriver) {
        setLastError(FAILDRIVERPRESENT);
		return;
	}

    //See what MAC address we have
	PacketDriver->GetMacAddress(ethHeader.My_MACAddress);
#if 0
    cout << "source MAC: " 
         << hex << setw(2) << setfill('0') 
         << setw(2) << (int)ethHeader.My_MACAddress[0] << ":" 
         << setw(2) << (int)ethHeader.My_MACAddress[1] << ":" 
         << setw(2) << (int)ethHeader.My_MACAddress[2] << ":" 
         << setw(2) << (int)ethHeader.My_MACAddress[3] << ":" 
         << setw(2) << (int)ethHeader.My_MACAddress[4] << ":" 
         << setw(2) << (int)ethHeader.My_MACAddress[5] << dec << endl;
#endif

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

    writeTimeout       = WRITETIMEOUT;
    readTimeout        = READTIMEOUT;
    maxRetries         = MAXRETRIES;
    maxInputDataBytes  = MAXINPUTDATABYTEADDRESS;
    maxOutputDataBytes = MAXOUTPUTDATABYTEADDRESS;

	if(FPGA_ID == NULL){
		PRINTF(("Invalid destination MAC address given!\n"));
		setLastError(INVALIDFPGAMACADDRESS);
		return;
	}

	memcpy(ethHeader.FPGA_MACAddress, FPGA_ID, 6);

	outstandingTransmits = 0;
    currentPacket = NULL;
    currentBuffer = NULL;

#ifdef DEBUG
	writeResends = 0;
	readResends = 0;
	paramWriteResends = 0;
	paramReadResends = 0;
	resetResends = 0;
	writeAndRunResends = 0;
#endif

	//Queue up a bunch of receives
	//We want to keep this full, so every time we read
	// one out we should add one back.
	for(UINT32 i = 0; i < maxOutstandingReads; i++){
		if(!addReceive()){
            return;
		}
	}
	
	//Send a soft reset to make sure that the user circuit is not running.
    LogIt(LOGIT_TIME_MARKER);
	if(!sendReset()){
		setLastError(FAILINITIALCONTACT);
	}

	return;
}

ETH_SIRC::~ETH_SIRC(){
	PRINTF(("Write Resends = %d\n", writeResends));
	PRINTF(("Read Resends = %d\n", readResends));
	PRINTF(("Param Reg Write Resends = %d\n", paramWriteResends));
	PRINTF(("Param Reg Read Resends = %d\n", paramReadResends));
	PRINTF(("Param Reg Write Resends = %d\n", paramWriteResends));
	PRINTF(("Param Reg Read Resends = %d\n", paramReadResends));
	PRINTF(("Write and Run Resends = %d\n", writeAndRunResends));

    delete PacketDriver;
}

//Dynamic parameters
//Retrieve the active set of parameters and limits for this instance
BOOL ETH_SIRC::getParameters(SIRC::PARAMETERS *outParameters, uint32_t maxOutLength)
{
    SIRC::PARAMETERS params;

    params.myVersion            = SIRC_PARAMETERS_CURRENT_VERSION;
    params.maxInputDataBytes    = maxInputDataBytes;
    params.maxOutputDataBytes   = maxOutputDataBytes;
    params.writeTimeout         = writeTimeout;
    params.readTimeout          = readTimeout;
    params.maxRetries           = maxRetries;
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
BOOL ETH_SIRC::setParameters(const SIRC::PARAMETERS *inParameters, uint32_t length)
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
    writeTimeout         = inParameters->writeTimeout;
    readTimeout          = inParameters->readTimeout;
    maxRetries           = inParameters->maxRetries;

    setLastError(0);
    return true;
}


//Helper macros

//Check last error, bail out if set
#define MAYBE_BAILOUT() {                                                         \
    int8_t _err2 = getLastError();                                                \
    if (_err2) return bailOut(_err2);                                             \
}

//reduce some visual clutter
#define BIGDEBUG_adding_receive(_P_)                                              \
    BIGDEBUG_ONLY({	printf("***********Adding receive packet @ %p\n", _P_);});
#define BIGDEBUG_adding_transmit(_P_)                                             \
    BIGDEBUG_ONLY({	printf("***********Transmitting packet @ %p\n", _P_); });
#define BIGDEBUG_packet_received(_P_,_V_)                                         \
    BIGDEBUG_ONLY({                                                               \
       printf("***********Packet receive completed on packet @ %p\n", _P_);       \
       if (_V_) printPacket(_P_);});
#define BIGDEBUG_packet_matched(_P_)                                              \
    BIGDEBUG_ONLY({printf("Matched in scoreboard packet @ %p!\n", _P_);});

//Interface methods.

//Send a block of data to an input buffer on the FPGA
// startAddress: local address on FPGA input buffer to begin writing at
// length: # of bytes to write
// buffer: data to be sent to FPGA
//Return true if write is successful.
//If write fails for any reason, return false w/error code
BOOL ETH_SIRC::sendWrite(uint32_t  startAddress, uint32_t length, uint8_t *buffer){
	//This function breaks the write request into packet-appropriate write commands.
	//These write commands are sent in blocks of maxOutstandingWrites.
	//Each write command is acknowledged when it has been received by the FPGA.
	//After sending out maxOutstandingWrites write commands, we check to see which,
	// if any commands have been acknowledged.  If they are not acknowledged in a 
	// timely manner, we resend the write command.
	//If any command is not acknowledged after MAXRETRIES attempts, we will
	// return false.
	uint32_t currLength;
	uint32_t numRetries;
	
    LogIt("sirc:sw %u %u",startAddress, length);

	setLastError(0);

	if(!buffer){
		setLastError(INVALIDBUFFER);
		return false;
	}

	if(startAddress > maxInputDataBytes){
		setLastError(INVALIDADDRESS);
		return false;
	}

	if(length == 0 || startAddress + length > maxInputDataBytes){
		setLastError(INVALIDLENGTH);
		return false;
	}

	while(length > 0){
		//Break this write into MAXWRITESIZE sized chunks or smaller
		if(length > MAXWRITESIZE)
			currLength = MAXWRITESIZE;
		else
			currLength = length;

		if(!createWriteRequestBackAndTransmit(startAddress, currLength, buffer,
                                              currLength == length)){
			//If the send errored out, something is very wrong.
            return bailOut(0);
		}

		//Update all of the markers
		buffer += currLength;
		startAddress += currLength;
		length -= currLength;

		//See if we have too many outstanding messages (or if we are done transmitting).
		//If so, we should scoreboard and check off any write acks we got back
		//A better way to do this would have an independent thread take care of the
		// scoreboarding, but synchronization might be very difficult
		if(outstandingTransmits >= (int)maxOutstandingWrites || length == 0){
			//Try to check writes off the scoreboard & resend outstanding packets up to N times
			numRetries = 0;
			for(;;){
				//Try to receive acks for the outstanding writes
				if(receiveWriteAcks())
					//We got all of the acks back, so break out of the for(;;) and go to the next block of writes
					break;

                //Verify that receiveWriteAcks did not return false due to some error
                // rather then just not getting back all of the acks we expected.
                MAYBE_BAILOUT();

                //Not all of the writes' acks came back, so re-send any outstanding writes that are still left outstanding
                //However, don't resend anything if that was the last time around.
                if(numRetries++ >= maxRetries){
                    //We have resent too many times
                    PRINTF(("Write resent too many times without acknowledgement!\n"));
                    return bailOut(FAILWRITEACK);
                }
                else{
                    LogIt("sirc::sw.retries %u",numRetries);
                    if (!resendOutstandingPackets(INVALIDWRITETRANSMIT DEBUG_ONLY_2ARGS("Write",&writeResends))) {
                        return false;
                    }
                }
			}
		}
	}

	//Make sure that there are no outstanding packets
	setLastError(0);
	assert(outstandingPackets.empty());
	assert(outstandingTransmits == 0);
	return true;
}

//Read a block of data from the output buffer of the FPGA
// startAddress: local address on FPGA output buffer to begin reading from
// length: # of bytes to read
// buffer: destination for data received from FPGA
//Return true if read is successful.
//If read fails for any reason, return false w/ error code
BOOL ETH_SIRC::sendRead(uint32_t startAddress, uint32_t length, uint8_t *buffer){
	//This function sends this read request to the FPGA.
	//The FPGA responds by breaking up the read request into packet-appropriate responses.
	//If we receive all of the parts back from the read request, we directly return true.
	//If not, we keep track of the parts we missed and re-send requests for those parts.
	//If we need to resend any part of the initial read request more than MAXRETRIES times,
	// we will return false.
	uint32_t numRetries;

    LogIt("sirc:sr %u %u",startAddress, length);

	setLastError(0);

	if(!buffer){
		setLastError(INVALIDBUFFER);
		return false;
	}

	if(startAddress > maxOutputDataBytes){
		setLastError(INVALIDADDRESS);
		return false;
	}

	if(length == 0 || startAddress + length > maxOutputDataBytes){
		setLastError(INVALIDLENGTH);
		return false;
	}

	//Send the read request
	if(!createReadRequestBackAndTransmit(startAddress, length)){
		return false;
	}

	//Now that we've sent out the read request, try to get back some responses.
	numRetries = 0;
	for(;;){
		//Try to get back all of the read responses associated with the current outstanding
		// read requests.  The first time through we will only have 1 request on the queue.
		// However, for subsequent retries, this may be larger than 1.
		//If we don't get back all of the reads we want, we will have all of the necessary resends
		// sitting in the outstanding packet queue.
		if(receiveReadResponses(startAddress, buffer))
			//All of the reads came back, so we are done
            break;

        //Verify that receiveReadData did not return false due to some error
        // rather then just not getting back all of the read responses we expected.
        MAYBE_BAILOUT();

        //Not all of the read replies came back, so we should send all of the packets on the outstanding list
        // unless that was the last chance we had
        if(numRetries++ >= maxRetries){
            //We have resent too many times
            PRINTF(("Read resent too many times without response!\n"));
            return bailOut(FAILREADACK);
        }
        else{
            //Transmit/re-transmit the packets in the outstanding list.
            LogIt("sirc::sr.retries %u",numRetries);
            if (!resendOutstandingPackets(INVALIDREADTRANSMIT DEBUG_ONLY_2ARGS("Read",&readResends))) {
                return false;
            }
        }
	}

	setLastError(0);
	assert(outstandingPackets.empty());
	assert(outstandingReadStartAddresses.empty());
	assert(outstandingReadLengths.empty());
	assert(outstandingTransmits == 0);
	return true;
}

//Send a 32-bit value from the PC to the parameter register file on the FPGA
// regNumber: register to which value should be sent (between 0 and 254)
// value: value to be written
//Returns true if write is successful.
//If write fails for any reason, returns false.
// Check error code with getLastError()
BOOL ETH_SIRC::sendParamRegisterWrite(uint8_t regNumber, uint32_t value){
	uint32_t numRetries;
	
	setLastError(0);

	if(!(regNumber < 255)){
		setLastError(INVALIDADDRESS);
		return false;
	}

	if(!createParamWriteRequestBackAndTransmit(regNumber, value)){
		//If the send errored out, something is very wrong.
        return bailOut(getLastError());
	}

	//Try to check the write off.  Resend up to N times
	numRetries = 0;
	for(;;){
		//Try to receive param write acks for the outstanding param write
		if(receiveParamWriteAck())
			//We got the ack back, so break out of the for(;;)
			break;

        //Verify that receiveParamWriteAck did not return false due to some error
        // rather then just not getting back the ack we expected.
        MAYBE_BAILOUT();

        //The param write ack didn't come back, so re-send the outstanding packet
        //However, don't resend anything if that was the last time around.
        if(numRetries++ >= maxRetries){
            //We have resent too many times
            PRINTF(("Param reg write resent too many times without acknowledgement!\n"));
            return bailOut(FAILWRITEACK);
        }
        else{
            LogIt("sirc::pw.retries %u",numRetries);
            //NB: this is the same as iterating over the outstanding because there's just one.
            if (!resendOutstandingPackets(INVALIDPARAMWRITETRANSMIT DEBUG_ONLY_2ARGS("ParamWrite",&paramWriteResends))) {
                return false;
            }
        }
	}

	setLastError(0);
	//Make sure that there are no outstanding packets
	assert(outstandingPackets.empty());
	assert(outstandingTransmits == 0);
	return true;
}

//Read a 32-bit value from the parameter register file on the FPGA back to the PC
// regNumber: register to which value should be read (between 0 and 254)
// value: value received from FPGA
//Returns true if read is successful.
//If read fails for any reason, returns false.
// Check error code with getLastError().
BOOL ETH_SIRC::sendParamRegisterRead(uint8_t regNumber, uint32_t *value){
	uint32_t numRetries;
	
	setLastError(0);

	if(!(regNumber < 255)){
		setLastError(INVALIDADDRESS);
		return false;
	}

	if(!createParamReadRequestBackAndTransmit(regNumber)){
		//If the send errored out, something is very wrong.
        return bailOut(getLastError());
	}

	//Try to check the read off.  Resend up to N times
	numRetries = 0;
	for(;;){
		//Try to receive param read response for the outstanding param read
		if(receiveParamReadResponse(value, readTimeout))
			//We got the ack back, so break out of the for(;;)
			break;

        //Verify that receiveParamReadResponse did not return false due to some error
        // rather then just not getting back the ack we expected.
        MAYBE_BAILOUT();

        //The param read response didn't come back, so re-send the outstanding packet
        //However, don't resend anything if that was the last time around.
        if(numRetries++ >= maxRetries){
            //We have resent too many times
            PRINTF(("Param reg read resent too many times without acknowledgement!\n"));
            return bailOut(FAILREADACK);
        }
        else{
            LogIt("sirc::pr.retries %u",numRetries);
            //NB: this is the same as iterating over the outstanding because there's just one.
            if (!resendOutstandingPackets(INVALIDPARAMREADTRANSMIT DEBUG_ONLY_2ARGS("ParamRead",&paramReadResends))) {
                return false;
            }
        }
	}

	setLastError(0);
	//Make sure that there are no outstanding packets
	assert(outstandingPackets.empty());
	assert(outstandingTransmits == 0);
	return true;
}

//Raise execution signal on FPGA
//Returns true if signal is raised.
//If signal is not raised for any reason, returns false.
// Check error code with getLastError()
BOOL ETH_SIRC::sendRun(){
	uint32_t numRetries;
	
	setLastError(0);

	if(!createParamWriteRequestBackAndTransmit(255, 1)){
		//If the send errored out, something is very wrong.
        return bailOut(getLastError());
	}

	//Try to check the write off.  Resend up to N times
	numRetries = 0;
	for(;;){
		//Try to receive param write acks for the outstanding param write
		if(receiveParamWriteAck())
			//We got the ack back, so break out of the for(;;)
			break;

        //Verify that receiveParamWriteAck did not return false due to some error
        // rather then just not getting back the ack we expected.
        MAYBE_BAILOUT();

        //The param write ack didn't come back, so re-send the outstanding packet
        //However, don't resend anything if that was the last time around.
        if(numRetries++ >= maxRetries){
            //We have resent too many times
            PRINTF(("Run signal resent too many times without acknowledgement!\n"));
            return bailOut(FAILWRITEACK);
        }
        else{
            LogIt("sirc::r.retries %u",numRetries);
            //NB: this is the same as iterating over the outstanding because there's just one.
            if (!resendOutstandingPackets(INVALIDPARAMWRITETRANSMIT DEBUG_ONLY_2ARGS("Run",&paramWriteResends))) {
                return false;
            }
        }
	}

	setLastError(0);
	//Make sure that there are no outstanding packets
	assert(outstandingPackets.empty());
	assert(outstandingTransmits == 0);
	return true;
}

//Wait until execution signal on FPGA is lowered
// maxWaitTime: # of seconds to wait until timeout.
//Returns true if signal is lowered.
//If function fails for any reason, returns false.
// Check error code with getLastError().
BOOL ETH_SIRC::waitDone(uint32_t maxWaitTimeInMsec){
	uint32_t value;

	setLastError(0);

	for(;;){
		//Send out the read
		if(!createParamReadRequestBackAndTransmit(255)){
			//If the send errored out, something is very wrong.
            return bailOut(getLastError());
		}

		//Try to get the read back
		if(!receiveParamReadResponse(&value, maxWaitTimeInMsec)){
			//If receiveParamReadResponse timed out, replace the
			// error code with FAILWAITACK
            int8_t err = getLastError();
			if(err == FAILREADACK){
                //The param read response didn't come back in time, so error out
				PRINTF(("Wait done response didn't come back in time!\n"));
				err = FAILWAITACK;
			}

			return bailOut(err);
		}

        //We got the ack back
        //See if the done register is 0
        if(value == 0){
            setLastError(0);

            //Make sure that there are no outstanding packets
            assert(outstandingPackets.empty());
            assert(outstandingTransmits == 0);
            return true;
        }
	}

	setLastError(FAILDONE);
	return false;
}

//Send a soft reset to the user circuit (useful when debugging new applications
//	and the circuit refuses to give back control to the host PC)
//Returns true if the soft reset is accepted
//If the reset command is refused for any reason, returns false.
// Check error code with getLastError()
BOOL ETH_SIRC::sendReset(){
	uint32_t numRetries;
	
	setLastError(0);

	if(!createResetRequestAndTransmit()){
		//If the send errored out, something is very wrong.
        return bailOut(getLastError());
	}

	//Try to check the reset off.  Resend up to N times
	numRetries = 0;
	for(;;){
		//Try to receive reset acknowledge
		if(receiveResetAck())
			//We got the ack back, so break out of the for(;;)
			break;

        //Verify that receiveResetAck did not return false due to some error
        // rather then just not getting back the ack we expected.
        MAYBE_BAILOUT();

        //The reset response didn't come back, so re-send the outstanding packet
        //However, don't resend anything if that was the last time around.
        if(numRetries++ >= maxRetries){
            //We have resent too many times
            PRINTF(("Reset resent too many times without acknowledgement!\n"));
            return bailOut(FAILRESETACK);
        }
        else{
            LogIt("sirc::R.retries %u",numRetries);
            //NB: this is the same as iterating over the outstanding because there's just one.
            if (!resendOutstandingPackets(INVALIDRESETTRANSMIT DEBUG_ONLY_2ARGS("Reset",&resetResends))) {
                return false;
            }
        }
	}

	setLastError(0);
	//Make sure that there are no outstanding packets
	assert(outstandingPackets.empty());
	assert(outstandingTransmits == 0);
	return true;
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
BOOL ETH_SIRC::sendWriteAndRun(uint32_t startAddress, uint32_t inLength, uint8_t *inData, 
							  uint32_t maxWaitTimeInMsec, uint8_t *outData, uint32_t maxOutLength, 
							  uint32_t *outputLength){
	uint32_t numPackets;
	uint32_t currLength;
	uint32_t numRetries;
	
	setLastError(0);

	//Check the input parameters
	if(!inData){
		setLastError(INVALIDBUFFER);
		return false;
	}
	if(startAddress > maxInputDataBytes){
		setLastError(INVALIDADDRESS);
		return false;
	}
	if(inLength == 0 || startAddress + inLength > maxInputDataBytes){
		setLastError(INVALIDLENGTH);
		return false;
	}

	//Check the output parameters
	if(!outData){
		setLastError(INVALIDBUFFER);
		return false;
	}
	if(maxOutLength == 0 || maxOutLength > maxOutputDataBytes){
		setLastError(INVALIDLENGTH);
		return false;
	}

	//Try to send the data to the FPGA
	//First break the write request into packet-appropriate write commands.
	//The first N are sent using the normal write command, the last one is sent using the
	// write and execute command.
	//Determine how many packets are we going to need to send.
	//This division will round down to the next integer
	numPackets = inLength / MAXWRITESIZE;
	if(inLength % MAXWRITESIZE == 0){
		//If the length of the input buffer fits exactly into N packets, let's send 1 less
		numPackets--;
	}
	currLength = numPackets * MAXWRITESIZE;

	//There are 3 phases to this function: write initial data to FPGA, send last write & run packet,
	// wait for data to come back.
	//Aside from the normal, unrecoverable problems that can occur (invalid parameters,
	// memory allocation fail, etc), there are four types of "recoverable" errors that can happen.
	for (numRetries = 0; numRetries < maxRetries;){
		//Write the initial part of the data to the FPGA
		//We want all of the initial writes to be send and acknowledged before we send the write & run command.
		//This is in the while() loop because we don't know if the execution is destructive.
		//In the current system the input and output buffers are independent, but in the future they may not be.
		//Thus, if anything goes wrong we will have to re-send all of the input data before re-execution.
		if(numPackets > 0){
			if(!sendWrite(startAddress, currLength, inData)){
				//Error #1: The write of the first N packets using regular send write commands error out.
				//		The sendWrite function already takes care of resend attempts, so if this fails the 
				//		whole function should fail.
				return false;	
			}
		}

		//Now we want to send the last packet out via a write & run command.
		if(!createWriteAndRunRequestBackAndTransmit(startAddress + currLength, inLength - currLength, inData + currLength)){
			//If the send errored out, something is very wrong.
			return false;
		}

		//Try to receive the responses from the FPGA.
		//If we return true, we got back all of the responses without any trouble.
		//If we return false, this may be due to any number of causes.
		if(receiveWriteAndRunAcks(maxWaitTimeInMsec, maxOutLength, outData, outputLength)){
			//We got back all of the write & run responses and they fit within the
			// output buffer without trouble. We are done ok.
			setLastError(0);
			return true;
		}

        if(getLastError() == FAILWRITEACK){
            //Error #2: We got no response at all. In this case, we don't know if the
            //		initial packet was lost or if all of the responses (could be a small number)
            //		were lost.  Because of this, we have to retry the entire sequence again, 
            //		beginning at the very first write (execution might have overwritten some of the
            //		input data).
            emptyOutstandingPackets();
            sendReset();
            numRetries++;
            DEBUG_ONLY(writeAndRunResends++;);
            continue;
        }

        if(getLastError() == FAILWRITEANDRUNCAPACITY){
            //Error #3: We got all of the expected responses back, but they don't fit in the output buffer
            //		The receiveWriteAndRunAcks function has already set lastError to FAILWRITEANDRUNCAPACITY, 
            //		and set outputLength to the total length of the response
            return false;
        }

        if(getLastError() == FAILWRITEANDRUNREADACK){
            //Error #4: We missed some response.
            //		The receiveWriteAndRunAcks function has already set outputLength to the total length of the response and
            //		filled in requests for the missing part into outstandingPackets, outstandingReadStartAddresses and 
            //		outstandingReadLengths.  Thus, all we have to do is just resend them, if we have not retried too many times
            //		Once we get into this state, don't reenter the outer while loop.  At this point
            //		we can only return true, false with FAILWRITEANDRUNCAPACITY/FAILREADACK, or false with some fatal error.
            //		Stated another way, we don't want to try resending the entire write and run command again.
            setLastError(0);
				
            while(numRetries < maxRetries){
                LogIt("sirc::war.retries %u",numRetries);
                if (!resendOutstandingPackets(INVALIDREADTRANSMIT DEBUG_ONLY_2ARGS("WriteAndRun",&writeAndRunResends)))
                    return false;

                numRetries++;

                //Try to get the reads back
                if(receiveReadResponses(0, outData)){
                    //We got back all of the outstanding reads
                    if(okCapacity){
                        setLastError(0);
                        return true;
                    }
                    else{
                        setLastError(FAILWRITEANDRUNCAPACITY);
                        return false;
                    }	
                }

                //Verify that receiveReadData did not return false due to some error
                // rather then just not getting back all of the read responses we expected.
                MAYBE_BAILOUT();
            }

            //We have resent too many times
            PRINTF(("Write and run resent too many times without response!\n"));
            return bailOut(FAILWRITEANDRUNREADACK);
        }

        //Some other, unrecoverable problem might have occured.  In that case, we enter the normal
        // unrecoverable exit routine.
        return bailOut(getLastError());
    }

	//If we get this far, we tried resending the write & run command too many times because of FAILWRITEACK errors
	PRINTF(("Write and run resent too many times without response!\n"));
	return bailOut(FAILWRITEACK);
}

//Internal methods

#pragma intrinsic(_byteswap_ulong,_byteswap_ushort) //jic. And btw why don't we define BYTE_ORDER

//Allocate a packet for xmit, initialize state & locals
inline BOOL ETH_SIRC::allocateAndFillPacket(uint16_t length){
	//Get a new xmit packet to put the message in.
	currentPacket = PacketDriver->AllocatePacket(NULL,MAXPACKETSIZE,false);
	if(!currentPacket){
		setLastError(FAILMEMALLOC);
		return false;
	}

	//The packet payload will be N bytes long
	assert(length <= MAXPACKETDATASIZE);

	//The length of the frame will be the length of the payload plus 6 + 6 + 2 (dest MAC,
	//  source MAC, and payload length)
	currentPacket->nBytesAvail = length + 14;

	//Set the destination and source addresses of the packet (0-5 and 6-11)
	memcpy(currentPacket->Buffer, &ethHeader, 12);

	//The payload length field (bytes 12 and 13) does not include the length of the header
#if defined(_MSC_VER) //other compilers might not
    *(uint16_t*)(currentPacket->Buffer+12) = _byteswap_ushort(length);
#else
	currentPacket->Buffer[13] = (length) % 256;
	currentPacket->Buffer[12] = (length) >> 8;
#endif

	//Get the beginning of the packet payload (header is 14 bytes)
	currentBuffer = &(currentPacket->Buffer[14]);

    return true;
}

//Set the start address and write length fields (1-4 and 5-8)
inline void ETH_SIRC::setLengthAndAddress(uint32_t length, uint32_t address){
#if defined(_MSC_VER) //other compilers might not
    *(uint32_t*)(currentBuffer+1) = _byteswap_ulong(address);
    *(uint32_t*)(currentBuffer+5) = _byteswap_ulong(length);
#else
	for(int i = 3; i >=0; i--){
		currentBuffer[i + 1] = address % 256;
		currentBuffer[i + 5] = length % 256;
		address = address >> 8;
		length = length >> 8;
	}
#endif
}

//Set the value field (2-5)
inline void ETH_SIRC::setValueField(uint32_t value){
#if defined(_MSC_VER) //other compilers might not
    *(uint32_t*)(currentBuffer+2) = _byteswap_ulong(value);
#else
	for(int i = 3; i >=0; i--){
		currentBuffer[i + 2] = value % 256;
		value = value >> 8;
	}
#endif
}

//This function queues a receive on the network port
//Return true on success, return false w/error code on failure
inline BOOL ETH_SIRC::addReceive(PACKET *Packet){
    
    if (Packet)
        Packet->Length = MAXPACKETSIZE;//recycle
    else
        Packet = PacketDriver->AllocatePacket(NULL,MAXPACKETSIZE,true);
	if(!Packet){
		setLastError(FAILMEMALLOC);
		return false;
	}

    BIGDEBUG_adding_receive(Packet);

    PacketDriver->PostReceivePacket(Packet);

    return true;
}

//This function adds a transmit to the output queue and sends the message
//Return true if the send goes OK, return false if not.
//Don't bother with an error code, the function that calls this will take care of that.
inline BOOL ETH_SIRC::addTransmit(PACKET* Packet){
	HRESULT Result;

    BIGDEBUG_adding_transmit(Packet);

    Result = PacketDriver->PostTransmitPacket(Packet);

	if(Result != S_OK && Result != ERROR_IO_PENDING){
		PRINTF(("Bad transmission packet!\n"));
		return false;
	}

	return true;
}

//Send packet, check for errors
inline BOOL ETH_SIRC::sendCurrentPacket(int8_t errorCode, BOOL flushOutstanding, char *packetName){
	if(addTransmit(currentPacket))
        return true;
	PRINTF(("%s not sent!\n", packetName));
    if (flushOutstanding)
        emptyOutstandingPackets();
	setLastError(errorCode);
	return false;
}

//Flush outstanding packets and return with (optional) error
BOOL ETH_SIRC::bailOut(int8_t errorCode){
    emptyOutstandingPackets();
    if (errorCode)
        setLastError(errorCode);
    return false;
}

//The ack on the outstanding packets isn't coming, so put them into the free list
//To avoid all problems, we should clear the completion ports first.
//This will ensure that no uncompleted requests are outstanding that will complete
// later, after the packet has been freed.
void ETH_SIRC::emptyOutstandingPackets(){
	PACKET *        Packet;

	//Keep polling until it comes up empty
	for(;;){
        Packet = PacketDriver->GetNextReceivedPacket(readTimeout);
        if (Packet == NULL)
            break;

		//Some packet completed, too late.
        assert(Packet->Mode == PacketModeReceiving);
        BIGDEBUG_packet_received(Packet,1);

        //Received packets get re-posted immediately
        (void) addReceive(Packet);
    }

	//Now put the outstanding transmits into the free list
	for(packetIter = outstandingPackets.begin(); packetIter != outstandingPackets.end(); packetIter++){
        PacketDriver->FreePacket(*packetIter,false);
        //Decrement the outstanding packet counter
        outstandingTransmits --;
	}
    outstandingPackets.clear();

	//Empty the read starting address and length lists
	outstandingReadStartAddresses.clear();
	outstandingReadLengths.clear();
}

//Create a write request, add it to the back of the outstanding queue and transmit it.
//Return true if the addition & transmission goes OK.
//Return false w/error code if not.
BOOL ETH_SIRC::createWriteRequestBackAndTransmit(uint32_t startAddress, uint32_t length, uint8_t *buffer, BOOL flushQueue){

    LogIt("sirc::cwr %u %u",startAddress,length);

	/*The packet will be N + 9 bytes long: N payload + 1 byte command + 4 bytes address + 4 bytes length) */
    if (!allocateAndFillPacket(9+length))
        return false;

	//Set the command byte to 'w'
	currentBuffer[0] = 'w';

    setLengthAndAddress(length,startAddress);

	//Copy the write data over
	memcpy(currentBuffer + 9, buffer, length);

	//Keep track of this message
	outstandingPackets.push_back(currentPacket);
	outstandingTransmits++;

    currentPacket->Flush = flushQueue;
    return sendCurrentPacket(INVALIDWRITETRANSMIT,false DEBUG_ONLY_1ARG("Write"));
}

//Try and grab as many write acks that we can up till:
// 1) we get all of the outstanding writes acked, return true
// 2) we haven't gotten a new ack for N seconds (N should never be less than 1), return false
// 3) we have some problem on the completion port or addReceive, return false w/ error code
BOOL ETH_SIRC::receiveWriteAcks(){
	PACKET *        Packet;

	for(;;){
        Packet = PacketDriver->GetNextReceivedPacket(writeTimeout);
        if (Packet == NULL)
            break;

		//Some packet completed
        assert(Packet->Mode == PacketModeReceiving);
        BIGDEBUG_packet_received(Packet,0);

        //Check if this is a good write ack
        if(checkWriteAck(Packet)){
            //This is a good ack, repost the receive packet.
            if (addReceive(Packet)){

                //See if we have gotten all of the write acks back
                //If we have gotten all the writes acked, we are done for now
                if(outstandingTransmits == 0){
                    return true;
                }
                //Nope, keep going
                continue;
            }

            //Something went wrong posting a receive, bail out.
            LogIt("sirc::rwa.ar");
            return false;
        }
	
        //This isn't an ack of something we sent, but we should free the packet anyways.
        if (!addReceive(Packet)){
            LogIt("sirc::rwa.ar1");
            return false;
        }
    }

	//This return false is not an error per se, we just timed out
    LogIt("sirc::rwa.timeo");
	return false;
}

static UINT32 w32(uint8_t *w)
{
    return ((w[0] << 24) |
            (w[1] << 16) |
            (w[2] <<  8) |
            (w[3] <<  0));
}

//See if this write ack matches one that is outstanding
//If the packet matches one in the outstandingPacket list, return true.
//If not, return false.
inline BOOL ETH_SIRC::checkWriteAck(PACKET* packet){
    //LogIt("sirc::cwa %u %u", w32(packet->Buffer+15), w32(pcket->Buffer+15+4));
    return checkSimpleResponse(packet,'w',9);
}

//Create a read request, add it to the back of the outstanding queue and transmit it.
//Return true if transmission goes smoothly.
//Return false with error code if anything goes wrong.
BOOL ETH_SIRC::createReadRequestBackAndTransmit(uint32_t startAddress, uint32_t length){

	/*The packet will be 9 bytes long: 1 byte command + 4 bytes address + 4 bytes length) */
    if (!allocateAndFillPacket(9))
        return false;

	//Set the command byte to 'r'
	currentBuffer[0] = 'r';

    setLengthAndAddress(length,startAddress);

	//Keep track of this message
	outstandingPackets.push_back(currentPacket);
	outstandingReadStartAddresses.push_back(startAddress);
	outstandingReadLengths.push_back(length);
	outstandingTransmits++;

    return sendCurrentPacket(INVALIDREADTRANSMIT,true DEBUG_ONLY_1ARG("Read"));
}


//Create a read request just before the location currently pointed to by the various iterators
// in the outstandingPacket lists.
//When we are done, the iterators will point to the location just beyond the read request we just made
//Return true if the addition went smoothly.
//Return false w/error code if not.
BOOL ETH_SIRC::createReadRequestCurrentIterLocation(uint32_t startAddress, uint32_t length){

	/*The packet will be 9 bytes long: 1 byte command + 4 bytes address + 4 bytes length) */
    if (!allocateAndFillPacket(9))
        return false;

	//Set the command byte to 'r'
	currentBuffer[0] = 'r';

    setLengthAndAddress(length,startAddress);

	//Keep track of this message
	packetIter = outstandingPackets.insert(packetIter, currentPacket);
	startAddressIter = outstandingReadStartAddresses.insert(startAddressIter, startAddress);
	lengthIter = outstandingReadLengths.insert(lengthIter, length);

	incrementCurrIterLocation();
	outstandingTransmits++;
	return true;
}

// We have sent out one or more read requests (in strictly increasing addresses).
// The transmitted request packets are in outstandingPackets and the corresponding starting address
//	and length of the requests are in outstandingReadStartAddresses and outstandingReadLengths.
// We pass this function the initial start address of the entire read so that we know what the
//  offset should be within the buffer for subsequent read request replies.
// Try any grab as many read responses as we can till:
//	1) we get all of the reads back that we asked for, return true
//	2) we haven't gotten a new ack for N seconds (N should never be less than 1), return false
//		and outstandingPacket/ReadStartAddress/ReadLength will be loaded with the resends
// 3) we have some problem on the completion port or addReceive, return false w/ error code
BOOL ETH_SIRC::receiveReadResponses(uint32_t initialStartAddress, uint8_t *buffer){
	PACKET *        Packet;

	//Let's keep track of where we are in the list of outstanding packets.
	//These iterators will always point to the lowest-address request still outstanding
	// (that is, we have not seen a response to it, nor anything with a higher requested address than it).
	//Since this is a sorted list, any packet earlier in the list will only have requests
	// from lower addresses.
	packetIter = outstandingPackets.begin();
	startAddressIter = outstandingReadStartAddresses.begin();
	lengthIter = outstandingReadLengths.begin();

	//This is the starting address we are expecting
	uint32_t currAddress = 0;
	uint32_t currLength = 0;

	noResends = true;

	for(;;){
        Packet = PacketDriver->GetNextReceivedPacket(readTimeout);
        if (Packet == NULL)
            break;

		//Some packet completed
        assert(Packet->Mode == PacketModeReceiving);
        BIGDEBUG_packet_received(Packet,0);

        //Check if this is any read response packet we are expecting.
        //If it is, copy the data to the buffer, update the currAddress/currLength,
        // and update the outstanding packet list (removing or adding as necessary).
        if(checkReadData(Packet, &currAddress, &currLength, buffer, initialStartAddress)){

            //This is a good read response, so repost the packet
            if (addReceive(Packet)){
                //We know we are done if there are no more transmits outstanding and the
                // currLength == 0
                if(outstandingTransmits == 0 && currLength == 0){
                    return true;
                }
                //We know we are done for now if we are at the end of the outstanding queue and we have
                // currLength == 0).  No sense in waiting to time out, we know that we had some problems
                // and we want to resend.
                if(packetIter == outstandingPackets.end() && currLength == 0){
                    break;
                }

                //We are not done, keep going.
                continue;
            }

            //Something went wrong posting a receive, bail out.
            return false;
        }
        else{
            //This was not a good read response, so just free the packet and go back around
            int8_t err = getLastError();

            //repost the packet
            if (!addReceive(Packet)){
                return false;
            }

            if(err != 0){
                //checkReadData had some sort of problem, so return
                setLastError(err);
                return false;
            }
        }
    }

	//We timed out. Any outstanding requests still in the outstanding list should be
	// re-sent.  This will be taken care of when we return from this function.
	//However, we have to add a re-send request for any remaining part of the current request, if any.
	//We have a current request if currLength is not zero.
	if(currLength != 0){
		//Add a new request before the current location.
		if(!createReadRequestCurrentIterLocation(currAddress, currLength)){
			return false;
		}
	}
	return false;
}

//This function looks at the packet we have been sent and determines if the packet
//	is a response to any of the outstanding read requests we have.
//If it it not a response to a read request, we will return false.
//If it is a response, we copy the data to the buffer in the correct location, update 
// currAddress & currLength, and add or remove any necessary read requests from the
// outstanding list.  If this goes OK, we will return true.  If anything goes wrong
// we will return false with an error code.
BOOL ETH_SIRC::checkReadData(PACKET* packet, uint32_t* currAddress, uint32_t* currLength, 
							 uint8_t* buffer, uint32_t initialStartAddress){
	uint8_t *message = packet->Buffer;

	uint32_t dataLength;
	uint32_t startAddress;
	int i;

	//When we enter this function, packetIter will always be pointing at a request for which 
	// we have not seen any responses.  This is because as soon as we see any 
	// response from a given request, we remove it from the list.
	//First, see if this is a valid read response
	//See if the packet is from the expected source
    if (memcmp(message+6,ethHeader.FPGA_MACAddress,6) != 0)
        return false;

	//Get the length of the packet
	dataLength = message[12];
	dataLength = (dataLength << 8) + message[13];
	//This packet must be at least 6 bytes long (1 byte command + 4 bytes address + 1 data byte)
	if(dataLength < 6){
		return false;
	}

	//Check the command byte
	if(message[14] != 'r')
		return false;

	//Get the start address
	startAddress = 0;
	for(i = 0; i < 4; i++){
		startAddress = startAddress << 8;
		startAddress += message[15 + i];
	}

	//This is probably a valid read response, let's try to match it up
	for(;;){
        //If currLength == 0, the request at packetIter is a new one and we are hoping to get
        //		responses for it.  currAddress does not have any meaningful value in it yet.
		if(*currLength == 0){
			//	If this is the case, we should update currAddress and currLength with the
			//		values from the request at packetIter.
			//See if we are all out of requests.
			if(lengthIter == outstandingReadLengths.end()){
				//This only happens when we get responses beyond of the range of the requests we have queued up
				//If this happens, something is wrong, so just toss out the packet
				return false;
			}

			*currLength = *lengthIter;
			*currAddress = *startAddressIter;
			
			//	Now, there are a few things that can happen:
			//	1) we get a response for the beginning of the request at packetIter
			if(startAddress == *currAddress){
                LogIt("sirc::crd0 %u %u",startAddress,dataLength-5);
				//	a) mark the packet acked
				markPacketAcked(*packetIter);
				//	b) remove the packet at from the outstanding list
				removeReadRequestCurrentIterLocation();
				//	b) copy over the received data to the buffer
				memcpy(buffer+(startAddress - initialStartAddress), message + 19, dataLength - 5);
				//	c) update currLength & currAddress
				*currLength -= dataLength - 5;
				*currAddress += dataLength - 5;
				return true;
			}
			//	2) we get a response for the middle of the request at packetIter (we missed some
			//		data for the beginning of the request)
			if(startAddress > *currAddress && startAddress < *currAddress + *currLength){
                LogIt("sirc::crd1 %u %u",startAddress,dataLength-5);
				noResends = false;
				//	a) mark the packet acked
				markPacketAcked(*packetIter);
				//	b) remove the packet at from the outstanding list
				removeReadRequestCurrentIterLocation();				
				//	c) create and insert a new read request for the missing piece
				if(!createReadRequestCurrentIterLocation(*currAddress, startAddress - *currAddress)){
					return false;
				}
				//	d) copy over the data
				memcpy(buffer+(startAddress - initialStartAddress), message + 19, dataLength - 5);
				//	e) update currLength & currAddress
				*currLength -= (startAddress - *currAddress) + dataLength - 5;
				*currAddress = startAddress + dataLength - 5;
				return true;
			}
			//	3) we get a response for an interval beyond the end of the request at packetIter
			//		(we missed responses for the entire request)
			if(startAddress >= *currAddress + *currLength){
                LogIt("sirc::crd2 %u %u ???",startAddress,dataLength-5);
				noResends = false;
				//	a) increment packetIter (leave the old request in the list)
				incrementCurrIterLocation();
				//	b) set currLength = 0 (indicate we are trying to consider a new request)
				*currLength = 0;
				//	c) go back to the start of the function
				continue;
			}
			// 4) we get a response for an interval before the request at packetIter.
			//		In this case, something has gone wrong (perhaps a delay in the network?)
			//		Either way, just toss out the packet
            return false;
		}

		//If currLength != 0, we are hoping to get a response at currAddress, a part of a request
		//		already started.

        //	There are a few things that can happen here:
        //	1) we get a response for right at currAddress (typical)
        if(startAddress == *currAddress){
            //	a) copy over the received data to the buffer
            LogIt("sirc::crd00 %u %u",startAddress,dataLength-5);
            memcpy(buffer+(startAddress - initialStartAddress), message + 19, dataLength - 5);
            //	b) update currLength & currAddress
            *currLength -= dataLength - 5;
            *currAddress += dataLength - 5;
            return true;
        }
        //	2) we get a response for the middle between currAddress and currLength (we missed some data
        //		for the beginning of the request)
        if(startAddress > *currAddress && startAddress < *currAddress + *currLength){
            LogIt("sirc::crd01 %u %u",startAddress,dataLength-5);
            noResends = false;
            //	a) create and insert a new read request for the missing piece
            if(!createReadRequestCurrentIterLocation(*currAddress, startAddress - *currAddress)){
                return false;
            }
            //	b) copy over the data
            memcpy(buffer+(startAddress - initialStartAddress), message + 19, dataLength - 5);
            //	c) update currLength & currAddress
            *currLength -= (startAddress - *currAddress) + dataLength - 5;
            *currAddress = startAddress + dataLength - 5;
            return true;
        }
        // 3) we get a response for an interval beyond the end of the current request
        //		(we missed the response for the remainder of the current request)
        if(startAddress >= *currAddress + *currLength){
            LogIt("sirc::crd02 %u %u ????",startAddress,dataLength-5);
            noResends = false;
            //	a) create and insert a new read request for the missing piece of the current request
            if(!createReadRequestCurrentIterLocation(*currAddress, *currLength)){
                return false;
            }
            //	b) set currLength to zero to indicate we are done with the current request
            *currLength = 0;
            //	c) go back to the start of the function
            continue;
        }
        // 4) we get a response for an interval before the current request
        //		In this case, something has gone wrong (perhaps a delay in the network?)
        //		Either way, we have probably created a new read request for this packet already,
        //			
        //		Either way, just toss out the packet
        return false;
	}
}

//Create a register write request, add it to the back of the outstanding queue and transmit it.
//Return true if the addition & transmission goes OK.
//Return false w/error code if not.
BOOL ETH_SIRC::createParamWriteRequestBackAndTransmit(uint8_t regNumber, uint32_t value){
	//The packet will be 6 bytes long (1 byte command + 1 byte address + 4 bytes length)
    if (!allocateAndFillPacket(6))
        return false;

	//Set the command byte to 'k'
	currentBuffer[0] = 'k';

	//Copy the register address over
	currentBuffer[1] = regNumber;
	
	//Copy the value over
    setValueField(value);

	//Keep track of this message
	outstandingPackets.push_back(currentPacket);
	outstandingTransmits++;

    return sendCurrentPacket(INVALIDPARAMWRITETRANSMIT,false DEBUG_ONLY_1ARG("Param write"));
}

//See if this param write ack matches the one that is outstanding
//If the packet matches the one in the outstandingPacket list, return true.
//If not, return false.
BOOL ETH_SIRC::checkParamWriteAck(PACKET* packet, uint32_t *unused){
    return checkSimpleResponse(packet, 'k', 6);
}

//Create a register read request, add it to the back of the outstanding queue and transmit it.
//Return true if the addition & transmission goes OK.
//Return false w/error code if not.
BOOL ETH_SIRC::createParamReadRequestBackAndTransmit(uint8_t regNumber){

	//The packet will be 2 bytes long (1 byte command + 1 byte address)
    if (!allocateAndFillPacket(2))
        return false;

	//Set the command byte to 'y'
	currentBuffer[0] = 'y';

	//Copy the register address over
	currentBuffer[1] = regNumber;

	//Keep track of this message
	outstandingPackets.push_back(currentPacket);
	outstandingTransmits++;

    return sendCurrentPacket(INVALIDPARAMREADTRANSMIT,false DEBUG_ONLY_1ARG("Param read"));
}


//See if this param write ack matches the one that is outstanding
//If the packet matches the one in the outstandingPacket list, return true.
//If not, return false.
BOOL ETH_SIRC::checkParamReadData(PACKET* packet, uint32_t *value){
    return checkResponseWithValue(packet,value, 'y');
}

//Create a write and run request, add it to the back of the outstanding queue and transmit it.
//Return true if the addition & transmission goes OK.
//Return false w/error code if not.
BOOL ETH_SIRC::createWriteAndRunRequestBackAndTransmit(uint32_t startAddress, uint32_t length, uint8_t *buffer){

	/*The packet will be N + 9 bytes long: N payload + 1 byte command + 4 bytes address + 4 bytes length) */
    if (!allocateAndFillPacket(9+length))
        return false;

	//Set the command byte to 'g'
	currentBuffer[0] = 'g';

    setLengthAndAddress(length,startAddress);

	//Copy the write data over
	memcpy(currentBuffer + 9, buffer, length);

	//Keep track of this message
    //It will be handled specially though (in receiveWriteAndRunAcks)
	outstandingPackets.push_back(currentPacket);
	outstandingTransmits++;

    return sendCurrentPacket(INVALIDWRITEANDRUNTRANSMIT,false DEBUG_ONLY_1ARG("Write and run"));
}

// We have sent out a write and run command.
// There are 5 possible situation that can occur
// 1) We get all of the expected responses back and they fit in output buffer
//		Set outputLength to the number of bytes we are returning and return true
// 2) We get no response at all
//		Set lastError to FAILWRITEACK and return false.
// 3) We get all of the expected responses back, but they don't fit in the output buffer
//		Set lastError to FAILWRITEANDRUNCAPACITY, set outputLength to the total length of the response and return false
// 4) We miss some reponse, regardless of whether or not it would fit in the output buffer
//		Set lastError to FAILREADACK, set outputLength to the total length of the response, put the requests for the
//		missing parts (except those that wouldn't fit in the output buffer) into outstandingPackets, 
//		outstandingReadStartAddresses and outstandingReadLengths, and return false
// 5) We have some other technical problem like the other receiveXXX functions.
//		Set lastError appropriately and return false.
BOOL ETH_SIRC::receiveWriteAndRunAcks(uint32_t maxWaitTimeInMsec, uint32_t maxOutLength, uint8_t *buffer, uint32_t *outputLength){
	PACKET *        Packet;

	//Let's keep track of where we are in the list of outstanding packets.

    //If we hear anything at all the 'g' command was received.
    //That implicitly acks the corresponding xmit packet.
    bool firstPacket = true;//outstandingPackets.front()?

	//We will add requests, though, as we miss packets.
	packetIter = outstandingPackets.begin();
	startAddressIter = outstandingReadStartAddresses.begin();
	lengthIter = outstandingReadLengths.begin();

	//This is the starting address we are expecting
	uint32_t currAddress = 0;
	//This is the remaining number of bytes we are expecting
	uint32_t currLength = 0;

	noResponse = true;

	for(;;){
        Packet = PacketDriver->GetNextReceivedPacket(maxWaitTimeInMsec);
        if (Packet == NULL)
            break;

		//Some packet completed
        assert(Packet->Mode == PacketModeReceiving);
        BIGDEBUG_packet_received(Packet,0);

        //Check if this is any read response packet we are expecting.
        //If it is, copy the data to the buffer, update the currAddress/currLength/outputLength,
        // and add missed reads if necessary.
        if(checkWriteAndRunData(Packet, &currAddress, &currLength, buffer, outputLength, maxOutLength)){

			//This is a good read response, therefore the command was heard.
	        //That is the packet at the head of the queue, free it now.
	        if (firstPacket) {
		        firstPacket = false;
			    markPacketAcked(*packetIter);
				outstandingPackets.erase(packetIter);
				packetIter = outstandingPackets.begin();
			}

            if (addReceive(Packet)){

                //Are we expecting any more packets?
                //We are not if the currLength is zero.  If we got here we have seen at least 1 good packet,
                //	so we can only be exiting with the case that: 
                //	1) we saw all of the packets we wanted to and the output fit 
                //		(lastError == 0 and we return true)
                //	2) we saw all of the packets we wanted to, but and the output did not fit
                //		(lastError == FAILWRITEANDRUNCAPACITY and we return false)
                //	3) we missed at least one packet somewhere down the line, regardless if the output could fit
                //		or not (lastError == FAILREADACK and we return false)
                if(currLength == 0){
                    return(getLastError() == 0);
                }

                //We want more.
                continue;
            }

            //Something went wrong posting a receive, bail out.
            return false;
        }

        //This was not a good read response, so just free the packet and go back around
        int8_t err = getLastError();

        if (!addReceive(Packet)){
            return false;
        }
        if(err != 0 && err != FAILWRITEANDRUNREADACK && err != FAILWRITEANDRUNCAPACITY){
            //checkWriteAndRunData had some sort of serious problem, so return
            return false;
        }
    }

    //If we saw no response at all we must free that xmit packet now.
    if (firstPacket) {
        markPacketAcked(*packetIter);
        outstandingPackets.erase(packetIter);
    }
	//packetIter = outstandingPackets.begin();//quick hack: BUGBUG recheck

	//We timed out.
	//Have we seen any response yet?  If not, let's return a FAILWRITEACK error
	if(noResponse){
		setLastError(FAILWRITEACK);
		return false;
	}
	
	//We have seen a response, but we were anticipating more packets.
	//Let's add a read request for the remaining part
	if(!createReadRequestCurrentIterLocation(currAddress, currLength)){
		return false;
	}
	//This return false is not an error per se, we just missed some packets and we'll have to send new read requests
	setLastError(FAILWRITEANDRUNREADACK);
	return false;
}

//This function looks at the packet we have been sent and determines if the packet
//	is write and run command response.
//If it it not a write and run command response, we will return false.
//If it is a response, we:
//		1) check to see if this is the first response we have seen
//				(If so, setup outputLength and currLength, maybe set 
//				lastError = FAILWRITEANDRUNCAPACITY.  If not, double-check outputLength 
//				against current message parameters.  Either way, set okCapacity.)
//		2) determine if we missed any packets
//				(if so, add any necessary read requests to the outstanding packet list 
//				and set lastError = FAILREADACK)
//		3) copy the data to the buffer in the correct location and update 
//				currAddress & currLength.  
//In all but some fatal error case, we will return true.  We might set an error code, but
//		the function will still return true.
BOOL ETH_SIRC::checkWriteAndRunData(PACKET* packet, uint32_t* currAddress, uint32_t* currLength,  
									uint8_t* buffer, uint32_t *outputLength, uint32_t maxOutLength){
	uint8_t *message = packet->Buffer;

	uint32_t dataLength;
	uint32_t startAddress;
	uint32_t remainingLength;
	int i;

	//First, see if this is a valid read response
	//See if the packet is from the expected source
    if (memcmp(message+6,ethHeader.FPGA_MACAddress,6) != 0)
        return false;

	//Get the length of the packet
	dataLength = message[12];
	dataLength = (dataLength << 8) + message[13];
	//This packet must be at least 10 bytes long (1 byte command + 4 bytes address + 4 bytes remaining # bytes + 1 data byte)
	if(dataLength < 10){
		return false;
	}

	//Check the command byte
	if(message[14] != 'g')
		return false;

	//Get the start address
	startAddress = 0;
	for(i = 0; i < 4; i++){
		startAddress = startAddress << 8;
		startAddress += message[15 + i];
	}

	//Get the remaining length
	remainingLength = 0;
	for(i = 0; i < 4; i++){
		remainingLength = remainingLength << 8;
		remainingLength += message[19 + i];
	}

	//This is some sort of valid read response, so:
	//		1) check to see if this is the first response we have seen
	//				(If so, setup outputLength and currLength, maybe set 
	//				lastError = FAILWRITEANDRUNCAPACITY.  If not, double-check outputLength 
	//				against current message parameters.  Either way, set okCapacity.
	if(noResponse){
		noResponse = false;
		*outputLength = startAddress + remainingLength;

		if(*outputLength > maxOutLength){
			*currLength = maxOutLength;
			setLastError(FAILWRITEANDRUNCAPACITY);
			okCapacity = false;
		}
		else{
			*currLength = *outputLength;
			okCapacity = true;
		}
	}
	else if(*outputLength != startAddress + remainingLength){
		setLastError(INVALIDWRITEANDRUNRECIEVE);
		return false;
	}

	//		2) determine if we missed any packets
	//				(if so, add any necessary read requests to the outstanding packet list 
	//				and set lastError = FAILREADACK)
	if(startAddress > *currAddress){
		//We missed some packets, so add a read request for the missing ones
		//Notice, we might have already had the FAILWRITEANDRUNCAPACITY error, but we are
		//		now switching to the FAILREADACK error.
		//If everything comes back normally after the re-send, we will reinstate the 
		//		FAILWRITEANDRUNCAPACITY if necessary
		setLastError(FAILWRITEANDRUNREADACK);
		if(!createReadRequestCurrentIterLocation(*currAddress, startAddress - *currAddress)){
			return false;
		}
		*currLength -= startAddress - *currAddress;
	}
	else if(startAddress < *currAddress){
		//This is data we were expecting earlier.  We have already put in another
		//	request for it, so just ignore this packet.
		return false;
	}

	//		3) copy the data to the buffer in the correct location and update 
	//				currAddress & currLength.
	memcpy(buffer + startAddress, message + 23, min(*currLength, dataLength - 9));
	*currAddress = startAddress + dataLength - 9;
	*currLength -= min(*currLength, dataLength - 9);

	return true;
}


//Create a reset request, add it to the back of the outstanding queue and transmit it.
//Return true if the addition & transmission goes OK.
//Return false w/error code if not.
BOOL ETH_SIRC::createResetRequestAndTransmit(){

	//The packet will be 1 bytes long (1 byte command)
    if (!allocateAndFillPacket(1))
        return false;

	//Set the command byte to 'm'
	currentBuffer[0] = 'm';

	//Keep track of this message
	outstandingPackets.push_back(currentPacket);
	outstandingTransmits++;

    return sendCurrentPacket(INVALIDRESETTRANSMIT,false DEBUG_ONLY_1ARG("Reset"));
}


//See if this reset ack matches the one that is outstanding
//If the packet matches the one in the outstandingPacket list, return true.
//If not, return false.
BOOL ETH_SIRC::checkResetAck(PACKET* packet, uint32_t *unused){
    return checkSimpleResponse(packet,'m',1);
}

//Generic method for receiving and checking a response packet
BOOL ETH_SIRC::receiveGenericAck(uint32_t timeOut, uint32_t *arg2, BOOL (ETH_SIRC::*checkFunction)(PACKET*,uint32_t *),int errorCode){
	PACKET *        Packet;

	for(;;){
        Packet = PacketDriver->GetNextReceivedPacket(timeOut);
        if (Packet == NULL)
            break;

		//Some packet completed
        assert(Packet->Mode == PacketModeReceiving);
        BIGDEBUG_packet_received(Packet,0);

        //Check if this is a good response
        if((this->*checkFunction)(Packet, arg2)){
            //This is a good response, repost the packet.
            if (addReceive(Packet)){
                //We have gotten the response we wanted, so we are done
                return true;
            }

            //Something went wrong posting a receive, bail out.
            return false;
        }
	
        //This isn't a response to something we sent, but we should free the packet anyways.
        if (!addReceive(Packet)){
            return false;
        }
    }

	//We did not receive the ack we wanted, error out.
    setLastError(errorCode);
	return false;

}

//See if this packet matches the register read that is outstanding
//If the packet matches the one in the outstandingPacket list, return true.
//If not, return false.
BOOL ETH_SIRC::checkResponseWithValue(PACKET *packet, uint32_t *value, uint8_t commandCode)
{
	uint8_t *message;
	uint8_t *testMessage;

	PACKET *testPacket;

	message = packet->Buffer;

	//See if the packet is from the expected source
    if (memcmp(message+6,ethHeader.FPGA_MACAddress,6) != 0)
        return false;

	//See if the packet is the correct length
	//This should be exactly 6 bytes long (command byte + reg address + value)
	if(message[12] != 0 || message[13] != 6)
		return false;

	//Check the command byte
	if(message[14] != commandCode)
		return false;

	//So far, so good - let's try to match this against the one outstanding write
	testPacket = outstandingPackets.front();
	testMessage = testPacket->Buffer;

	//Check if we recognize reg address
	if(message[15] == testMessage[15]){
        BIGDEBUG_packet_matched(testPacket);
		//Copy the value over
		*value = 0;
		for(int i = 0; i < 4; i++){
			*value += message[i + 16] << (3 - i) * 8;
		}

		//We matched a transmission, so see if that command was completed already.
        markPacketAcked(testPacket);

		//remove this from the outstanding packets
		outstandingPackets.pop_front();

		return true;
	}

	return false;
}

//See if this message matches a register register write ack that is outstanding
//If the packet matches the one in the outstandingPacket list, return true.
//If not, return false.
BOOL ETH_SIRC::checkSimpleResponse(PACKET *packet, uint8_t commandCode, uint8_t length)
{
	uint8_t *message;
	uint8_t *testMessage;

	message = packet->Buffer;

	//See if the packet is from the expected source
    if (memcmp(message+6,ethHeader.FPGA_MACAddress,6) != 0)
        return false;

	//See if the packet is the correct length
	//This should be exactly length bytes long
	if(message[12] != 0 || message[13] != length)
		return false;

	//So far, so good - let's try to match this against one of the outstanding requests
	//We add the newest packets sent to the end of the list, so the oldest (and likely first to be acked)
	// packets should be near the front.
	for(packetIter = outstandingPackets.begin(); packetIter != outstandingPackets.end(); packetIter++){
        PACKET *testPacket = *packetIter;

		testMessage = testPacket->Buffer;

        //Check if we recognize the command byte, and however more bytes we need (reg address & value, ...)
        if (memcmp(message+14,testMessage+14,length) == 0){

            BIGDEBUG_packet_matched(testPacket);
            //We matched a transmission, so see if that command was completed already.
            markPacketAcked(testPacket);

            //remove this from the outstanding packets
            outstandingPackets.erase(packetIter);

            return true;
        }
	}

	return false;
}

//Common function to handle retransmissions
BOOL ETH_SIRC::resendOutstandingPackets(int errorCode, char *callerName, int *counter){
    for(packetIter = outstandingPackets.begin(); packetIter != outstandingPackets.end(); packetIter++){
        PACKET *packet = *packetIter;

        //Increment the proper debug counter
        DEBUG_ONLY(*counter++;);

        //Log the event
        LogIt("sirc::resend %p",(UINT_PTR)packet);

        //Retransmit now
        if(!addTransmit(packet)){
            //We are in serious trouble.
            PRINTF(("%s not sent!\n",callerName));
            return bailOut(errorCode);
        }
    }
    return true;
}

void ETH_SIRC::incrementCurrIterLocation(){
	assert(packetIter != outstandingPackets.end());
	packetIter++;
	assert(startAddressIter != outstandingReadStartAddresses.end());
	startAddressIter++;
	assert(lengthIter != outstandingReadLengths.end());
	lengthIter++;
}

void ETH_SIRC::removeReadRequestCurrentIterLocation(){
	packetIter = outstandingPackets.erase(packetIter);
	startAddressIter = outstandingReadStartAddresses.erase(startAddressIter);
	lengthIter = outstandingReadLengths.erase(lengthIter);
}

//Mark this packet acked and free it if the transmission has been completed.
inline void ETH_SIRC::markPacketAcked(PACKET* packet){
    assert((packet->Mode == PacketModeTransmitting) ||
           (packet->Mode == PacketModeTransmittingBuffer));
    //We have seen a response from the read request, free the transmission packet.
    PacketDriver->FreePacket(packet,false);

    //Decrement the outstanding packet counter
    outstandingTransmits --;

}

void ETH_SIRC::printPacket(PACKET* packet){
    DEBUG_ONLY(
    {
        unsigned int i;
	
        printf("Packet contents:\n");
        printf("\t%18s%18s%6s[Data]\n\t", "[Dest Address]", "[Src Address]","[Len]");
        for(i = 0; i < packet->nBytesAvail; i++){
            printf("%02x ", packet->Buffer[i]);	
        }
        printf("\n");
    });
}
