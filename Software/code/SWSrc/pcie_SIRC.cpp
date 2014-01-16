// Title: PCIE_SIRC class definition
// 
// Description: 
// Host software to send and receive packets over PCI Express
// Based on code from Ray Bittner - raybit@microsoft.com
// Read and write to input/output buffers and parameter register file.  Also, 
// start execution, wait until execution is completed and (maybe) reconfigure the
// device.
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

#include "sirc_internal.h"
#include "initguid.h"

//Memory offset of the input side of the memory (it's all one memory, but the hardware writes to different locations)
#define INPUT_OFFSET 0x00000000

//Memory offset of the output side of the memory (it's all one memory, but the hardware writes to different locations)
#define OUTPUT_OFFSET 0x08000000

//Memory offset of parameter register file
#define PARAMETER_REG_OFFSET 0xF0000000

//This is a hack - we should limit the size of the buffer in a smarter way
//For now, I'm making it 128MB
//BUGBUG This needs some serious rethinking.
#define MAX_BUFFER_LENGTH 1024*1024*128

//
// Define an Interface Guid for toaster device class.
// This GUID is used to register (IoRegisterDeviceInterface) 
// an instance of an interface so that user application 
// can control the toaster device.
//

DEFINE_GUID (GUID_DEVINTERFACE_TOASTER, 
        0xF9FBD7F4, 0xDBB9, 0x4a0b, 0x91, 0x80, 0x42, 0xAF, 0x7F, 0xE0, 0x34, 0x74);
//{F9FBD7F4-DBB9-4a0b-9180-42AF7FE03474}

//
// Define a WMI GUID to get toaster device info.
//

DEFINE_GUID (TOASTER_WMI_STD_DATA_GUID, 
		0x59ADB7E9L, 0x8A1B, 0x45c3, 0x84, 0x47, 0x1A, 0x7D, 0x15, 0xC3, 0x20, 0x37);
// {59ADB7E9-8A1B-45c3-8447-1A7D15C32037}

//
// Define a WMI GUID to represent device arrival notification WMIEvent class.
//

DEFINE_GUID (TOASTER_NOTIFY_DEVICE_ARRIVAL_EVENT, 
		0xA8F47227, 0x3299, 0x407c, 0x84, 0x74, 0xF6, 0x3D, 0xE8, 0xE2, 0xBC, 0x29);
// {A8F47227-3299-407c-8474-F63DE8E2BC29}


SIRC_DLL_LINKAGE PCIE_SIRC::PCIE_SIRC(int desiredInstance)
{
    wholeWordBuffer = NULL;
    hFile = NULL;
	hFile = INVALID_HANDLE_VALUE;
	if(!FindDevice(desiredInstance)){
        setLastError( FAILDRIVERPRESENT);
        return;
	}

    maxInputDataBytes = MAX_BUFFER_LENGTH;
    maxOutputDataBytes = MAX_BUFFER_LENGTH;

	wholeWordBuffer = (uint8_t*)VirtualAlloc(NULL, MAX_BUFFER_LENGTH, MEM_RESERVE | MEM_PHYSICAL, PAGE_EXECUTE_READWRITE);
	wholeWordBuffer = (uint8_t *)VirtualAlloc(NULL, MAX_BUFFER_LENGTH, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
}

PCIE_SIRC::~PCIE_SIRC()
{
	if (hFile != NULL)
		CloseHandle( hFile );

    if (wholeWordBuffer) {
        VirtualFree (wholeWordBuffer, MAX_BUFFER_LENGTH, MEM_DECOMMIT);
        VirtualFree (wholeWordBuffer, 0, MEM_RELEASE);
    }
}

//Dynamic parameters
//Retrieve the active set of parameters and limits for this instance
BOOL PCIE_SIRC::getParameters(SIRC::PARAMETERS *outParameters, uint32_t maxOutLength)
{
    SIRC::PARAMETERS params;

    params.myVersion            = SIRC_PARAMETERS_CURRENT_VERSION;
    params.maxInputDataBytes    = maxInputDataBytes;
    params.maxOutputDataBytes   = maxOutputDataBytes;
    params.writeTimeout         = 0; // unlimited
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
BOOL PCIE_SIRC::setParameters(const SIRC::PARAMETERS *inParameters, uint32_t length)
{
    //Sometimes you got to know what you are doing.
    if ((length < sizeof(*inParameters)) ||
        (inParameters->myVersion < SIRC_PARAMETERS_CURRENT_VERSION)){
        setLastError(INVALIDLENGTH);
        return false;
    }

    //Ignored: maxOutstandingReads  = inParameters->maxOutstandingReads;
    //Ignored: maxOutstandingWrites  = inParameters->maxOutstandingWrites;
    
    //BUGBUG: Should reallocate unaligned buffer..which is a bad idea anyways..
    maxInputDataBytes    = inParameters->maxInputDataBytes;
    maxOutputDataBytes   = inParameters->maxOutputDataBytes;

    //Ignored: writeTimeout         = inParameters->writeTimeout;
    //Ignored: readTimeout          = inParameters->readTimeout;
    //Ignored: maxRetries           = inParameters->maxRetries;

    setLastError(0);
    return true;
}


void PCIE_SIRC::PrintError(char *pszRoutineName, char *pszComment)
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

	printf( "PCIE_SIRC->%s() Error %s: 0x%X - %S\n", pszRoutineName, pszComment, GetLastError(), lpMessage );
	LocalFree( lpMessage );
#endif
}

bool PCIE_SIRC::FindDevice(int instance)
{
	HDEVINFO hDeviceInfoSet;
	int iIndex;
	SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
	SP_DEVICE_INTERFACE_DETAIL_DATA *pDeviceInterfaceDetailData;
	DWORD dwStructureSize;

	// Get a handle to the device information set, which will have info on every device that is present under the
	//	media class GUID
	hDeviceInfoSet = SetupDiGetClassDevs(
		(LPGUID)&GUID_DEVINTERFACE_TOASTER, // Only get interfaces for my GreedyCAM Memory card
		NULL, // No enumerator filter string
		NULL, // No pointer to a top level window for UI
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE // Return only devices that are present, and that match this interface GUID
	);

	if(hDeviceInfoSet == INVALID_HANDLE_VALUE)
	{
		PrintError( "FindDevices", "SetupDiGetClassDevs() Found No Media Class Devices" );
		return false;
	}

#ifdef BIGDEBUG
    printf("Device Interfaces Detected\n");
    printf("--------------------------\n");
#endif

	// Attempt to get the context structure for one of the device interfaces in the set
	iIndex = 0;
	pDeviceInterfaceDetailData = NULL;
	DeviceInterfaceData.cbSize = sizeof( DeviceInterfaceData ); // Have to do this before the call, bizarre
	while(SetupDiEnumDeviceInterfaces(
		hDeviceInfoSet,							// All Media class device interfaces
		NULL,									// No constraint on which instance is returned
		(LPGUID)&GUID_DEVINTERFACE_TOASTER,		// Look for my devices
		iIndex,									// Not sure what this indexes through yet
		&DeviceInterfaceData					// Structure that will contain the returned info
	))
	{
		if (pDeviceInterfaceDetailData != NULL)
			free( pDeviceInterfaceDetailData );

		// To get the detailed information, I first need to figure out what sized buffer I need
		SetupDiGetDeviceInterfaceDetail(
			hDeviceInfoSet,						// Handle to this device class
			&DeviceInterfaceData,				// This specific device in the class
			NULL,								// Set to NULL to get the variable structure size
			0,									// ...
			&dwStructureSize,					// Returned structure size
			NULL
		);
		// Make sure the call failed appropriately, which means that the dwStructureSize will be valid
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			PrintError( "FindDevices", "SetupDiGetDeviceInterfaceDetail() structure size query failed" );
			SetupDiDestroyDeviceInfoList( hDeviceInfoSet );
			return false;
		}

		// Now I know how big the buffer actually has to be
		pDeviceInterfaceDetailData = (SP_DEVICE_INTERFACE_DETAIL_DATA *)malloc( dwStructureSize );
		if (pDeviceInterfaceDetailData == NULL) {
			PrintError( "FindDevices", "SetupDiGetDeviceInterfaceDetail() Couldn't allocate space for pDeviceInterfaceDetailData" );
			SetupDiDestroyDeviceInfoList( hDeviceInfoSet );
			return false;
		}

		// Now call the function again using the correct size to allow for the variable length string
		pDeviceInterfaceDetailData->cbSize = sizeof( SP_DEVICE_INTERFACE_DETAIL_DATA );
		if (!SetupDiGetDeviceInterfaceDetail(
			hDeviceInfoSet,						// Handle to this device class
			&DeviceInterfaceData,				// This specific device in the class
			pDeviceInterfaceDetailData,			// Pointer to the output structure
			dwStructureSize,					// The size of the buffer that I'm passing in
			&dwStructureSize,					// The size that the buffer needs to be will be returned (should be the same)
			NULL								// It can return an SP_DEVINFO_DATA structure here
		))
		{
			PrintError( "FindDevices", "SetupDiGetDeviceInterfaceDetail() detail data query failed" );
			SetupDiDestroyDeviceInfoList( hDeviceInfoSet );
			return false;
		}

#ifdef BIGDEBUG
        printf("%d) %S\n", iIndex, pDeviceInterfaceDetailData->DevicePath );
#endif

        if ((instance >= 0) && (iIndex == instance))
            goto FoundIt;
		iIndex++;
	}

	// Make sure that SetupDiEnumDeviceInterfaces() failed for the right reason
	if (GetLastError() != ERROR_NO_MORE_ITEMS)
	{
		PrintError( "FindDevices", "SetupDiEnumDeviceInterfaces()" );
		SetupDiDestroyDeviceInfoList( hDeviceInfoSet );
		return false;
	}

 FoundIt:
	SetupDiDestroyDeviceInfoList( hDeviceInfoSet );

	// Work with the last 
	if (pDeviceInterfaceDetailData != NULL)
	{
		hFile = CreateFile ( pDeviceInterfaceDetailData->DevicePath,
			GENERIC_READ | GENERIC_WRITE,	// Open for reading and writing
			0,								// Cannot be shared
			NULL,							// No SECURITY_ATTRIBUTES structure, so no inheritance by child processes
			OPEN_EXISTING,					// No special create flags
			0,								// No special attributes
			NULL							// No template file whose attributes should be copied
		);
		free( pDeviceInterfaceDetailData );
		if (hFile != INVALID_HANDLE_VALUE)
			return true;
		else
			PrintError( "FindDevices", "CreateFile()" );
	}

	return false;
}

//Reads from the PCIe need to start word-aligned (startAddress must be a multiple of 4)
// and the length of the read must also be a multiple of 4.
//We have to fix things up if this is not the case.
//Start Address		Length offset			
//address offset	0	1	2	3
//				0	0	0	0	0
//				1	-1	-1	-1	-1
//				2	-2	-2	-2	-2
//				3	-3	-3	-3	-3	
//Length			Length offset			
//address offset	0	1	2	3
//				0	0	+3	+2	+1
//				1	+4	+3	+2	+1
//				2	+4	+3	+2	+1
//				3	+4	+3	+2	+1
BOOL PCIE_SIRC::sendRead(uint32_t startAddress, uint32_t length, uint8_t *readBackData)
{
	DWORD dwBytesRead, dwTotalBytesRead;
	OVERLAPPED OverlapStructure;
	int lengthWholeWord;
	int startAddressWholeWord;
	int addressOffset = startAddress % 4;
	int lengthOffset = length % 4;

	//printf("Sending read, %d, %d\n", startAddress, length);

	// Start at the user specified address
	OverlapStructure.Offset = ((DWORD) startAddress) + OUTPUT_OFFSET;
	OverlapStructure.OffsetHigh = 0;
	OverlapStructure.hEvent = 0;

	dwTotalBytesRead = 0;

	//There is something wrong with the read we have to fix it up
	if(addressOffset != 0 || lengthOffset != 0){
		//Fix the starting address
		startAddressWholeWord = startAddress - addressOffset;

		//Fix the length
		if(lengthOffset == 0){
			lengthWholeWord = length + 4;
		}
		else{
			lengthWholeWord = length + (4 - lengthOffset);
		}		

		while (dwTotalBytesRead != (DWORD) lengthWholeWord){
			ReadFile(hFile, wholeWordBuffer + dwTotalBytesRead, (DWORD) lengthWholeWord - dwTotalBytesRead, NULL, &OverlapStructure );
			GetOverlappedResult( hFile, &OverlapStructure, &dwBytesRead, TRUE );
			if (GetLastError() != 0 && GetLastError() != ERROR_IO_PENDING){
				PrintError( "Read", "" );
                setLastError( INVALIDREADTRANSMIT);
				return false;
			}
			OverlapStructure.Offset += dwBytesRead;
			dwTotalBytesRead += dwBytesRead;
		}

		//Copy the word-aligned data into the non-word aligned request buffer
		memcpy(readBackData, wholeWordBuffer + addressOffset, length);
	}

	else{
		while (dwTotalBytesRead != (DWORD) length){
			ReadFile( hFile, readBackData + dwTotalBytesRead, (DWORD) length - dwTotalBytesRead, NULL, &OverlapStructure );
			GetOverlappedResult( hFile, &OverlapStructure, &dwBytesRead, TRUE );
			if (GetLastError() != 0 && GetLastError() != ERROR_IO_PENDING){
				PrintError( "Read", "" );
                setLastError( INVALIDREADTRANSMIT);
				return false;
			}
			OverlapStructure.Offset += dwBytesRead;
			dwTotalBytesRead += dwBytesRead;
		}
	}
	return true;
}

//Writes from the PCIe need to start word-aligned (startAddress must be a multiple of 4)
// and the length of the read must also be a multiple of 4.
//We have to fix things up if this is not the case.
//Start Address		Length offset			
//address offset	0	1	2	3
//				0	0	0	0	0
//				1	-1	-1	-1	-1
//				2	-2	-2	-2	-2
//				3	-3	-3	-3	-3	
//Length			Length offset			
//address offset	0	1	2	3
//				0	0	+3	+2	+1
//				1	+4	+3	+2	+1
//				2	+4	+3	+2	+1
//				3	+4	+3	+2	+1
//NOTICE - We can't fix the case in which the starting address or length ends up not being word-aligned but
// we don't want to disturb the data in the earlier/later bytes.  We are going to overwrite them
// with zeros!
BOOL PCIE_SIRC::sendWrite(uint32_t startAddress, uint32_t length, uint8_t *buffer)
{
	DWORD dwBytesWritten, dwTotalBytesWritten;
	OVERLAPPED OverlapStructure;

	int lengthWholeWord;
	int startAddressWholeWord;
	int addressOffset = startAddress % 4;
	int lengthOffset = length % 4;

	// Start at the user specified address
	OverlapStructure.Offset = ((DWORD) startAddress) + INPUT_OFFSET;
	OverlapStructure.OffsetHigh = 0;
	OverlapStructure.hEvent = 0;

	dwTotalBytesWritten = 0;

	//There is something wrong with the write we have to fix it up
	if(addressOffset != 0 || lengthOffset != 0){
		//Fix the starting address
		startAddressWholeWord = startAddress - addressOffset;

		//Fix the length
		if(lengthOffset == 0){
			lengthWholeWord = length + 4;
		}
		else{
			lengthWholeWord = length + (4 - lengthOffset);
		}		

		//Copy the non-word aligned request buffer into the word-aligned data buffer
		//Zero out the earlier bytes
		memset(wholeWordBuffer, 0, addressOffset);
		//Copy over the data
		memcpy(wholeWordBuffer + addressOffset, buffer, length);
		//Zero out the later bytes
		memset(wholeWordBuffer + addressOffset + length, 0, lengthWholeWord - length);

		//printf("Sending corrected Write, %d, %d\n", startAddressWholeWord, lengthWholeWord);

		while (dwTotalBytesWritten != (DWORD) lengthWholeWord){
			// hFile is opened for synchronous I/O, so this should not return until it is done.  I'm not sure if it does or
			//	not, because I have to call GetOverlappedResult() to find out how big the transfer was, and that won't return
			//	until the operation is done.
			WriteFile( hFile, wholeWordBuffer + dwTotalBytesWritten, (DWORD) lengthWholeWord - dwTotalBytesWritten, NULL, &OverlapStructure );
			GetOverlappedResult( hFile, &OverlapStructure, &dwBytesWritten, TRUE );
			// Because I'm using overlapped I/O to pass the file pointer, ERROR_IO_PENDING is returned from WriteFile() even
			//	when it succeeds.
			if (GetLastError() != 0 && GetLastError() != ERROR_IO_PENDING){
				PrintError( "Write", "" );
                setLastError( INVALIDWRITETRANSMIT);
				return false;
			}
			// I have to manually update the starting address in the overlapped structure
			OverlapStructure.Offset += dwBytesWritten;
			dwTotalBytesWritten += dwBytesWritten;
		}
	}
	else{
		//printf("Sending Write, %d, %d\n", startAddress, length);

		while (dwTotalBytesWritten != (DWORD) length){
			// hFile is opened for synchronous I/O, so this should not return until it is done.  I'm not sure if it does or
			//	not, because I have to call GetOverlappedResult() to find out how big the transfer was, and that won't return
			//	until the operation is done.
			WriteFile( hFile, buffer + dwTotalBytesWritten, (DWORD) length - dwTotalBytesWritten, NULL, &OverlapStructure );
			GetOverlappedResult( hFile, &OverlapStructure, &dwBytesWritten, TRUE );
			// Because I'm using overlapped I/O to pass the file pointer, ERROR_IO_PENDING is returned from WriteFile() even
			//	when it succeeds.
			if (GetLastError() != 0 && GetLastError() != ERROR_IO_PENDING)
			{
				PrintError( "Write", "" );
                setLastError( INVALIDWRITETRANSMIT);
				return false;
			}
			// I have to manually update the starting address in the overlapped structure
			OverlapStructure.Offset += dwBytesWritten;
			dwTotalBytesWritten += dwBytesWritten;
		}
	}
	return true;
}

BOOL PCIE_SIRC::sendParamRegisterRead(uint8_t regNumber, uint32_t *value)
{
	DWORD dwBytesRead, dwTotalBytesRead;
	DWORD dwByteCount = 4;
	OVERLAPPED OverlapStructure;

	// Start at the user specified address
	OverlapStructure.Offset = PARAMETER_REG_OFFSET + (32 * regNumber);
	OverlapStructure.OffsetHigh = 0;
	OverlapStructure.hEvent = 0;

	//printf("Sending Param Reg Read\n");

	dwTotalBytesRead = 0;
	while (dwTotalBytesRead != dwByteCount)
	{
		ReadFile( hFile, value + dwTotalBytesRead, dwByteCount - dwTotalBytesRead, NULL, &OverlapStructure );
		GetOverlappedResult( hFile, &OverlapStructure, &dwBytesRead, TRUE );
		if (GetLastError() != 0 && GetLastError() != ERROR_IO_PENDING)
		{
			PrintError( "ParamRead", "" );
            setLastError( INVALIDPARAMREADTRANSMIT);
			return false;
		}
		OverlapStructure.Offset += dwBytesRead;
		dwTotalBytesRead += dwBytesRead;
	}
	return true;
}

BOOL PCIE_SIRC::sendParamRegisterWrite(uint8_t regNumber, uint32_t value)
{
	DWORD dwBytesWritten, dwTotalBytesWritten;
	DWORD dwByteCount = 4;
	OVERLAPPED OverlapStructure;
	
	// Start at the user specified address
	// The parameter registers are spaced out on cache line boundaries.
	// Thus, each local card address is a multiple of 32 bytes
	OverlapStructure.Offset = PARAMETER_REG_OFFSET + (32 * regNumber);
	OverlapStructure.OffsetHigh = 0;
	OverlapStructure.hEvent = 0;

	//printf("Sending Param Reg Write\n");

	dwTotalBytesWritten = 0;
	while (dwTotalBytesWritten != dwByteCount)
	{
		// hFile is opened for synchronous I/O, so this should not return until it is done.  I'm not sure if it does or
		//	not, because I have to call GetOverlappedResult() to find out how big the transfer was, and that won't return
		//	until the operation is done.
		WriteFile( hFile, &value + dwTotalBytesWritten, dwByteCount - dwTotalBytesWritten, NULL, &OverlapStructure );
		GetOverlappedResult( hFile, &OverlapStructure, &dwBytesWritten, TRUE );
		// Because I'm using overlapped I/O to pass the file pointer, ERROR_IO_PENDING is returned from WriteFile() even
		//	when it succeeds.
		if (GetLastError() != 0 && GetLastError() != ERROR_IO_PENDING)
		{
			PrintError( "ParamWrite", "" );
            setLastError( INVALIDPARAMWRITETRANSMIT);
			return false;
		}
		// I have to manually update the starting address in the overlapped structure
		OverlapStructure.Offset += dwBytesWritten;
		dwTotalBytesWritten += dwBytesWritten;
	}
	return true;
}

BOOL PCIE_SIRC::sendRun()
{
	//printf("Sending Run\n");
	return (sendParamRegisterWrite(255, 1));
}

BOOL PCIE_SIRC::waitDone(uint32_t maxWaitTimeInMsec)
{
	unsigned int value;

	//printf("Waiting for Done\n");

	uint32_t currTime = GetTickCount();
	uint32_t endTime = currTime + maxWaitTimeInMsec;

	setLastError( 0);

	//Wait for the system to finish execution
    do {
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
BOOL PCIE_SIRC::sendReset()
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
BOOL PCIE_SIRC::sendWriteAndRun(uint32_t startAddress, uint32_t inLength, uint8_t *inData, 
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

