// Copyright (C) Microsoft Corporation. All rights reserved.

//
// Packet driver routines
//
#include "sirc_internal.h"
#define _CRT_SECURE_NO_WARNINGS 1

//=============================================================================
//    SubSection: Debug
//
//    Description: Debugging printouts, removed from release
//=============================================================================

#ifdef DEBUG
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 1
#endif
#define WARN(x)     printf x
#define DPRINTF(x)  {if (Debug) printf x;}
#define NOISE(x)    {if (Debug>2) printf x;}
#else
#define DEBUG_LEVEL 0
#define WARN(x)
#define DPRINTF(x)
#define NOISE(x)
#endif
#define UnusedParameter(x) x=x

//=============================================================================
//    SubSection: System
//
//    Description: OS/processor adaptations.
//=============================================================================

//
// This function might still be lacking on some old Windows systems, e.g. XP32/SP3
// NB: If your processor does not implement cmpxchg8b you lose.
//

#pragma warning(disable:4100) /* unreferenced formal parameter */

#if _MSC_VER <= 1400
typedef __int64 LONGLONG;
#endif

#if _WIN64
#define MyInterlockedCompareExchange64 InterlockedCompareExchange64
#else
inline
LONGLONG
__declspec(naked)
__stdcall
MyInterlockedCompareExchange64 (
    IN OUT LONGLONG volatile *Destination,
    IN LONGLONG ExChange,
    IN LONGLONG Comparand
    )
{
    __asm {
        push        ebx
        push        ebp  
        mov         ebp,dword ptr [esp+0Ch]  
        mov         ebx,dword ptr [esp+10h]  
        mov         ecx,dword ptr [esp+14h]  
        mov         eax,dword ptr [esp+18h]  
        mov         edx,dword ptr [esp+1Ch]  
        lock cmpxchg8b qword ptr [ebp]  
        pop         ebp  
        pop         ebx  
        ret         14h  
   }
}

#pragma warning(default:4100) /* unreferenced formal parameter */

#endif

//=============================================================================
//    SubSection: Data Structures::
//
//    Description: Definitions for the VPC constants and data structures.
//=============================================================================


#define NETSV_DRIVER_NAME_V3                    L"\\\\.\\Global\\VPCNetS3"
#define NETSV_DRIVER_G_NAME_V2                  L"\\\\.\\Global\\VPCNetS2"
#define NETSV_DRIVER_NAME_V2                    L"\\\\.\\VPCNetS2"
#define OLD_DRIVER_NAME                         L"\\\\.\\Packet"

#define    kVPCNetSvVersionMajor2    2
#define    kVPCNetSvVersionMinor2    6
#define    kVPCNetSvVersion2       ((kVPCNetSvVersionMajor2 << 16) | kVPCNetSvVersionMinor2)

#define    kVPCNetSvVersionMajor3    3
#define    kVPCNetSvVersionMinor3    1
#define    kVPCNetSvVersion3       ((kVPCNetSvVersionMajor3 << 16) | kVPCNetSvVersionMinor3)

enum
{
    kIoctlFunction_SetOid                     = 0,
    kIoctlFunction_QueryOid,
    kIoctlFunction_Reset,
    kIoctlFunction_EnumAdapters,
    kIoctlFunction_GetStatistics,
    kIoctlFunction_GetVersion,
    kIoctlFunction_GetFeatures,
    kIoctlFunction_SendToHostOnly,
    kIoctlFunction_RegisterGuest,
    kIoctlFunction_DeregisterGuest,
    kIoctlFunction_CreateVirtualAdapter,
    kIoctlFunction_DestroyVirtualAdapter,
    kIoctlFunction_GetAdapterAttributes,
    kIoctlFunction_Noop,
    kIoctlFunction_CreatePacketBuffers_UNIMPLEMENTED,
    kIoctlFunction_ProcessTransmitBuffers,
    kIoctlFunction_NotifyReceiveBufferConsumed,
    kIoctlFunction_CreatePacketBuffers,

    kIoctlFunction_AllocateMACAddress,
    kIoctlFunction_FreeMACAddress
};

//
// These IOCTLs apply only to the VPC control object
//
#define IOCTL_GET_VERSION                    CTL_CODE(FILE_DEVICE_PROTOCOL, kIoctlFunction_GetVersion,                METHOD_BUFFERED, FILE_ANY_ACCESS)

//
// These IOCTLs apply only to the adapter object
//
#define IOCTL_CREATE_VIRTUAL_ADAPTER        CTL_CODE(FILE_DEVICE_PROTOCOL, kIoctlFunction_CreateVirtualAdapter,        METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DESTROY_VIRTUAL_ADAPTER        CTL_CODE(FILE_DEVICE_PROTOCOL, kIoctlFunction_DestroyVirtualAdapter,    METHOD_BUFFERED, FILE_ANY_ACCESS)

//
// These IOCTLs apply only to the virtual adapter object
//
#define IOCTL_REGISTER_GUEST                CTL_CODE(FILE_DEVICE_PROTOCOL, kIoctlFunction_RegisterGuest,            METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_DEREGISTER_GUEST                CTL_CODE(FILE_DEVICE_PROTOCOL, kIoctlFunction_DeregisterGuest,            METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_CREATE_PACKET_BUFFERS            CTL_CODE(FILE_DEVICE_PROTOCOL, kIoctlFunction_CreatePacketBuffers,        METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PROCESS_TRANSMIT_BUFFERS		CTL_CODE(FILE_DEVICE_PROTOCOL, kIoctlFunction_ProcessTransmitBuffers,	METHOD_NEITHER,	 FILE_ANY_ACCESS)
#define IOCTL_NOTIFY_RECEIVE_BUFFER_CONSUMED	\
											CTL_CODE(FILE_DEVICE_PROTOCOL, kIoctlFunction_NotifyReceiveBufferConsumed,	METHOD_NEITHER,	 FILE_ANY_ACCESS)

//
// NETSV_ALLOCATE_MAC_ADDRESS.fFlags constants
//
#define kVPCNetSvAllocateMACAddressFlag_AddressActionMask        0x0003
#define    kVPCNetSvAllocateMACAddressFlag_GenerateAddress       0x0000    // The driver should generate a unique MAC address
#define    kVPCNetSvAllocateMACAddressFlag_AddressIsSuggested    0x0001    // The driver should use the specified address if not already in use - generate a new one otherwise
#define kVPCNetSvAllocateMACAddressFlag_AddressIsExclusive       0x0002    // The driver must use the specified address - fail if not available
#define kVPCNetSvAllocateMACAddressFlag_AddressMayDuplicate      0x0003    // The driver must use the specified address - may duplicate an existing address

#define kVPCNetSvAllocateMACAddressFlag_AddressWasGenerated      0x8000    // The driver generated a new MAC address
#define kVPCNetSvAllocateMACAddressFlag_AddressWasDuplicated     0x4000    // The driver generated a new MAC address

//
// NETSV_REGISTER_GUEST.fFlags constants
//
#define kVirtualSwitchRegisterGuestFlag_AddressActionMask        0x00000003
#define    kVirtualSwitchRegisterGuestFlag_GenerateAddress       0x00000000    // The VPCNetSv driver should generate a unique MAC address
#define    kVirtualSwitchRegisterGuestFlag_AddressIsSuggested    0x00000001    // The VPCNetSv driver should use the specified address if not already in use - generate a new one otherwise
#define kVirtualSwitchRegisterGuestFlag_AddressIsExclusive       0x00000002    // The VPCNetSv driver must use the specified address - fail if not available
#define kVirtualSwitchRegisterGuestFlag_AddressMayDuplicate      0x00000003    // The VPCNetSv driver must use the specified address - may duplicate an existing address

#define kVirtualSwitchRegisterGuestFlag_AddressWasGenerated      0x80000000    // The VPCNetSv driver generated a new MAC address
#define kVirtualSwitchRegisterGuestFlag_AddressDuplicated        0x40000000    // The driver generated a new MAC address

#define DEFAULT_PACKET_FILTER    (NDIS_PACKET_TYPE_DIRECTED + NDIS_PACKET_TYPE_MULTICAST + NDIS_PACKET_TYPE_BROADCAST)

#pragma pack(push)
#pragma pack(1)

//
// Packet Pump related structures
//
typedef struct    VPCNetSvPacketEntry    VPCNetSvPacketEntry, *VPCNetSvPacketEntryPtr;
struct VPCNetSvPacketEntry
{
    union
    {
        struct VPCNetSvPacketEntry *    fNextPacket;        // pointer to next packet in list - software
        volatile UINT32                 fNextPacketOffset;  // offset to next packet from beginning of packet buffer

        // Padding to make sure we have same size on 64 bit systems
        UINT64                        _dummy_0_;
    };

    UINT32            fPacketLength;                        // the length of the packet data - kVPCNetSvMaximumPacketLength max
    union
    {
        void *            fAppRefCon;                       // an application-defined reference or constant

        // Padding to make sure we have same size on 64 bit systems
        UINT64        _dummy_1_;
    };
#define kVPCNetSvMaximumPacketLength                1518ul
    UINT8            fPacketData[kVPCNetSvMaximumPacketLength];    // packet data
};


typedef struct    VPCNetSvPacketEntryQueueHead    VPCNetSvPacketEntryQueueHead, *VPCNetSvPacketEntryQueueHeadPtr;
struct VPCNetSvPacketEntryQueueHead
{
    volatile UINT32        fHead;
    volatile UINT32        fModificationCount;
};


typedef struct     VPCNetSvPacketBuffer    VPCNetSvPacketBuffer, *VPCNetSvPacketBufferPtr;
struct VPCNetSvPacketBuffer
{
    VPCNetSvPacketEntryQueueHead    fFreeBufferQueue;         // LIFO queue - offset to last queued packet entry
    VPCNetSvPacketEntryQueueHead    fReadyBufferQueue;        // LIFO queue - offset to last queued packet entry
    VPCNetSvPacketEntryQueueHead    fCompletedBufferQueue;    // LIFO queue - offset to last queued packet entry
    volatile UINT32                 fPendingCount;            // The number of packets in flight
    VPCNetSvPacketEntry             fPacketArray[ 1 ];        // Variable size array of entries
};


typedef struct    VPCNetSvPacketBufferDesc    VPCNetSvPacketBufferDesc, *VPCNetSvPacketBufferDescPtr;
struct VPCNetSvPacketBufferDesc
{
    //
    // This buffer is generated by the driver and returned by-copy.
    //
    union
    {
        VPCNetSvPacketBufferPtr        fPacketBuffer;

        // Padding to make sure we have same size on 64 bit systems
        UINT64                    _dummy_0_;
    };
    
    UINT32                    fLength;
    UINT32                    fMinimumEntryOffset;
    UINT32                    fMaximumEntryOffset;
};


typedef struct
{
    UINT32            fAdapterID;
} NETSV_CREATE_VIRTUAL_ADAPTER, *PNETSV_CREATE_VIRTUAL_ADAPTER;

#define ETHERNET_ADDRESS_LENGTH 6

typedef struct _NETSV_ALLOCATE_MAC_ADDRESS    NETSV_ALLOCATE_MAC_ADDRESS, *PNETSV_ALLOCATE_MAC_ADDRESS;
struct _NETSV_ALLOCATE_MAC_ADDRESS
{
    UINT16            fFlags;
    UINT8            fMACAddress[ETHERNET_ADDRESS_LENGTH];
};
typedef const struct _NETSV_ALLOCATE_MAC_ADDRESS    *PCNETSV_ALLOCATE_MAC_ADDRESS;

typedef struct _NETSV_FREE_MAC_ADDRESS    NETSV_FREE_MAC_ADDRESS, *PNETSV_FREE_MAC_ADDRESS;
struct _NETSV_FREE_MAC_ADDRESS
{
    UINT8            fMACAddress[ETHERNET_ADDRESS_LENGTH];
};
typedef const struct _NETSV_FREE_MAC_ADDRESS    *PCNETSV_FREE_MAC_ADDRESS;

typedef struct
{
    UINT16            fVersion;                               // Version of this structure
    UINT16            fLength;                                // Length of following data
    UINT32            fFlags;                                 // Flags
    UINT8             fMACAddress[ETHERNET_ADDRESS_LENGTH];   // Guest VM MAC address
} NETSV_REGISTER_GUEST, *PNETSV_REGISTER_GUEST;

#define NETSV_REGISTER_GUEST_VERSION    0x0002
#define    NETSV_REGISTER_GUEST_LENGTH        (sizeof(NETSV_REGISTER_GUEST) - 2*sizeof(UINT16))


#define    NETSV_DEREGISTER_GUEST_LENGTH    sizeof(NETSV_DEREGISTER_GUEST)


typedef struct _NETSV_CREATE_PACKET_BUFFERS    NETSV_CREATE_PACKET_BUFFERS, *PNETSV_CREATE_PACKET_BUFFERS;
struct _NETSV_CREATE_PACKET_BUFFERS
{
    union
    {
        //
        // Data provided by the user application to th ekernel driver.
        //
        struct _NETSV_CREATE_PACKET_BUFFERS_INPUT
        {
            union
            {
                HANDLE                        fPacketComplete;    // Handle to an Event
                UINT64                    _dummy_0_;
            };
            
            union
            {
                HANDLE                        fPacketReceived;    // Handle to an Event
                UINT64                    _dummy_1_;
            };
            
        }    fInput;

        //
        // Data returned by the kernel mode driver to the user application.
        //
        struct _NETSV_CREATE_PACKET_BUFFERS_OUTPUT
        {
            VPCNetSvPacketBufferDesc    fTxBuffer;
            VPCNetSvPacketBufferDesc    fRxBuffer;
        }    fOutput;
    };
};

#pragma pack(pop)

//
// Mapping from/to shared memory offsets and pointers
//
#ifdef DEBUG //DEBUG with stricter boundary checks
#define IsValidPacketEntryOffset(_PacketBufferDesc_, _EntryOffset_)        \
    (                                                                      \
       ((_EntryOffset_) >= (_PacketBufferDesc_)->fMinimumEntryOffset) &&   \
       ((_EntryOffset_) <= (_PacketBufferDesc_)->fMaximumEntryOffset) &&   \
       (((((_EntryOffset_) - (_PacketBufferDesc_)->fMinimumEntryOffset)/sizeof(VPCNetSvPacketEntry)) *  \
           sizeof(VPCNetSvPacketEntry) + (_PacketBufferDesc_)->fMinimumEntryOffset) == (_EntryOffset_)) \
    )

#else // Minimal boundary checks
#define IsValidPacketEntryOffset(_PacketBufferDesc_, _EntryOffset_)        \
    (                                                                      \
       ((_EntryOffset_) >= (_PacketBufferDesc_)->fMinimumEntryOffset) &&   \
       ((_EntryOffset_) <= (_PacketBufferDesc_)->fMaximumEntryOffset)      \
    )

#endif

#define GetPacketEntryPointerFromOffset(_PacketBufferDesc_, _EntryOffset_)                   \
    ((VPCNetSvPacketEntryPtr) (IsValidPacketEntryOffset(_PacketBufferDesc_, _EntryOffset_) ? \
      (((UINT8 *) ((_PacketBufferDesc_)->fPacketBuffer)) + (_EntryOffset_)) : NULL))

#define GetPacketEntryOffsetFromPointer(_PacketBufferDesc_, _EntryPointer_) \
    ((UINT32) (((_EntryPointer_) != NULL) ? (((UINT8 *) (_EntryPointer_)) - \
      ((UINT8 *)((_PacketBufferDesc_)->fPacketBuffer))) : 0ul))

//=============================================================================
//    SubSection: PacketManager::
//
//    Description: A simple class for allocation etc of packets
//=============================================================================

class PacketManager {
public:
    PacketManager(void)
    {
        FreePackets = NULL;
    }
    ~PacketManager(void)
    {
        for (;;) {
            PACKET *Packet = FreePackets;

            if (Packet == NULL)
                break;

            // make sure list is not circular
            assert(Packet->Next != Packet);

            FreePackets = Packet->Next;

            //Cannot do this, we do NOT own the buffers. 
            //They might not be malloced.
            //delete Packet->Buffer;
            Packet->Buffer = NULL;
            delete Packet;
        }
    }

    PACKET *Allocate(void)
    {
        PACKET * Packet = this->FreePackets;

        if (Packet != NULL) {
            this->FreePackets = Packet->Next;

            // make sure list is not circular
            assert(FreePackets != Packet);

            Packet->Flush = TRUE;
            return Packet;
        }
        Packet = new PACKET;
        if (Packet != NULL) {
            memset(Packet,0,sizeof *Packet);
            Packet->Init(NULL,0);
        }
        return Packet;
    }

    void Free(IN PACKET *Packet)
    {
        Packet->Init(Packet->Buffer,Packet->Length);

        // make sure list is not circular
        assert(FreePackets != Packet);

        Packet->Next = this->FreePackets;
        this->FreePackets = Packet;
    }

private:
    PACKET *FreePackets;
};

//=============================================================================
//    SubSection: MicKey::
//
//    Description: Registry key definitions
//=============================================================================

static const HKEY MicKeyHive     = HKEY_CURRENT_USER;//was: HKEY_LOCAL_MACHINE
static const char MicKey[]       = "SOFTWARE\\Microsoft\\Invisible Computing";
static const char MicKeyMacPat[] = "SerplexMAC";

//=============================================================================
//    SubSection: VirtualPcDriver::
//
//    Description: VirtualPC's NDIS filter driver, generic interface definition
//=============================================================================

class VirtualPcDriver : public PACKET_DRIVER {
public:
    //
    // Methods that must be subclassed by each version
    //
    virtual BOOL Open(IN const wchar_t *AdapterName) = 0;

    virtual BOOL Flush(void) = 0;

    virtual PACKET * AllocatePacket(IN BYTE *Buffer,
                                    IN UINT Length,
                                    IN BOOL fForReceive
                                    ) = 0;
    virtual void FreePacket(IN PACKET *Packet,
                            IN BOOL bForReceiving) = 0;

    virtual HRESULT PostReceivePacket(IN PACKET *Packet) = 0;
    virtual HRESULT PostTransmitPacket(IN PACKET *Packet) = 0;
    virtual PACKET_MODE GetNextCompletedPacket(OUT PACKET ** pPacket,
                                               IN  UINT32 TimeOutInMsec
                                               ) = 0;
    virtual HRESULT SetFilter(IN UINT32 Filter) = 0;

    virtual BOOL GetMaxOutstanding(OUT UINT32 *NumReads,
                                   OUT UINT32 *NumWrites) = 0;


    //
    // Methods that may be subclassed
    //
    virtual HRESULT SetMyVirtualMac( IN UINT8 *Mac);
    virtual HRESULT GetMyVirtualMac( OUT UINT8 *EthernetAddress);

    virtual HANDLE OpenAdapter(IN wchar_t * SymbolicName,
                               OUT HANDLE  * phAuxHandle);
    virtual HANDLE InitializeInstance(IN wchar_t * SymbolicName,
                                      OUT wchar_t * AdapterName,
                                      IN UINT32 AdapterID);

    virtual BOOL ChangeMacAddress( IN UINT8 *MacAddress);

    virtual PACKET *GetNextReceivedPacket(IN UINT32 TimeOutInMsec);

    //
    // Methods that likely should not be subclassed
    //
    virtual BOOL DriverRequest(IN HANDLE hDriver,
                               IN BOOL Set,
                               IN OUT PPACKET_OID_DATA OidData
                               );
    virtual HRESULT SetDriverFilter(IN HANDLE hDriver,
                                  IN ULONG  Filter
                                  );
    virtual HRESULT GetSpeed(IN HANDLE hDriver,
                             OUT ULONG *Speed
                             );
    virtual BOOL SelectAdapter(IN  HANDLE          hControl,
                               IN  const wchar_t * DesiredAdapterName,
                               OUT wchar_t       * SelectedAdapter);

    //
    // Return the MAC address used by the interface
    //
    virtual BOOL GetMacAddress(OUT UINT8 *EthernetAddress)
    {
        return (GetMyVirtualMac(EthernetAddress) == S_OK);
    };

    //
    // Inlined methods that cannot be subclassed
    //

    //
    // Return the driver's version
    //
    UINT32 GetVersion(void)
    {
        return mVersion;
    }
        
    //
    // Sets the driver's version
    //
    void SetVersion(IN UINT32 Current)
    {
        mVersion = Current;
    }

    //
    // Selects the MAC to use or generate, up to a given maximum
    //
#define MAX_NUMBER_OF_MACs_TO_GENERATE 10
    bool NextMacKey(void)
    {
        if (mMacIndex >= MAX_NUMBER_OF_MACs_TO_GENERATE)
            return false;
        mMacIndex++;
        return true;
    }

    //
    // Generate the registry key to access the current MAC index
    //
    void GenMacKey(void)
    {
        if (mMacIndex > 0)
            sprintf_s(mMicKeyMac,sizeof mMicKeyMac,"%s%d", MicKeyMacPat, mMacIndex);
        else
            sprintf_s(mMicKeyMac,sizeof mMicKeyMac,"%s", MicKeyMacPat);
    }

    //
    // Returns the registry key for the current MAC index
    //
    char *CurMacKey(void)
    {
        return (mMicKeyMac[0]) ? mMicKeyMac : NULL;
    }

    //
    // Constructor
    //
    VirtualPcDriver(void)
    {
        hFileHandle = INVALID_HANDLE_VALUE;
        mVersion = 0;
        mMacIndex = 0;
        mMicKeyMac[0] = 0;
        Debug = 0;
        Quiet = FALSE;
    }

    //
    // Destructor
    //
    ~VirtualPcDriver(void)
    {
    }

    //
    // Debug support
    //
    int Debug;
    BOOL Quiet;

    //
    // Despised global
    //
    HANDLE hFileHandle;
private:
    //
    // Common private state
    //
    UINT32 mVersion;
    UINT32 mMacIndex;
    char mMicKeyMac[128];
};

//=============================================================================
//  Method: VirtualPcDriver::DriverRequest().
//
//  Description: Submit a request to the packet driver.
//=============================================================================

BOOL
VirtualPcDriver::DriverRequest(
    IN HANDLE hDriver,
    IN BOOL Set,
    IN OUT PPACKET_OID_DATA OidData
    )
{
    DWORD    BytesReturned;
    BOOL     bResult;
    UINT32   OidCommand;
    UINT32   OidLength;

    NOISE(("\tDriverRequest(%u,x%x)",Set,OidData->Oid));

    //
    // Compute the length of the data, the code to use.
    //
    OidLength = sizeof(PACKET_OID_DATA) - 1 + OidData->Length;

    if ( Set != FALSE )
    {
        OidCommand = IOCTL_PROTOCOL_SET_OID;
    }
    else
    {
        OidCommand = IOCTL_PROTOCOL_QUERY_OID;
    }
    
    //
    //  Submit request.
    //

    bResult = DeviceIoControl(
                        hDriver,
                        OidCommand,
                        OidData,
                        OidLength,
                        OidData,
                        OidLength,
                        &BytesReturned,
                        NULL
                        );

    return bResult;
}

//=============================================================================
//  Method: VirtualPcDriver::SetDriverFilter().
//
//  Description: Set capture filters.
//=============================================================================

HRESULT
VirtualPcDriver::SetDriverFilter(
    IN HANDLE hDriver,
    IN ULONG  Filter
    )
{
    ULONG               IoCtlBufferLength = sizeof(PACKET_OID_DATA)-1 + sizeof(ULONG);
    BYTE                IoCtlBuffer[sizeof(PACKET_OID_DATA)-1 + sizeof(ULONG)];
    PPACKET_OID_DATA    OidData = NULL;
    BOOL                bResult;

    DPRINTF(("SetDriverFilter(x%x)",Filter));

    //
    // Prepare our arguments
    //
    memset(IoCtlBuffer, 0, IoCtlBufferLength);
    OidData = (PPACKET_OID_DATA) IoCtlBuffer;
    OidData->Oid = OID_GEN_CURRENT_PACKET_FILTER;
    OidData->Length = sizeof(ULONG);

    memcpy(OidData->Data, &Filter, sizeof Filter);

    //
    // Issue the request
    //
    bResult = DriverRequest(hDriver, TRUE, OidData);

    return (bResult ? S_OK : E_FAIL);
}


//=============================================================================
//  Method: VirtualPcDriver::GetSpeed().
//
//  Description: Get the link speed of the interface below.
//=============================================================================

HRESULT 
VirtualPcDriver::GetSpeed(
    IN HANDLE hDriver,
    OUT ULONG *Speed
    )
{
    ULONG               IoCtlBufferLength = sizeof(PACKET_OID_DATA)-1 + sizeof(ULONG);
    BYTE                IoCtlBuffer[sizeof(PACKET_OID_DATA)-1 + sizeof(ULONG)];
    PPACKET_OID_DATA    OidData = NULL;
    BOOL                bResult;

    NOISE(("GetSpeed()"));

    //
    // Prepare our arguments
    //
    memset(IoCtlBuffer, 0, IoCtlBufferLength);
    OidData = (PPACKET_OID_DATA) IoCtlBuffer;
    OidData->Oid = OID_GEN_LINK_SPEED;
    OidData->Length = sizeof(ULONG);

    //
    // Issue the request
    //
    bResult = DriverRequest(hDriver, FALSE, OidData);

    //
    // This might fail.
    //
    if (!bResult)
    {
        *Speed = 0; // donno
    } else
        memcpy(Speed, OidData->Data, sizeof(ULONG));

    return (bResult ? S_OK : E_FAIL);
}

//=============================================================================
//  Method: VirtualPcDriver::SetMyVirtualMac().
//
//  Description: Assigns a MAC address to the interface.
//=============================================================================

HRESULT
VirtualPcDriver::SetMyVirtualMac( IN UINT8 *Mac)
{
    HRESULT sc = E_FAIL;
    HKEY Key;
    LONG Return;
    UINT32 Length;
    UINT8 Data[128];

    DPRINTF(("SetMyVirtualMac()"));

    //
    // Generate the key name to use
    //
    GenMacKey();

    //
    // Open/Create the key. On some systems requires admin privileges.
    //
    Return = RegCreateKeyExA(MicKeyHive,
                             MicKey,
                             0,
                             NULL, // or "" ?
                             REG_OPTION_NON_VOLATILE,
                             KEY_ALL_ACCESS,
                             NULL,
                             &Key,
                             NULL);
    if (Return != ERROR_SUCCESS) {
        WARN(("Failed to create key '%s' (er=%d)", MicKey, Return));
        return E_FAIL;
    }

    //
    // String-ify the MAC data and write the key
    //
    sprintf_s((char*)Data,sizeof Data,"%x:%x:%x:%x:%x:%x", Mac[0], Mac[1], Mac[2], Mac[3], Mac[4], Mac[5]);
    Length = (UINT32) strlen((char*)Data)+1;
    Return = RegSetValueExA(Key,
                            CurMacKey(),
                            0,
                            REG_SZ,
                            Data,
                            Length);

    if (Return != ERROR_SUCCESS) {
        WARN(("Failed to write value '%s' in key '%s' (er=%d)", CurMacKey(), MicKey, Return));
        goto Done;
    }

    sc = S_OK;
 Done:
    RegCloseKey(Key);
    return sc;
}

//=============================================================================
//  Method: VirtualPcDriver::GetMyVirtualMac().
//
//  Description: Generates a MAC address for the interface.
//               Uses the registry as permanent storage.
//=============================================================================

HRESULT
VirtualPcDriver::GetMyVirtualMac( OUT UINT8 *EthernetAddress)
{
    HRESULT sc = E_FAIL;
    HKEY Key;
    LONG Return;
    DWORD Type, Length;
    UINT8 Data[128];
    ULONG Mac[6];

    DPRINTF(("GetMyVirtualMac()"));

    //
    // Generate the key name to use
    //
    GenMacKey();

    //
    // Open the key.
    //
    Return = RegOpenKeyExA(MicKeyHive,
                           MicKey,
                           0,
                           KEY_READ,
                           &Key);
    if (Return != ERROR_SUCCESS) {
        WARN(("Cannot find the '%s' key (er=%d)", MicKey, Return));
        return E_FAIL;
    }

    //
    // Read the value, check it exists and that it is a string
    //
    Length = (sizeof Data) - 1;
    Return = RegQueryValueExA(Key,
                              CurMacKey(),
                              0,
                              &Type,
                              Data,
                              &Length);

    if (Return != ERROR_SUCCESS) {
        WARN(("Missing value '%s' in key '%s' (er=%d)", CurMacKey(), MicKey, Return));
        goto Done;
    }

    if (Type != REG_SZ) {
        WARN(("Value '%s\\%s' wrong type x%x (not REG_SZ)", MicKey, CurMacKey(), Type));
        goto Done;
    }

    //
    // Parse the data. Accept more than our own format.
    //
    Data[Length] = 0;
    Return = sscanf_s((char*)Data,"%x:%x:%x:%x:%x:%x", &Mac[0], &Mac[1], &Mac[2], &Mac[3], &Mac[4], &Mac[5]);
    if (Return != 6)
    Return = sscanf_s((char*)Data,"%x %x %x %x %x %x", &Mac[0], &Mac[1], &Mac[2], &Mac[3], &Mac[4], &Mac[5]);
    if (Return != 6)
    Return = sscanf_s((char*)Data,"%x-%x-%x-%x-%x-%x", &Mac[0], &Mac[1], &Mac[2], &Mac[3], &Mac[4], &Mac[5]);
    if (Return != 6) {
        WARN(("MAC '%s' in bad taste (not xx:xx:xx:xx:xx:xx)", Data));
        goto Done;
    }

    //
    // Done ok
    //
    EthernetAddress[0] = (UINT8) Mac[0];
    EthernetAddress[1] = (UINT8) Mac[1];
    EthernetAddress[2] = (UINT8) Mac[2];
    EthernetAddress[3] = (UINT8) Mac[3];
    EthernetAddress[4] = (UINT8) Mac[4];
    EthernetAddress[5] = (UINT8) Mac[5];

    sc = S_OK;
 Done:
    RegCloseKey(Key);
    return sc;
}

//=============================================================================
//  Method: VirtualPcDriver::GetNextReceivedPacket().
//
//  Description: Dequeues the next packet from the receive queue.
//               If any xmit packet has completed its ignored.
//=============================================================================

PACKET *
VirtualPcDriver::GetNextReceivedPacket(
    IN UINT32 TimeOutInMsec
)
{
    PACKET *Packet = NULL;
    for (;;) {
        PACKET_MODE Mode = this->GetNextCompletedPacket(&Packet,TimeOutInMsec);
        if (Mode == PacketModeReceiving)
            break;
        if (Mode == PacketModeInvalid)
            return NULL;
        //otherwise drop it on the floor
    }
    //LogIt("pkt::rc %p",(UINT_PTR)Packet); duplicate
    return Packet;
}

//=============================================================================
//  Method: VirtualPcDriver::ChangeMacAddress().
//
//  Description: Changes the MAC address the driver is using
//=============================================================================

BOOL
VirtualPcDriver::ChangeMacAddress(
    IN UINT8 *MacAddress
)
{
    UINT8 EthernetAddress[6];
    GetMacAddress(EthernetAddress);
    if (memcmp(EthernetAddress,MacAddress,6) == 0)
        return TRUE;
    //
    // Must really change it
    //
    DWORD  BytesReturned;
    NETSV_REGISTER_GUEST GuestInfo;

    //
    // Deregister
    //
    memset(&GuestInfo,0,sizeof GuestInfo);
    GuestInfo.fVersion    = NETSV_REGISTER_GUEST_VERSION;
    GuestInfo.fLength     = NETSV_REGISTER_GUEST_LENGTH;

    if (DeviceIoControl(hFileHandle,
                        (UINT32)IOCTL_DEREGISTER_GUEST,
                        &GuestInfo, sizeof(GuestInfo),
                        &GuestInfo, sizeof(GuestInfo),
                        &BytesReturned,
                        NULL) == FALSE) 
    {
        //
        // Did not work, fail.
        //
        DWORD le = GetLastError();
        WARN(("Could not deregister guest address (le=%u)", le));
        return FALSE;
    }

    //
    // Say what we expect
    //
    memset(&GuestInfo,0,sizeof GuestInfo);
    GuestInfo.fVersion    = NETSV_REGISTER_GUEST_VERSION;
    GuestInfo.fLength     = NETSV_REGISTER_GUEST_LENGTH;
    memcpy(GuestInfo.fMACAddress,MacAddress,6);
    GuestInfo.fFlags      = kVirtualSwitchRegisterGuestFlag_AddressIsExclusive;

    //
    // Register
    //
    if (DeviceIoControl(hFileHandle,
                        (UINT32)IOCTL_REGISTER_GUEST,
                        &GuestInfo, sizeof(GuestInfo),
                        &GuestInfo, sizeof(GuestInfo),
                        &BytesReturned,
                        NULL) == FALSE) 
    {
        //
        // Did not work, MAC is in use, fail.
        //
        DWORD le = GetLastError();
        WARN(("Could not set desired MAC address (le=%u)", le));
        return FALSE;
    }

    return TRUE;
}

//=============================================================================
//  Method: VirtualPcDriver::InitializeInstance().
//
//  Description: Opens our driver instance and initializes it.
//=============================================================================

HANDLE
VirtualPcDriver::InitializeInstance(
    IN wchar_t    *SymbolicName,
    OUT wchar_t   *AdapterName,
    IN UINT32      AdapterID
    )
{
    HANDLE hVirtualAdapter;

    //
    // Create the object name from the given index
    //
    wsprintfW(
              AdapterName,
              L"\\\\.\\%s_%08lX", 
              &SymbolicName[12],
              AdapterID);
    AdapterName[1024-1]=L'\0';
    
    NOISE(("InitializeInstance(%ls)",AdapterName));

    //
    // Try to open the adapter
    //
    hVirtualAdapter = CreateFileW(AdapterName, 
                                  GENERIC_READ | GENERIC_WRITE, 
                                  FILE_SHARE_READ|FILE_SHARE_WRITE,
                                  NULL, 
                                  OPEN_EXISTING, 
                                  FILE_FLAG_OVERLAPPED|FILE_FLAG_NO_BUFFERING, 
                                  NULL);


    //
    // Register with the driver
    //
    if (hVirtualAdapter != INVALID_HANDLE_VALUE)
    {
        DWORD  BytesReturned;
        HRESULT sc;
        NETSV_REGISTER_GUEST GuestInfo;

        //
        // Say what we expect
        //
        memset(&GuestInfo,0,sizeof GuestInfo);
        GuestInfo.fVersion     = NETSV_REGISTER_GUEST_VERSION;
        GuestInfo.fLength     = NETSV_REGISTER_GUEST_LENGTH;

    TryAgain:
        //
        // Do we have a MAC to use
        //
        sc = GetMyVirtualMac(GuestInfo.fMACAddress);

        //
        // If not, generate a new one
        //
        if (FAILED(sc))
         {
            GuestInfo.fFlags = kVirtualSwitchRegisterGuestFlag_GenerateAddress;
        }
        else
         {
            GuestInfo.fFlags = kVirtualSwitchRegisterGuestFlag_AddressIsSuggested;
        }

        //
        // Register
        //
        if (DeviceIoControl(hVirtualAdapter,
                        (UINT32)IOCTL_REGISTER_GUEST,
                        &GuestInfo, sizeof(GuestInfo),
                        &GuestInfo, sizeof(GuestInfo),
                        &BytesReturned,
                        NULL) == FALSE) 
        {
            //
            // Did not work, MAC is in use. Try another one
            //
            if (NextMacKey())
                goto TryAgain;

            //
            // Really did not work, give it up.
            //
            WARN(("VirtualSwitchRegisterGuest failed (error %d)", GetLastError()));
            CloseHandle(hVirtualAdapter);
            hVirtualAdapter = INVALID_HANDLE_VALUE;
        }
    
        //
        // If we got a new MAC remember it, and tell the user about it.
        //
        if (GuestInfo.fFlags & kVirtualSwitchRegisterGuestFlag_AddressWasGenerated) 
        {
#if 1
            printf("VirtualPCDriver::A new Guest MAC address was generated %02x:%02x:%02x:%02x:%02x:%02x\n",
                             GuestInfo.fMACAddress[0], GuestInfo.fMACAddress[1],
                             GuestInfo.fMACAddress[2], GuestInfo.fMACAddress[3],
                             GuestInfo.fMACAddress[4], GuestInfo.fMACAddress[5]
                             );
#endif
            if (GuestInfo.fFlags & kVirtualSwitchRegisterGuestFlag_AddressIsSuggested) 
                NextMacKey();
            SetMyVirtualMac(GuestInfo.fMACAddress);
        }
    }
    
    return hVirtualAdapter;
}

//=============================================================================
//    Method: VirtualPcDriver::SelectAdapter().
//
//    Description: Parse the enumerated adapters info, pick the desired one
//=============================================================================

BOOL 
VirtualPcDriver::SelectAdapter(
    IN  HANDLE          hControl,
    IN  const wchar_t * DesiredAdapterName,
    OUT wchar_t       * SelectedAdapter)
{
    BOOL          bResult;
    wchar_t       Buffer[2*1024];
    DWORD         BytesReturned;

    //
    // Enumerate the adapters supported
    //
    bResult = DeviceIoControl(
                        hControl,
                        IOCTL_ENUM_ADAPTERS,
                        NULL,
                        0,
                        Buffer,
                        sizeof(Buffer),
                        &BytesReturned,
                        NULL
                        );

    if ( bResult == FALSE )
    {
        WARN(("Error: Packet Driver fails to enumerate adapters"));
        goto Out;
    }

    wchar_t *AdapterInfo = Buffer;
    bResult = FALSE;

    //
    // Get and skip the number of adapters
    //
    UINT16 nAdapters = *AdapterInfo;
    AdapterInfo += 2; // a one-wchar 'string' 

    //
    // Scan looking for the one we want
    //
    while (nAdapters-- > 0)
    {
        BOOL ok = TRUE;
        const wchar_t * Check = DesiredAdapterName;
        wchar_t * SymbolicName = SelectedAdapter;
        wchar_t * CurrentName = NULL;

        //
        // Check/Skip the adapter name.
        //
        DPRINTF(("Enumerating Adapter '%ls'",AdapterInfo));
        CurrentName = AdapterInfo;
        while( *AdapterInfo != (wchar_t) '\0' )
        {
            if (Check && ok)
                ok = (*AdapterInfo == *(Check++));
            AdapterInfo++;
        }

        AdapterInfo++;        //... Skip the NULL.

        //
        // Copy and terminate the symbolic name.
        //
        while( *AdapterInfo != (wchar_t) '\0' )
        {
            *SymbolicName++ = *AdapterInfo++;
        }

        *SymbolicName++ = *AdapterInfo++;

        //
        // Match?
        //
        if (ok)
        {
            if (!Quiet)
                printf("Will use NIC '%ls'\n",CurrentName);
            bResult = TRUE;
            break;
        }
    }

 Out:
    return bResult;
}

//=============================================================================
//    Method: VirtualPcDriver::OpenAdapter().
//
//    Description: Creates a new adapter instance for our use.
//=============================================================================

HANDLE
VirtualPcDriver::OpenAdapter(
    IN wchar_t * SymbolicName,
    OUT HANDLE  * phAuxHandle
    )
{
    wchar_t   AdapterName[1024];
    HANDLE    hHostAdapter = INVALID_HANDLE_VALUE;
    HANDLE    hVirtualAdapter = INVALID_HANDLE_VALUE;
    ULONG     bytesReturned;
    NETSV_CREATE_VIRTUAL_ADAPTER createAdapter;

    //
    // Generate the name of the control point
    //
    wsprintfW(
              AdapterName,
              L"\\\\.\\%s",
              &SymbolicName[12]
              );

    NOISE(("OpenAdapter(%ls)",AdapterName));

    //
    // Open the driver, check
    //
    hHostAdapter = CreateFileW(
                               AdapterName,
                               GENERIC_WRITE | GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL,
                               OPEN_EXISTING,
                               FILE_FLAG_OVERLAPPED,
                               0
                               );

    if (hHostAdapter == INVALID_HANDLE_VALUE)
    {
        WARN(("Failed to open Host adapter - not installed?"));
        goto Out;
    }

    //
    // Create a new adapter instance, starting low
    //
    createAdapter.fAdapterID = 0;
    
    if (DeviceIoControl(hHostAdapter,
                        (UINT32)IOCTL_CREATE_VIRTUAL_ADAPTER,
                        &createAdapter, sizeof(createAdapter),
                        &createAdapter, sizeof(createAdapter),
                        &bytesReturned,
                        NULL) == FALSE)
    {
        WARN(("Failed to create Host adapter instance"));
        goto Out;
    }

    //
    // Initialize the newly created instance
    //
    hVirtualAdapter = InitializeInstance(
                                         SymbolicName,
                                         AdapterName,
                                         createAdapter.fAdapterID);
    //
    // Done
    //
 Out:
    ///
    // Apparently, hHostAdapter must remain open. Sigh.
    //
    if (hVirtualAdapter == INVALID_HANDLE_VALUE)
    {
        CloseHandle(hHostAdapter);
        hHostAdapter = INVALID_HANDLE_VALUE;
    }
    *phAuxHandle = hHostAdapter;
    return hVirtualAdapter;
}

//=============================================================================
//    SubSection: VirtualPcDriver2::
//
//    Description: VirtualPC's Version 2 implementation
//=============================================================================

class VirtualPcDriver2 : public VirtualPcDriver {
public:
    VirtualPcDriver2(IN int        gDebug,
                     IN BOOL       gQuiet);
    virtual ~VirtualPcDriver2(void);

    virtual BOOL Open(IN const wchar_t *AdapterName);

    virtual BOOL Flush(void);

    virtual PACKET * AllocatePacket(IN BYTE *Buffer,
                                IN UINT Length,
                                IN BOOL fForReceive
                                );
    virtual void FreePacket(IN PACKET *Packet,
                            IN BOOL bForReceiving);

    virtual HRESULT PostReceivePacket(IN PACKET *Packet);
    virtual HRESULT PostTransmitPacket(IN PACKET *Packet);
    virtual PACKET_MODE GetNextCompletedPacket(OUT PACKET ** pPacket,
                                               IN  UINT32 TimeOutInMsec
                                               );
    virtual HRESULT SetFilter(IN UINT32 Filter)
    {
        return SetDriverFilter(hFileHandle,Filter);
    }

    virtual BOOL GetMaxOutstanding(OUT UINT32 *NumReads,
                                   OUT UINT32 *NumWrites)
    {
        *NumReads = 0; // unlimited
        *NumWrites = 0;
        return TRUE;
    }

    virtual BOOL GetSymbolicName(IN const wchar_t * AdapterName,
                                 OUT wchar_t      * SymbolicName);

private:
    //
    // Our private methods
    //

    //
    // Our private state
    //
    HANDLE AuxHandle;
    HANDLE IoCompletionPort;
    PacketManager PacketMgr;
    UINT8 EthernetAddress[6];
    BOOL bInitialized;
};



//=============================================================================
//    Method: VirtualPcDriver2::GetSymbolicName().
//
//    Description: Get the NT symbolic name for the packet driver we'll use.
//                 The driver supports multiple NICs, we'll go for the first
//                 that has an ethernet address assigned and is not in use.
//=============================================================================

BOOL
VirtualPcDriver2::GetSymbolicName(
    IN const wchar_t * AdapterName,
    OUT wchar_t      * SymbolicName
    )
{
    HANDLE        hControl;
    DWORD         BytesReturned;
    UINT32        version;
    wchar_t       *pDriverName;
    BOOL          bRet = FALSE;
    
    NOISE(("VirtualPcDriver2::GetSymbolicName()"));

    //
    // Open the driver's control point
    // Try first for the global space
    //
    pDriverName = NETSV_DRIVER_G_NAME_V2;
    hControl = CreateFileW(pDriverName,
                           GENERIC_READ | GENERIC_WRITE, 
                           0,//FILE_SHARE_READ|FILE_SHARE_WRITE
                           NULL, 
                           OPEN_EXISTING, 
                           FILE_FLAG_OVERLAPPED|FILE_FLAG_NO_BUFFERING, 
                           NULL);
                          
    if (hControl == INVALID_HANDLE_VALUE)
    {
        //
        // Did not work. Try for the older one
        //
        DWORD le = GetLastError();
        if (le == ERROR_FILE_NOT_FOUND) {
            pDriverName = NETSV_DRIVER_NAME_V2;
            hControl = CreateFileW(pDriverName,
                                   GENERIC_READ | GENERIC_WRITE, 
                                   0,//FILE_SHARE_READ|FILE_SHARE_WRITE,
                                   NULL, 
                                   OPEN_EXISTING, 
                                   FILE_FLAG_OVERLAPPED|FILE_FLAG_NO_BUFFERING, 
                                   NULL);
                          
            if (hControl != INVALID_HANDLE_VALUE)
                goto FoundInterface;
            le = GetLastError();
        }
        //
        // Really did not work, warn user and fail.
        //
        WARN(("Failed to open VirtualPC Packet Driver (%ls) - not installed (le=%d)?",
               pDriverName,le));
        return FALSE;
    }

    //
    // Get and check the version. Warn if strange.
    //
    FoundInterface:
    if (DeviceIoControl(hControl, 
                        (UINT32)IOCTL_GET_VERSION,
                        NULL, 0,
                        &version, sizeof(version),
                        &BytesReturned,
                        NULL) == FALSE)
    {
        WARN(("Bad VirtualPC Packet Driver - no version?!?"));
        goto Out;
    }
    SetVersion(version);
    if (version < kVPCNetSvVersion2)
    {
        WARN(("VirtualPC Packet Driver has unexpected version (Actual: %x, Expected: %x)", version, kVPCNetSvVersion2));
    }

    //
    // Select the adapter we will use
    //
    bRet = SelectAdapter(hControl,
                         AdapterName,
                         SymbolicName);
    //
    // and done.
    //
 Out:
    CloseHandle(hControl);

    return bRet;
}

//=============================================================================
//    Method: VirtualPcDriver2::Open().
//
//    Description: Open the packet driver.
//=============================================================================

BOOL
VirtualPcDriver2::Open(IN const wchar_t *AdapterName
    )
{
    BOOL        bResult;
    HRESULT     Result;
    wchar_t     SymbolicName[MAX_LINK_NAME_LENGTH];

    DPRINTF(("VirtualPcDriver2::Open()"));

    //
    //  Make sure we have an interface, lest we crash and burn later
    //
    bResult = GetSymbolicName(AdapterName,SymbolicName);

    if (bResult == FALSE)
    {
        WARN(("OpenPacketDriver: no %s NIC.",
               AdapterName ? "such" : "suitable"));
        return FALSE;
    }

    //
    //  Open the NDIS packet driver so we can send and receive.
    //
    this->hFileHandle = OpenAdapter(SymbolicName, &this->AuxHandle);

    if (this->hFileHandle == INVALID_HANDLE_VALUE)
    {
        WARN(("OpenPacketDriver: no suitable NIC."));
        return FALSE;
    }

    //
    //  Set up the capture filter.
    //
    Result = SetDriverFilter(
                    this->hFileHandle,
                    //NDIS_PACKET_TYPE_BROADCAST | 
                    //NDIS_PACKET_TYPE_MULTICAST |
                    //NDIS_PACKET_TYPE_ALL_MULTICAST |
                    NDIS_PACKET_TYPE_DIRECTED       
                    );

    //
    //  Get the ethernet address for this adapter.
    //
    bResult = GetMacAddress(this->EthernetAddress);
    if (!bResult)
    {
        WARN(("OpenPacketDriver: NIC has no MAC??"));
        CloseHandle(this->hFileHandle);
        if (this->AuxHandle != INVALID_HANDLE_VALUE)
            CloseHandle(this->AuxHandle);
        return FALSE;
    }

    DPRINTF(("%sPacketDriver MAC is %x:%x:%x:%x:%x:%x ", "V2_",
             this->EthernetAddress[0],
             this->EthernetAddress[1], 
             this->EthernetAddress[2],
             this->EthernetAddress[3], 
             this->EthernetAddress[4],
             this->EthernetAddress[5]));

    DPRINTF(("PacketDriver Version is %x",GetVersion()));

    //
    //  Create the I/O completion ports for send and
    //  receive operations to and from the packet driver.
    //
    this->IoCompletionPort = CreateIoCompletionPort(
                                        this->hFileHandle,
                                        NULL,
                                        (ULONG) this, 
                                        0
                                        );

    assert(this->IoCompletionPort != NULL);

    //
    // We are ready for work.
    //
    this->bInitialized = TRUE;

    return TRUE;
}

//=============================================================================
//    Method: VirtualPcDriver2::PostReceivePacket().
//
//    Description: Posts a packet for receiving.
//=============================================================================


HRESULT
VirtualPcDriver2::PostReceivePacket( 
    IN PACKET * Packet
    )
{
    HRESULT Result = S_OK;
    BOOL    bResult = FALSE;
    DWORD   nBytesTransferred = 0;

    LogIt("pkt::rp %p",(UINT_PTR)Packet);

    //
    //  Call NT ReadFile().
    //
    Packet->nBytesAvail = 0;
    Packet->Result = ERROR_IO_PENDING;

    Packet->Mode = PacketModeReceiving;

    bResult = ReadFile(
                    this->hFileHandle, 
                    Packet->Buffer, 
                    Packet->Length, 
                    &nBytesTransferred, 
                    &Packet->Overlapped
                    );
    
    //
    //  If the read request completed immediately,
    //  complete the pending request now.
    //
    if ( bResult != FALSE )
    {
        Packet->nBytesAvail = nBytesTransferred;
        Packet->Result = S_OK;

        return S_OK;
    }
    
    //
    //  The read request returned FALSE, check to see if its pending.
    //
    Result = GetLastError();

    if ( Result == ERROR_IO_PENDING ) 
    {
        return Result;
    } 

    //
    //  Check for EOF. 
    //
    if ( Result == ERROR_HANDLE_EOF ) 
    {
        assert(nBytesTransferred == 0);

        return S_FALSE;
    } 

    return Result;
}

//=============================================================================
//    Method: VirtualPcDriver2::PostTransmitPacket().
//
//    Description: Posts a packet for transmitting.
//=============================================================================


HRESULT
VirtualPcDriver2::PostTransmitPacket( 
    IN PACKET * Packet
    )
{
    HRESULT Result = S_OK;
    BOOL    bResult = FALSE;
    DWORD   nBytesTransferred = 0;

    LogIt("pkt::xp %p %u",(UINT_PTR)Packet,Packet->nBytesAvail);

    //
    // Make sure data was prepared
    //
    if (Packet->Mode != PacketModeTransmitting)
        Packet->Mode = PacketModeTransmitting;

    //
    //  Call NT WriteFile().
    //
    Packet->Result = ERROR_IO_PENDING;

    bResult = WriteFile(
                    this->hFileHandle, 
                    Packet->Buffer, 
                    Packet->nBytesAvail, 
                    &nBytesTransferred, 
                    &Packet->Overlapped
                    );
    
    //
    //  If the write request completed immediately,
    //  complete the pending request now.
    //
    if ( bResult != FALSE )
    {
        Packet->Result = S_OK;

        return S_OK;
    }
    
    //
    //  The write request returned FALSE, check to see if its pending.
    //
    Result = GetLastError();

    if ( Result == ERROR_IO_PENDING ) 
    {
        return Result;
    } 

    //
    //  Check for EOF. 
    //
    if ( Result == ERROR_HANDLE_EOF ) 
    {
        assert(nBytesTransferred == 0);


        return S_FALSE;
    } 

    return Result;
}

//=============================================================================
//    Method: VirtualPcDriver2::GetNextCompletedPacket().
//
//    Description: Dequeues the first packet that has completed, Maybe waits.
//=============================================================================

PACKET_MODE
VirtualPcDriver2::GetNextCompletedPacket( 
    OUT PACKET ** pPacket,
    IN  UINT32 TimeOutInMsec
    )
{
    DWORD           Transferred;
    PACKET *        Packet;
    BOOL            bResult;
    OVERLAPPED *    Overlapped;
#if (_MSC_VER > 1200)
    ULONG_PTR       Key;
#else
    UINT_PTR        Key;
#endif

    //
    //  Wait for an I/O to complete.
    //
 Retry:
    Packet = NULL;
    Overlapped = NULL;

    bResult = GetQueuedCompletionStatus(
                            this->IoCompletionPort,
                            &Transferred,
                            &Key,
                            &Overlapped,
                            TimeOutInMsec
                            );

    if (!bResult ||(Overlapped == NULL)) {
        //DWORD le = GetLastError();
        LogIt("pkt:to");
        return PacketModeInvalid;
    }

    //
    //  If the completion was ok, move the packet to the 
    //  user for further processing. Check.
    //
    Packet = (PACKET *) Overlapped;

    //
    //  This can happen if the user is ignoring xmit completions.
    //  The user might have received an indication that the packet has been
    //  received and went ahead and put it back on the free queue.
    //  We'll help the daring user by ignoring the packet.
    //  Note that the packet might have gone as far as being re-posted.
    //  In this case our caller has better be dropping it on the floor too.
    //  
    if (Packet->Mode == PacketModeInvalid)
        goto Retry;

    if ( Packet->Mode == PacketModeReceiving )
    {
        //
        //  Finish initializing the pending packet.
        //  
        Packet->nBytesAvail = Transferred;
        Packet->Result = S_OK;

        //
        //  Move it on up to the application.
        //
        *pPacket = Packet;
        LogIt("pkt::rc %p",(UINT_PTR)Packet);
        return PacketModeReceiving;
    }

    assert((Packet->Mode == PacketModeTransmitting) ||
           (Packet->Mode == PacketModeTransmittingBuffer));

    //
    //  Mark the transmission complete
    //
    *pPacket = Packet;
    LogIt("pkt::xc %p",(UINT_PTR)Packet);
    return PacketModeTransmitting;
}

//=============================================================================
//    Method: VirtualPcDriver2::Flush().
//
//    Description: Flush all pending I/Os, to recover packets in flight
//=============================================================================

BOOL
VirtualPcDriver2::Flush( 
    void
    )
{
    PACKET *Packet;
    DWORD           Transferred;
    BOOL            bResult;
    OVERLAPPED *    Overlapped;
#if (_MSC_VER > 1200)
    ULONG_PTR       Key;
#else
    UINT_PTR        Key;
#endif

    if (!bInitialized)
        return FALSE;

    /* Flush all pending I/Os
     */
    CancelIo(this->hFileHandle);

    for (;;) 
    {
        //
        //  Wait for an I/O to complete.
        //
        Packet = NULL;
        Overlapped = NULL;

        bResult = GetQueuedCompletionStatus(
                            this->IoCompletionPort,
                            &Transferred,
                            &Key,
                            &Overlapped,
                            0
                            );

        //
        //  If the completion was ok, free the packet. Else done.
        //
        if (!bResult || (Overlapped == NULL))
            break;

        Packet = (PACKET *) Overlapped;

        //
        // Should we deallocate the buffer also?
        //
        if (Packet->Mode == PacketModeTransmittingBuffer)
        {
            delete Packet->Buffer;
            Packet->Buffer = NULL;
        }

        PacketMgr.Free(Packet);
    }

    return TRUE;
}


//=============================================================================
//    Method: VirtualPcDriver2::AllocatePacket().
//
//    Description: Allocates one packet, either for xmit or recv.
//=============================================================================

PACKET *
VirtualPcDriver2::AllocatePacket( 
    IN BYTE *Buffer,
    IN UINT Length,
    IN BOOL fForReceive
    )
{
    //
    // Get a packet from the packet manager
    //
    PACKET *newPacket = PacketMgr.Allocate();
    if (newPacket == NULL)
        return NULL;

    //
    // Check that a buffer is assigned to the packet.
    // If not get a new one
    //
    BYTE *oldBuffer = newPacket->Buffer;

    if (Buffer == NULL) {
        if (oldBuffer == NULL) {
            // always max size it
            Buffer = ::new BYTE[kVPCNetSvMaximumPacketLength];
            if (Buffer == NULL) {
                //
                // We must be out of memory, fail.
                //
                PacketMgr.Free(newPacket);
                return NULL;
            }
        } else
            Buffer = oldBuffer;
    }        

    //
    // Initialize the new packet
    //
    newPacket->Init(Buffer,Length);
    newPacket->Mode = (fForReceive) ? PacketModeReceiving : PacketModeTransmitting;

    LogIt((fForReceive) ? "pkt::ra %p" : "pkt::xa %p",
          (UINT_PTR)newPacket);

    return newPacket;
}

//=============================================================================
//    Method: VirtualPcDriver2::FreePacket().
//
//    Description: Return a packet to the (proper) free list
//=============================================================================


void
VirtualPcDriver2::FreePacket( 
    IN PACKET * Packet,
    IN BOOL     bForReceiving
    )
{
    //
    // Return it to the packet manager
    //
    UnusedParameter(bForReceiving);
    LogIt((bForReceiving) ? "pkt::rf %p %u" : "pkt::xf %p %u",
          (UINT_PTR)Packet,Packet->nBytesAvail);
    PacketMgr.Free(Packet);
}

//=============================================================================
//  Constructor: VirtualPcDriver2()
//
//=============================================================================
VirtualPcDriver2::VirtualPcDriver2(
     IN int        gDebug,
     IN BOOL       gQuiet
     )
{
    Debug = gDebug;
    Quiet = gQuiet;
    hFileHandle = AuxHandle = IoCompletionPort = INVALID_HANDLE_VALUE;
    memset(EthernetAddress,0,6);
    bInitialized = FALSE;
}

//=============================================================================
//  Destructor: VirtualPcDriver2()
//
//=============================================================================
VirtualPcDriver2::~VirtualPcDriver2(void)
{
    if (!this->bInitialized)
        return;

    //
    // Stop and reclaim all packets
    //
    // Actually.. we are throwing everything away so what's the point.
    //Flush();

    //
    // Close handles
    //
    if (this->hFileHandle != INVALID_HANDLE_VALUE)
        CloseHandle(this->hFileHandle);
    this->hFileHandle = INVALID_HANDLE_VALUE;

    if (this->AuxHandle != INVALID_HANDLE_VALUE)
        CloseHandle(this->AuxHandle);
    this->AuxHandle = INVALID_HANDLE_VALUE;

    //
    // Close the completion port 
    //
    CloseHandle(this->IoCompletionPort);
    this->IoCompletionPort = INVALID_HANDLE_VALUE;

    //
    // Done
    //
    bInitialized = FALSE;
}

//=============================================================================
//    SubSection: VirtualPc3Driver::
//
//    Description: VirtualPC's Version 3 implementation
//
//    Notes:
//    The xmt and recv PacketBufferDesc have 3 queues: free, ready, complete.
//    Xmit cycles USER(complete->ready) and KERN(ready->complete)
//    Recv cycles KERN(free->ready) and USER(ready->free)
//    We keep the idle packets in the Xmit.free, and Recv.complete
//=============================================================================

//
// but first.. a trivial helper class to help with completion queues
//
class PacketCompletions {
public:
    //
    // Give us a list of packets, but only when we are empty
    //
    void Push(IN VPCNetSvPacketEntryPtr FifoHead)
    {
        assert(mHead == NULL);
        mHead = FifoHead;
    }

    //
    // Take the first packet off the list
    //
    VPCNetSvPacketEntryPtr Pop(void)
    {
        VPCNetSvPacketEntryPtr Packet = mHead;
        if (Packet != NULL) {
            mHead = Packet->fNextPacket;
            Packet->fNextPacket = NULL;
        }
        return Packet;
    }

    //
    // Length of the list
    //
    UINT Length(void)
    {
        UINT l = 0;
        VPCNetSvPacketEntryPtr Packet = mHead;
        while (Packet != NULL) {
            l++;
            Packet = Packet->fNextPacket;
        }
        return l;
    }

    //
    // Constructor
    //
    PacketCompletions(void)
    {
        mHead = NULL;
    }

private:
    VPCNetSvPacketEntryPtr mHead;
};

//
// VPC Interface proper
//
class VirtualPcDriver3 : public VirtualPcDriver {
public:
    VirtualPcDriver3(IN int        gDebug,
                     IN BOOL       gQuiet);
    virtual ~VirtualPcDriver3(void);

    virtual BOOL Open(IN const wchar_t *AdapterName);

    virtual BOOL Flush(void);

    virtual PACKET * AllocatePacket(IN BYTE *Buffer,
                                    IN UINT Length,
                                    IN BOOL fForReceive
                                    );
    virtual void FreePacket(IN PACKET *Packet,
                            IN BOOL bForReceiving);

    virtual HRESULT PostReceivePacket(IN PACKET *Packet);
    virtual HRESULT PostTransmitPacket(IN PACKET *Packet);
    virtual PACKET_MODE GetNextCompletedPacket(OUT PACKET ** pPacket,
                                               IN  UINT32 TimeOutInMsec
                                               );
    virtual PACKET *GetNextReceivedPacket(IN UINT32 TimeOutInMsec);
    virtual HRESULT SetFilter(IN UINT32 Filter)
    {
        return SetDriverFilter(hFileHandle,Filter);
    }

    virtual BOOL GetMaxOutstanding(OUT UINT32 *NumReads,
                                   OUT UINT32 *NumWrites)
    {
        if (!bInitialized)
            return false;
        *NumReads  = MaxRecvOutstanding;
        *NumWrites = MaxXmitOutstanding;
        return TRUE;
    }

    virtual BOOL GetSymbolicName(IN const wchar_t * AdapterName,
                                 OUT wchar_t      * SymbolicName);

private:
    //
    // Our private methods
    //
    void InitializePacketBuffer(
         IN VPCNetSvPacketBufferDescPtr    PacketBufferDesc,
         IN BOOL fForReceive);

    void PacketBufferEnqueue(
         IN VPCNetSvPacketBufferDescPtr    PacketBufferDesc,
         IN VPCNetSvPacketEntryQueueHead * PacketQueueHead,
         IN VPCNetSvPacketEntry *          PacketEntry);

    VPCNetSvPacketEntryPtr PacketBufferDequeue(
         IN VPCNetSvPacketBufferDescPtr    PacketBufferDesc,
         IN VPCNetSvPacketEntryQueueHead * PacketQueueHead );

    VPCNetSvPacketEntryPtr PacketBufferSteal(
         IN VPCNetSvPacketBufferDescPtr    PacketBufferDesc,
         IN VPCNetSvPacketEntryQueueHead * PacketQueueHead,
         OUT UINT32                      * ListCount,
         IN bool                           fReverse);

    inline VPCNetSvPacketEntryPtr PacketBufferStealAndReverse(
         IN VPCNetSvPacketBufferDescPtr    PacketBufferDesc,
         IN VPCNetSvPacketEntryQueueHead * PacketQueueHead,
         OUT UINT32                      * ListCount)
    {
        return PacketBufferSteal(PacketBufferDesc,PacketQueueHead,ListCount,true);
    }

    void PacketBufferMoveList(
         IN VPCNetSvPacketBufferDescPtr    PacketBufferDesc,
         IN VPCNetSvPacketEntryQueueHead * PacketQueueHead,
         IN UINT32                         PacketEntryOffset);

    inline bool PacketBufferIsQueueEmpty(
         IN VPCNetSvPacketBufferDescPtr    PacketBufferDesc,
         IN VPCNetSvPacketEntryQueueHead * PacketQueueHead )
    {
        UINT32 EntryOffset = PacketQueueHead->fHead;
        return !IsValidPacketEntryOffset(PacketBufferDesc, EntryOffset);
    }

    //
    // BUGBUG allocate/free for single-threaded does *not* really need atomic ops
    //
    VPCNetSvPacketEntryPtr AllocateTransmitPacketEntry(void)
    {
        return PacketBufferDequeue(&mTxBuffer,
                                   &mTxBuffer.fPacketBuffer->fFreeBufferQueue);
    }

    VPCNetSvPacketEntryPtr AllocateReceivePacketEntry(void)
    {
        return PacketBufferDequeue(&mRxBuffer,
                                   &mRxBuffer.fPacketBuffer->fCompletedBufferQueue);
    }

    void FreeTransmitPacketEntry(IN VPCNetSvPacketEntry * PacketEntry)
    {
        PacketBufferEnqueue(&mTxBuffer,
                            &mTxBuffer.fPacketBuffer->fFreeBufferQueue,
                            PacketEntry);
    }

    void FreeReceivePacketEntry(IN VPCNetSvPacketEntry * PacketEntry)
    {
        PacketBufferEnqueue(&mRxBuffer,
                            &mRxBuffer.fPacketBuffer->fCompletedBufferQueue,
                            PacketEntry);
    }

    void PostReceivePacketEntry(IN VPCNetSvPacketEntry * PacketEntry)
    {
        PacketBufferEnqueue(&mRxBuffer,
                            &mRxBuffer.fPacketBuffer->fFreeBufferQueue,
                            PacketEntry);
    }

    void PostTransmitPacketEntry(IN VPCNetSvPacketEntry * PacketEntry)
    {
        PacketBufferEnqueue(&mTxBuffer,
                            &mTxBuffer.fPacketBuffer->fReadyBufferQueue,
                            PacketEntry);
    }

    //
    // Our private state
    //
    HANDLE AuxHandle;
    static const int nEvents = 2;
    enum {
        iXmitEvent = 0,
        iRecvEvent = 1
    };
    HANDLE hEvents[nEvents];
    VPCNetSvPacketBufferDesc mTxBuffer;
    VPCNetSvPacketBufferDesc mRxBuffer;
    PacketManager PacketMgr;
    PacketCompletions ReceiveCompleted;
    PacketCompletions TransmitCompleted;

    UINT8 EthernetAddress[6];
    BOOL bInitialized;
    UINT32 MaxRecvOutstanding;
    UINT32 MaxXmitOutstanding;
};

//=============================================================================
//    Method: VirtualPcDriver3::GetSymbolicName().
//
//    Description: Get the NT symbolic name for the packet driver we want.
//                 The driver supports multiple ones, we'll go for the first
//                 that has an ethernet address assigned and is not in use.
//=============================================================================

BOOL
VirtualPcDriver3::GetSymbolicName(
    IN const wchar_t * AdapterName,
    OUT wchar_t      * SymbolicName
    )
{
    void *        hControl;
    DWORD         BytesReturned;
    UINT32        version, versionExpected;
    wchar_t       *pDriverName;
    BOOL          bRet = FALSE;
    
    NOISE(("VirtualPcDriver3::GetSymbolicName()"));

    //
    // Open the driver's control point
    // Try first for the newest control point
    //
    pDriverName = NETSV_DRIVER_NAME_V3;
    versionExpected = kVPCNetSvVersion3;

    hControl = CreateFileW(pDriverName, 
                           GENERIC_READ | GENERIC_WRITE, 
                           0,//FILE_SHARE_READ|FILE_SHARE_WRITE,
                           NULL, 
                           OPEN_EXISTING, 
                           FILE_FLAG_OVERLAPPED|FILE_FLAG_NO_BUFFERING, 
                           NULL);
                          
    if (hControl == INVALID_HANDLE_VALUE)
    {
        //
        // Did not work. Try for the older one
        //
        DWORD le = GetLastError();

#define SUPPORT_V3_ON_S2 1
#if SUPPORT_V3_ON_S2 
        if (le == ERROR_FILE_NOT_FOUND) {
            pDriverName = NETSV_DRIVER_G_NAME_V2;
            versionExpected = kVPCNetSvVersion2;
            hControl = CreateFileW(pDriverName,
                                   GENERIC_READ | GENERIC_WRITE, 
                                   0,//FILE_SHARE_READ|FILE_SHARE_WRITE,
                                   NULL, 
                                   OPEN_EXISTING, 
                                   FILE_FLAG_OVERLAPPED|FILE_FLAG_NO_BUFFERING, 
                                   NULL);
                          
            if (hControl != INVALID_HANDLE_VALUE)
                goto FoundInterface;
            le = GetLastError();
        }
#endif
        //
        // Really did not work, warn user and fail.
        //
        WARN(("Failed to open VirtualPC Packet Driver (%ls) - not installed (le=%d)?",
               pDriverName,le));
        return FALSE;
    }

    //
    // We have an interface. Check the version against what we expect. Warn if weird.
    //
#if SUPPORT_V3_ON_S2 
    FoundInterface:
#endif
    if (DeviceIoControl(hControl, 
                        (UINT32)IOCTL_GET_VERSION,
                        NULL, 0,
                        &version, sizeof(version),
                        &BytesReturned,
                        NULL) == FALSE)
    {
        WARN(("Bad VirtualPC Packet Driver - no version?!?"));
        goto Out;
    }
    SetVersion(version);
    if (version < versionExpected)
    {
        WARN(("VirtualPC Packet Driver has unexpected version (Actual: %x, Expected: %x)", version, versionExpected));
    }

    //
    // Select the adapter we will use
    //
    bRet = SelectAdapter(hControl,
                         AdapterName,
                         SymbolicName);
    //
    // and done.
    //
 Out:
    CloseHandle(hControl);

    return bRet;
}

//=============================================================================
//    Method: VirtualPcDriver3::Open().
//
//    Description: Open the packet driver.
//=============================================================================

BOOL
VirtualPcDriver3::Open(IN const wchar_t *AdapterName
    )
{
    BOOL        bResult;
    HRESULT     Result;
    wchar_t     SymbolicName[MAX_LINK_NAME_LENGTH];

    DPRINTF(("VirtualPcDriver3::Open()"));

    //
    //  Make sure we have an interface, lest we crash and burn later
    //
    bResult = GetSymbolicName(AdapterName,SymbolicName);

    if (bResult == FALSE)
    {
        WARN(("OpenPacketDriver: no %s NIC.",
               AdapterName ? "such" : "suitable"));
        return FALSE;
    }

    NOISE(("VirtualPcDriver3::symname is %ls",SymbolicName));

    //
    //  Open the NDIS packet driver so we can queue/dequeue packets.
    //
    this->hFileHandle = OpenAdapter(SymbolicName, &this->AuxHandle);

    if (this->hFileHandle == INVALID_HANDLE_VALUE)
    {
        WARN(("OpenPacketDriver: no suitable NIC."));
        return FALSE;
    }

    //
    //  Get the ethernet address for this adapter.
    //
    bResult = GetMacAddress(this->EthernetAddress);
    if (!bResult)
    {
        WARN(("OpenPacketDriver: NIC has no MAC??"));
        goto Bad;
    }

    DPRINTF(("%sPacketDriver MAC is %x:%x:%x:%x:%x:%x ", "V3_",
             this->EthernetAddress[0],
             this->EthernetAddress[1], 
             this->EthernetAddress[2],
             this->EthernetAddress[3], 
             this->EthernetAddress[4],
             this->EthernetAddress[5]));

    DPRINTF(("PacketDriver Version is %x",GetVersion()));

    //
    //  No captures yet
    //
    Result = SetDriverFilter(this->hFileHandle,0);

    //
    // Create the synchronization events
    //
    hEvents[iXmitEvent] = CreateEvent(NULL,FALSE,FALSE,NULL);
    hEvents[iRecvEvent] = CreateEvent(NULL,FALSE,FALSE,NULL);

    NETSV_CREATE_PACKET_BUFFERS    bufferInfo;
    memset(&bufferInfo,0,sizeof bufferInfo);
    
    bufferInfo.fInput.fPacketComplete = hEvents[iXmitEvent];
    bufferInfo.fInput.fPacketReceived = hEvents[iRecvEvent];

    //
    // Allocate the packet buffers
    //
    DWORD length = sizeof(bufferInfo);
    bResult = DeviceIoControl(
                             this->hFileHandle,
                             (DWORD) IOCTL_CREATE_PACKET_BUFFERS,
                             &bufferInfo,
                             sizeof(bufferInfo),
                             &bufferInfo,
                             length,
                             &length,
                             NULL);

    if (!bResult) {
        WARN(("OpenPacketDriver: CreatePacketBuffers failed?? (le=%d).",
               GetLastError()));
        goto Bad;
    }

    mTxBuffer     = bufferInfo.fOutput.fTxBuffer;
    mRxBuffer     = bufferInfo.fOutput.fRxBuffer;

    NOISE(("TxBuffer: %p %d %u %u",
             mTxBuffer.fPacketBuffer,
             mTxBuffer.fLength,
             mTxBuffer.fMinimumEntryOffset,
             mTxBuffer.fMaximumEntryOffset));
    NOISE(("RxBuffer: %p %d %u %u",
             mRxBuffer.fPacketBuffer,
             mRxBuffer.fLength,
             mRxBuffer.fMinimumEntryOffset,
             mRxBuffer.fMaximumEntryOffset));

    //
    // Initialize the packet buffers
    //
    InitializePacketBuffer(&mTxBuffer,FALSE);
    InitializePacketBuffer(&mRxBuffer,TRUE);

    //
    //  Set up the operational capture filter.
    //
    Result = SetDriverFilter(
                    this->hFileHandle,
                    //NDIS_PACKET_TYPE_BROADCAST | 
                    //NDIS_PACKET_TYPE_MULTICAST |
                    //NDIS_PACKET_TYPE_ALL_MULTICAST |
                    NDIS_PACKET_TYPE_DIRECTED       
                    );

    //
    // We are working
    //
    this->bInitialized = TRUE;

    return TRUE;

    //
    // Failed, close all handles.
    //
 Bad:
    CloseHandle(this->hFileHandle);
    if (this->AuxHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(this->AuxHandle);
        this->AuxHandle = INVALID_HANDLE_VALUE;
    }
    for (int i = 0; i < nEvents; i++) {
        if (hEvents[i] != INVALID_HANDLE_VALUE)
            CloseHandle(hEvents[i]);
        hEvents[i] = INVALID_HANDLE_VALUE;
    }

    return FALSE;
}

//=============================================================================
//    Method: VirtualPcDriver3::PacketBufferDequeue().
//
//    Description: Take one packet off the given queue.
//=============================================================================

VPCNetSvPacketEntryPtr
VirtualPcDriver3::PacketBufferDequeue(
    IN VPCNetSvPacketBufferDescPtr     PacketBufferDesc,
    IN VPCNetSvPacketEntryQueueHead *  PacketQueueHead )
{
    VPCNetSvPacketBufferPtr packetBuffer    = PacketBufferDesc->fPacketBuffer;
    UINT32 minimumOffset                    = PacketBufferDesc->fMinimumEntryOffset;
    UINT32 maximumOffset                    = PacketBufferDesc->fMaximumEntryOffset;
    VPCNetSvPacketEntryPtr packetEntry      = NULL;
    UINT32 entryOffset                      = 0;

    //
    // Lock-free operation. Loop until we succeed, usually at first try.
    //
    for(;;)
    {
        volatile UINT32 head            = PacketQueueHead->fHead;
        volatile UINT32 modCount        = PacketQueueHead->fModificationCount;
        volatile UINT32 newModCount     = modCount + 1;

        entryOffset = head;

        //
        // Sanity check against corruptions
        //
        if ((head < minimumOffset) || (head > maximumOffset))
        {
            break;
        }
        
        volatile __int64 comparand = ((UINT64)head | ((UINT64)modCount << 32));
        volatile __int64 exchange  = (((UINT64)(((VPCNetSvPacketEntryPtr)
             ((UINT8*)packetBuffer + head))->fNextPacketOffset)) | ((UINT64)newModCount << 32));
        volatile __int64 *dest     = (__int64 *)PacketQueueHead;

        //
        // Try to do the assignment now.
        //
        if (MyInterlockedCompareExchange64(dest, exchange, comparand) == comparand)
        {
            break;
        }
    }

    //
    // Map to a pointer
    //
    packetEntry = GetPacketEntryPointerFromOffset(PacketBufferDesc, entryOffset);

    if (packetEntry != NULL)
    {
        packetEntry->fNextPacket = NULL;
    }
    
    return packetEntry;
}

//=============================================================================
//    Method: VirtualPcDriver3::PacketBufferEnqueue().
//
//    Description: Add one packet to the given queue.
//=============================================================================

void
VirtualPcDriver3::PacketBufferEnqueue(
    IN VPCNetSvPacketBufferDescPtr       PacketBufferDesc,
    IN VPCNetSvPacketEntryQueueHead *    PacketQueueHead,
    IN VPCNetSvPacketEntry *             PacketEntry)
{
    //
    // Sanity checks
    //
    if ( PacketBufferDesc == NULL || PacketEntry == NULL )
    {
        return;
    }

    UINT32 entryOffset = GetPacketEntryOffsetFromPointer(PacketBufferDesc, PacketEntry);

    if (!IsValidPacketEntryOffset(PacketBufferDesc, entryOffset))
    {
        return;
    }

    //
    // Lock-free operation. Loop until we succeed, usually at first try.
    //
    for (;;)
    {
        volatile UINT32 head            = PacketQueueHead->fHead;
        volatile UINT32 modCount        = PacketQueueHead->fModificationCount;
        volatile UINT32 newModCount     = modCount + 1;

        //
        // Queue is a LIFO queue
        //
        PacketEntry->fNextPacketOffset    = head;

        volatile __int64 comparand = ((UINT64)head | ((UINT64)modCount << 32));
        volatile __int64 exchange  = ((UINT64)entryOffset | ((UINT64)newModCount << 32));
        volatile __int64 *dest     = (__int64 *)PacketQueueHead;

        //
        // Try to do the assignment now.
        //
        if (MyInterlockedCompareExchange64(dest, exchange, comparand) == comparand)
        {
            break;
        }
    }
}

//=============================================================================
//    Method: VirtualPcDriver3::PacketBufferMoveList().
//
//    Description: Move a list of packets to the given queue.
//                 The list is by linked by offsets (NOT pointers) e.g. in driver-mode.
//=============================================================================

void
VirtualPcDriver3::PacketBufferMoveList(
    IN VPCNetSvPacketBufferDescPtr       PacketBufferDesc,
    IN VPCNetSvPacketEntryQueueHead *    PacketQueueHead,
    IN UINT32                            PacketEntryOffset)
{
    //
    // Sanity checks
    //
    if ( PacketBufferDesc == NULL || !IsValidPacketEntryOffset(PacketBufferDesc, PacketEntryOffset) )
    {
        return;
    }

    //
    // If the queue is empty its easier
    //
    while (PacketBufferIsQueueEmpty(PacketBufferDesc,PacketQueueHead))
    {
        volatile UINT32 head            = PacketQueueHead->fHead;
        volatile UINT32 modCount        = PacketQueueHead->fModificationCount;
        volatile UINT32 newModCount     = modCount + 1;

        //
        // Do not touch the new list, we must assume it properly terminated.
        //

        //
        volatile __int64 comparand = ((UINT64)head | ((UINT64)modCount << 32));
        volatile __int64 exchange  = ((UINT64)PacketEntryOffset | ((UINT64)newModCount << 32));
        volatile __int64 *dest     = (__int64 *)PacketQueueHead;

        //
        // Try to do the assignment now.
        //
        if (MyInterlockedCompareExchange64(dest, exchange, comparand) == comparand)
        {
            return;
        }
    }
    
    //
    // Slower path. Scan the list to find its tail.
    //
    VPCNetSvPacketEntry *tailPacketEntry = GetPacketEntryPointerFromOffset(PacketBufferDesc,PacketEntryOffset);
    for (;;)
    {
        VPCNetSvPacketEntry *nextPacketEntry = GetPacketEntryPointerFromOffset(PacketBufferDesc,tailPacketEntry->fNextPacketOffset);
        if (nextPacketEntry == NULL)
            break;
        tailPacketEntry = nextPacketEntry;
    }


    //
    // Lock-free operation. Loop until we succeed, usually at first try.
    //
    for (;;)
    {
        volatile UINT32 head            = PacketQueueHead->fHead;
        volatile UINT32 modCount        = PacketQueueHead->fModificationCount;
        volatile UINT32 newModCount     = modCount + 1;

        //
        // Queue is a LIFO queue
        //
        tailPacketEntry->fNextPacketOffset = head;

        volatile __int64 comparand = ((UINT64)head | ((UINT64)modCount << 32));
        volatile __int64 exchange  = ((UINT64)PacketEntryOffset | ((UINT64)newModCount << 32));
        volatile __int64 *dest     = (__int64 *)PacketQueueHead;

        //
        // Try to do the assignment now.
        //
        if (MyInterlockedCompareExchange64(dest, exchange, comparand) == comparand)
        {
            break;
        }
    }
}

//=============================================================================
//    Method: VirtualPcDriver3::PacketBufferSteal().
//
//    Description: Empty the queue, return the possibly reversed (LIFO->FIFO) content
//=============================================================================

VPCNetSvPacketEntryPtr
VirtualPcDriver3::PacketBufferSteal(
    IN VPCNetSvPacketBufferDescPtr       PacketBufferDesc,
    IN VPCNetSvPacketEntryQueueHead *    PacketQueueHead,
    OUT UINT32                      *    ListCount,
    IN bool                              fReverse
    )
{
    UINT32 currentOffset    = 0;
    UINT32 minimumOffset    = PacketBufferDesc->fMinimumEntryOffset;
    UINT32 maximumOffset    = PacketBufferDesc->fMaximumEntryOffset;

    //
    // Lock-free operation. Loop until we succeed, usually at first try.
    //
    for(;;)
    {
        volatile UINT32 head            = PacketQueueHead->fHead;
        volatile UINT32 modCount        = PacketQueueHead->fModificationCount;
        volatile UINT32 newModCount     = modCount + 1;

        currentOffset = head;

        //
        // Sanity check against corruptions
        //
        if ((currentOffset < minimumOffset) || (currentOffset > maximumOffset))
        {
            break;
        }
        
        volatile __int64 comparand    = ((UINT64)head | ((UINT64)modCount << 32));
        volatile __int64 exchange     = ((0) | ((UINT64)newModCount << 32));
        volatile __int64 *dest        = (__int64 *)PacketQueueHead;

        //
        // Try to do the assignment now.
        //
        if (MyInterlockedCompareExchange64(dest, exchange, comparand) == comparand)
        {
            break;
        }
    }

    //
    // Reverse the list so that the packets are in FIFO order and convert
    // the offsets to pointers.
    //
    VPCNetSvPacketEntryPtr reversedList = 
        GetPacketEntryPointerFromOffset(PacketBufferDesc,
                                        currentOffset);
    //
    // We might be done already..
    //
    if (!fReverse)
        return reversedList;

    VPCNetSvPacketEntryPtr entryList    = NULL;
    UINT32 nDequeued                    = 0;

    NOISE(("\nReversing %x,%p ",currentOffset,reversedList));
    while (reversedList != NULL)
    {
        VPCNetSvPacketEntryPtr current;

        current      = reversedList;
        reversedList = 
            GetPacketEntryPointerFromOffset(PacketBufferDesc,
                                            current->fNextPacketOffset);
        
        NOISE(("%x,%p ",current->fNextPacketOffset,reversedList));

        current->fNextPacket     = entryList;
        entryList                 = current;
        nDequeued++;
    }

    //
    // Say how many we dequeued and done
    //
    *ListCount = nDequeued;
    return entryList;
}


//=============================================================================
//    Method: VirtualPcDriver3::InitializePacketBuffer().
//
//    Description: Initialize the given packet buffer area.
//=============================================================================

void
VirtualPcDriver3::InitializePacketBuffer(
    IN VPCNetSvPacketBufferDescPtr    PacketBufferDesc,
    IN BOOL fForReceive
    )
{
    UINT32 nEnqueued = 0;

    //
    // We actually seem to get an active area, we need it quiet.
    // So in the off-chance the driver was messing with queues.. zap twice. 
    //
    memset(PacketBufferDesc->fPacketBuffer, 0, PacketBufferDesc->fLength);
    memset(PacketBufferDesc->fPacketBuffer, 0, PacketBufferDesc->fLength);

    //
    // Put all packets in the proper idle queue
    //
    NOISE(("init:"));
    for (UINT32 offset = PacketBufferDesc->fMinimumEntryOffset;
         offset <= PacketBufferDesc->fMaximumEntryOffset;
         offset += sizeof(VPCNetSvPacketEntry))
    {
        //
        // Get a pointer to the entry
        //
        VPCNetSvPacketEntryPtr packetEntry = 
            GetPacketEntryPointerFromOffset(PacketBufferDesc, offset);

        //
        // Maximum length always. Enque.
        //
        packetEntry->fPacketLength = kVPCNetSvMaximumPacketLength;

        PacketBufferEnqueue(PacketBufferDesc,
                            (fForReceive) ?
                              &PacketBufferDesc->fPacketBuffer->fCompletedBufferQueue :
                              &PacketBufferDesc->fPacketBuffer->fFreeBufferQueue,
                            packetEntry);
        NOISE((" %x,%p",offset,packetEntry));

        nEnqueued++;
    }
    DPRINTF(("InitializePacketBuffer() queued %d packets",nEnqueued));

    //
    // Here we count how many packets we can have outstanding
    //
    if (fForReceive)
        MaxRecvOutstanding = nEnqueued;
    else
        MaxXmitOutstanding = nEnqueued;

}

//=============================================================================
//    Method: VirtualPcDriver3::AllocatePacket().
//
//    Description: Allocates one packet, either for xmit or recv.
//=============================================================================

PACKET * 
VirtualPcDriver3::AllocatePacket( 
    IN BYTE *Buffer,
    IN UINT Length,
    IN BOOL fForReceive
    )
{
    UnusedParameter(Buffer);

    //
    // Get a new packet from the packet manager
    //
    PACKET *newPacket = PacketMgr.Allocate();
    if (newPacket == NULL)
        return NULL;

    //
    // Get a packet entry from the proper idle queue, check.
    //
    VPCNetSvPacketEntryPtr packetEntry = (fForReceive) ?
            AllocateReceivePacketEntry() : AllocateTransmitPacketEntry();
    if (packetEntry == NULL)
    {
        //
        // Ran out of buffers. For receive there's nothing we can do.
        //
        if (fForReceive)
            goto Bad;
        //
        // For xmit, we can look at the completed queue. This assumes that the user
        // is ignoring xmit completions (as SIRC client does). Otherwise... trouble.
        // [BUGBUG There should be an explicit user-settable flag to control this?]
        //
        UINT32 nUnused = 0;
        packetEntry = 
            PacketBufferSteal(&mTxBuffer,
                              &mTxBuffer.fPacketBuffer->fCompletedBufferQueue,
                              &nUnused,
                              false);
        if (packetEntry == NULL) {
            //
            // There was nothing there.
            //
            goto Bad;
        }

        //
        // The rest of the packets go onto the free list. NB: List is empty!
        //
        PacketBufferMoveList(&mTxBuffer,
                             &mTxBuffer.fPacketBuffer->fFreeBufferQueue,
                             packetEntry->fNextPacketOffset);
    }

    LogIt((fForReceive) ? "pkt::ra %p" : "pkt::xa %p",
          (UINT_PTR)packetEntry);

    //
    // Initialize the packet entry
    //
    packetEntry->fPacketLength = (Length > kVPCNetSvMaximumPacketLength) ?
        kVPCNetSvMaximumPacketLength : Length;
    packetEntry->fAppRefCon    = newPacket;

    //
    // Initialize the packet
    //
    newPacket->Init(packetEntry->fPacketData,Length);
    newPacket->DriverState = packetEntry;
    newPacket->Mode = (fForReceive) ? PacketModeReceiving : PacketModeTransmitting;

    return newPacket;

    //
    // Failed, put packet back if we allocated one
    //
 Bad:
    LogIt("oom3!\n");
    assert(FALSE); //should not happen.
    PacketMgr.Free(newPacket);
    return NULL;
}

//=============================================================================
//    Method: VirtualPcDriver3::FreePacket().
//
//    Description: Return a packet to the (proper) free list
//=============================================================================

void
VirtualPcDriver3::FreePacket( 
    IN PACKET * Packet,
    IN BOOL bForReceiving
    )
{
    VPCNetSvPacketEntryPtr packetEntry = (VPCNetSvPacketEntryPtr)Packet->DriverState;

    LogIt((bForReceiving) ? "pkt::rf %p %u" : "pkt::xf %p %u",
          (UINT_PTR)packetEntry,Packet->nBytesAvail);

    //
    // Say the packet no longer holds a packet entry.
    //
    Packet->Buffer = NULL;
    Packet->Length = 0;

    //
    // Return the packet entry to the proper idle queue
    //
    if (bForReceiving)
        FreeReceivePacketEntry(packetEntry);
    else {
        //
        // If it has not been explicitly removed from the kernel-controlled queue
        // we cannot touch it.
        //
        if (!Packet->KernelOwned)
            FreeTransmitPacketEntry(packetEntry);
    }

    //
    // Return the packet to the packet manager
    //
    PacketMgr.Free(Packet);
}

//=============================================================================
//    Method: VirtualPcDriver3::PostReceivePacket().
//
//    Description: Posts a packet for receiving.
//=============================================================================

HRESULT
VirtualPcDriver3::PostReceivePacket( 
    IN PACKET * Packet
    )
{
    VPCNetSvPacketEntryPtr packetEntry = (VPCNetSvPacketEntryPtr)Packet->DriverState;

    LogIt("pkt::rp %p",(UINT_PTR)packetEntry);

    //
    // JIC, refresh size; then post.
    //
    packetEntry->fPacketLength = (Packet->Length > kVPCNetSvMaximumPacketLength) ?
        kVPCNetSvMaximumPacketLength : Packet->Length;
    Packet->KernelOwned = TRUE;
    PostReceivePacketEntry(packetEntry);

    return ERROR_IO_PENDING;
}

//=============================================================================
//    Method: VirtualPcDriver3::PostTransmitPacket().
//
//    Description: Posts a packet for transmitting.
//=============================================================================

HRESULT
VirtualPcDriver3::PostTransmitPacket( 
    IN PACKET * Packet
    )
{
    VPCNetSvPacketEntryPtr packetEntry = (VPCNetSvPacketEntryPtr)Packet->DriverState;

    LogIt("pkt::xp %p %u",(UINT_PTR)packetEntry,Packet->nBytesAvail);

    //
    // Set the size, then post.
    //
    packetEntry->fPacketLength = (Packet->nBytesAvail > kVPCNetSvMaximumPacketLength) ?
        kVPCNetSvMaximumPacketLength : Packet->nBytesAvail;
    Packet->KernelOwned = TRUE;
    PostTransmitPacketEntry(packetEntry);

    //
    // Bump the count of in-flight xmit packets
    //
    LONG oldCount, newCount;
    for (;;) {
        oldCount = mTxBuffer.fPacketBuffer->fPendingCount;
        newCount = oldCount + 1;
        if (InterlockedCompareExchange(
                 (volatile LONG *)&mTxBuffer.fPacketBuffer->fPendingCount,
                 newCount,
                 oldCount) == oldCount)
                break;
    }

    //
    // Wakeup the driver if it was (possibly) idle.
    //
    if (/*(newCount == 1) || */Packet->Flush) {
        DWORD length = 0;
        if (DeviceIoControl(this->hFileHandle,
                            (DWORD) IOCTL_PROCESS_TRANSMIT_BUFFERS,
                            NULL,
                            0,
                            NULL,
                            0,
                            &length,
                            NULL) == FALSE)
        {
            //
            // Not much we can do about it.
            //
            WARN(("Warning: Failed to wakeup xmit path (le=%u)",
                   GetLastError()));
        }
    }

    return ERROR_IO_PENDING;
}

//=============================================================================
//  Method: VirtualPcDriver3::GetNextReceivedPacket().
//
//  Description: Dequeues the next packet from the receive queue.
//               The xmit queue is ignored.
//=============================================================================

PACKET *
VirtualPcDriver3::GetNextReceivedPacket(
    IN UINT32 TimeOutInMsec
)
{
    VPCNetSvPacketEntry * packetEntry;
    UINT32 nDequeued = 1;
    PACKET_MODE Mode = PacketModeInvalid;

    NOISE(("VirtualPcDriver3::GetNextReceivedPacket(%u)..",TimeOutInMsec));

    packetEntry = ReceiveCompleted.Pop();
    if (packetEntry != NULL)
        goto HaveReceived;

    //
    // Before we go to sleep check the Pending field
    //
 ReCheck:
    if (mRxBuffer.fPacketBuffer->fPendingCount == 0) {
        //
        // Before we go to sleep, tell the driver we pulled everything.
        // Testing shows without this call ping times go from <1ms to >2sec.
        //
        DWORD length = 0;
        if (DeviceIoControl(this->hFileHandle,
                            (DWORD) IOCTL_NOTIFY_RECEIVE_BUFFER_CONSUMED,
                            NULL,
                            0,
                            NULL,
                            0,
                            &length,
                            NULL) == FALSE)
        {
            //
            // Not much we can do about it.
            //
            WARN(("Warning: Failed to wakeup recv path (le=%u)",
                   GetLastError()));
        }

        //DPRINTF(("WFSO.."));
        DWORD EventIndex = WaitForSingleObject(hEvents[iRecvEvent],TimeOutInMsec);

        if (EventIndex != WAIT_OBJECT_0) {
            LogIt("pkt::grp.timeout");
#if 1
            UINT l = ReceiveCompleted.Length();
            if (l) LogIt("pkt::bad rc is %u",l);
            l = mRxBuffer.fPacketBuffer->fPendingCount;
            if (l) LogIt("pkt::bad rq is %u",l);
#endif
            return NULL;
        }
    }


    //
    // For receive...
    // dequeue, adjust m??Buffer.fPacketBuffer->fPendingCount
    // if more than one put into an in-order intermediate queue.
    //
    nDequeued = 0;
    packetEntry = PacketBufferStealAndReverse(&mRxBuffer,
                                              &mRxBuffer.fPacketBuffer->fReadyBufferQueue,
                                              &nDequeued);
    NOISE(("!%d! ",nDequeued));

    //
    // Since we are now re-checking before sleeping, we might get into
    // the case where we took the last packet after the kernel signalled,
    // we called WFMO, came back, but nothing new was there.
    //
    if (packetEntry == NULL) {
        NOISE(("\tc%u ",mRxBuffer.fPacketBuffer->fPendingCount));
        goto ReCheck;
    }

    //
    // Adjust count of in-flight packets
    //
    for (;;) {
        LONG oldCount = mRxBuffer.fPacketBuffer->fPendingCount;
        LONG newCount = oldCount - nDequeued;
        if (InterlockedCompareExchange(
              (volatile LONG *)&mRxBuffer.fPacketBuffer->fPendingCount,
              newCount,
              oldCount) == oldCount)
            break;
    }

    //
    // Park what we dont consume
    //
    if (nDequeued > 1)
        ReceiveCompleted.Push(packetEntry->fNextPacket);

    //
    // Get back the packet pointer from the packet entry, check
    //
 HaveReceived:
    PACKET *newPacket = (PACKET*)packetEntry->fAppRefCon;
    assert( (newPacket != NULL) && (newPacket->DriverState == packetEntry) );

    //
    // Say how much data we got
    //
    newPacket->nBytesAvail = packetEntry->fPacketLength;

    //
    // and done
    //
    newPacket->KernelOwned = FALSE;
    LogIt("pkt::rc %p",(UINT_PTR)newPacket);
    return newPacket;
}

//=============================================================================
//    Method: VirtualPcDriver3::GetNextCompletedPacket().
//
//    Description: Dequeues the first packet that has completed, Maybe waits.
//=============================================================================

PACKET_MODE
VirtualPcDriver3::GetNextCompletedPacket( 
    OUT PACKET ** pPacket,
    IN  UINT32    TimeOutInMsec
    )
{
    VPCNetSvPacketEntry * packetEntry;
    UINT32 nDequeued = 1, EventIndex = 0;
    PACKET_MODE Mode = PacketModeInvalid;

    NOISE(("VirtualPcDriver3::GetNextCompletedPacket(%u)..",TimeOutInMsec));

    //
    // See if we have something available already
    // Check the xmit q first: if we block it we run out of memory.
    //
    packetEntry = TransmitCompleted.Pop();
    if (packetEntry != NULL)
        goto HaveTransmitted;

    // Must check the bottom side of the Q as well
    if (!PacketBufferIsQueueEmpty(&mTxBuffer,
                                  &mTxBuffer.fPacketBuffer->fCompletedBufferQueue)) {
        //LogIt("pkt::xbottom");
        goto CheckTheXmitQueue;
    }    

    //
    // Now check the receive Q, top half.
    //
    packetEntry = ReceiveCompleted.Pop();
    if (packetEntry != NULL)
        goto HaveReceived;

    //
    // Before we go to sleep check the Pending field one more time
    // Do not *always* check for receives first, lest we starve xmit completes.
    //
    if (mRxBuffer.fPacketBuffer->fPendingCount) {
        EventIndex = iRecvEvent;
        NOISE(("\tp%u ",mRxBuffer.fPacketBuffer->fPendingCount));
    } else {

    GoToSleep:
        //
        // Before we go to sleep, tell the driver we pulled everything.
        // Testing shows without this call ping times go from <1ms to >2sec.
        //
        DWORD length = 0;
        if (DeviceIoControl(this->hFileHandle,
                            (DWORD) IOCTL_NOTIFY_RECEIVE_BUFFER_CONSUMED,
                            NULL,
                            0,
                            NULL,
                            0,
                            &length,
                            NULL) == FALSE)
        {
            //
            // Not much we can do about it.
            //
            WARN(("Warning: Failed to wakeup recv path (le=%u)",
                   GetLastError()));
        }

        //DPRINTF(("WFMO.."));
        EventIndex = WaitForMultipleObjects(nEvents,hEvents,FALSE,TimeOutInMsec);

    }
    DPRINTF(("VirtualPcDriver3::GetNextCompletedPacket() -> %x",EventIndex));

    //
    // BUGBUG should test for WAIT_ABANDONED 
    //
    if (EventIndex >= nEvents) {
        LogIt("pkt::gcp.timeout");
#if 0
        UINT l = TransmitCompleted.Length();
        if (l) LogIt("pkt::bad xc is %u",l);
        l = ReceiveCompleted.Length();
        if (l) LogIt("pkt::bad rc is %u",l);
        l = mRxBuffer.fPacketBuffer->fPendingCount;
        if (l) LogIt("pkt::bad rq is %u",l);
        l = mTxBuffer.fPacketBuffer->fPendingCount;
        if (l) LogIt("pkt::?xq is %u",l);
        l = (!PacketBufferIsQueueEmpty(&mTxBuffer,
                                       &mTxBuffer.fPacketBuffer->fCompletedBufferQueue);

        if (l) LogIt("pkt::bad xq is %u",l);
#endif
        goto Out;
    }

    //
    // For receive...
    // dequeue, adjust m??Buffer.fPacketBuffer->fPendingCount
    // if more than one put into an in-order intermediate queue.
    //
    if (EventIndex == iRecvEvent) {

        nDequeued = 0;
        packetEntry = PacketBufferStealAndReverse(&mRxBuffer,
                                                  &mRxBuffer.fPacketBuffer->fReadyBufferQueue,
                                                  &nDequeued);
        NOISE(("!%d! ",nDequeued));

        //
        // Since we are now re-checking before sleeping, we might get into
        // the case where we took the last packet after the kernel signalled,
        // we called WFMO, came back, but nothing new was there.
        //
        if (packetEntry == NULL) {
            NOISE(("\tc%u ",mRxBuffer.fPacketBuffer->fPendingCount));
            goto GoToSleep;
        }

        //
        // Adjust count of in-flight packets
        //
        for (;;) {
            LONG oldCount = mRxBuffer.fPacketBuffer->fPendingCount;
            LONG newCount = oldCount - nDequeued;
            if (InterlockedCompareExchange(
                  (volatile LONG *)&mRxBuffer.fPacketBuffer->fPendingCount,
                  newCount,
                  oldCount) == oldCount)
                break;
        }

        //
        // Park what we dont consume
        //
        if (nDequeued > 1)
            ReceiveCompleted.Push(packetEntry->fNextPacket);

        //
        // Get back the packet pointer from the packet entry, check
        //
    HaveReceived:
        PACKET *newPacket = (PACKET*)packetEntry->fAppRefCon;
        assert( (newPacket != NULL) && (newPacket->DriverState == packetEntry) );

        //
        // Say how much data we got
        //
        newPacket->nBytesAvail = packetEntry->fPacketLength;
        newPacket->KernelOwned = FALSE;
        *pPacket = newPacket;

        //
        // and done
        //
        Mode = PacketModeReceiving;
        goto Out;
    }

    //
    // For xmit... same story.
    //
    if (EventIndex == iXmitEvent) {

        //
        // See what just completed, check.
        //
    CheckTheXmitQueue:
        nDequeued = 0;
        packetEntry = 
            PacketBufferStealAndReverse(&mTxBuffer,
                                        &mTxBuffer.fPacketBuffer->fCompletedBufferQueue,
                                        &nDequeued);
        NOISE(("?%d? ",nDequeued));
        if (packetEntry == NULL) {
            //
            // There was nothing there.
            //
            goto GoToSleep;
        }

        //
        // Park what we dont consume
        //
        if (nDequeued > 1)
            TransmitCompleted.Push(packetEntry->fNextPacket);

        //
        // Get back the packet pointer from the packet entry, check
        //
    HaveTransmitted:
        PACKET *newPacket = (PACKET*)packetEntry->fAppRefCon;
        assert( (newPacket != NULL) && (newPacket->DriverState == packetEntry) );

        //
        // Say how much data we sent
        //
        newPacket->nBytesAvail = packetEntry->fPacketLength;
        newPacket->KernelOwned = FALSE;
        *pPacket = newPacket;

        //
        // and done
        //
        Mode = PacketModeTransmitting;
        goto Out;
    }

    //
    // Should not happen
    //
    WARN(("VirtualPcDriver3::GetNextCompletedPacket cannot handle EventIndex"));
    assert(FALSE);
    return PacketModeInvalid;

 Out:
    LogIt((Mode == PacketModeTransmitting) ? "pkt::xc %p" :
          ((Mode == PacketModeReceiving) ? "pkt::rc %p" : "pkt:to"),
          (UINT_PTR)*pPacket);
    return Mode;
}

//=============================================================================
//    Method: VirtualPcDriver3::Flush().
//
//    Description: Flush all pending I/Os, to recover packets in flight
//=============================================================================

BOOL
VirtualPcDriver3::Flush( 
    void
    )
{
    VPCNetSvPacketEntry *packetEntry, *nextPacketEntry;
    PACKET *Packet;
    UINT32 nDequeued;

    if (!bInitialized)
        return FALSE;

    //
    // Stop the receive Q by stealing all free packets
    //
    nDequeued = 0;
    packetEntry = 
        PacketBufferStealAndReverse(&mRxBuffer,
                                    &mRxBuffer.fPacketBuffer->fFreeBufferQueue,
                                    &nDequeued);

    //
    // Return all packets to the idle Q
    //
    while (packetEntry != NULL)
    {
        nextPacketEntry = packetEntry->fNextPacket;
        Packet = (PACKET*)packetEntry->fAppRefCon;

        assert( (Packet != NULL) && (Packet->DriverState == packetEntry) );

        FreePacket(Packet,TRUE);

        packetEntry = nextPacketEntry;
    }

    //
    // Stop the xmit queue by retracting all non-xmitted packets
    //
    nDequeued = 0;
    packetEntry = 
        PacketBufferStealAndReverse(&mTxBuffer,
                                    &mTxBuffer.fPacketBuffer->fReadyBufferQueue,
                                    &nDequeued);

    mTxBuffer.fPacketBuffer->fPendingCount = 0;

    //
    // Return all packets to the idle Q
    //
    while (packetEntry != NULL)
    {
        nextPacketEntry = packetEntry->fNextPacket;
        Packet = (PACKET*)packetEntry->fAppRefCon;

        assert( (Packet != NULL) && (Packet->DriverState == packetEntry) );

        FreePacket(Packet,FALSE);

        packetEntry = nextPacketEntry;
    }

    //
    // Now go recover everything that was completed
    //
    for (;;)
    {
        PACKET_MODE     Mode;

        Packet = NULL;
        Mode = GetNextCompletedPacket(&Packet,0);

        if (Mode == PacketModeInvalid)
            break;

        FreePacket(Packet, Mode == PacketModeReceiving);

    }

    return TRUE;
}


//=============================================================================
//  Constructor: VirtualPcDriver3()
//
//=============================================================================
VirtualPcDriver3::VirtualPcDriver3(
     IN int        gDebug,
     IN BOOL       gQuiet)
{
    Debug = gDebug;
    Quiet = gQuiet;
    hFileHandle = AuxHandle = INVALID_HANDLE_VALUE;
    hEvents[iXmitEvent] = hEvents[iRecvEvent] = INVALID_HANDLE_VALUE;
    memset(&mTxBuffer,0,sizeof mTxBuffer);
    memset(&mRxBuffer,0,sizeof mRxBuffer);
    memset(EthernetAddress,0,6);
    bInitialized = FALSE;
}

//=============================================================================
//  Destructor: VirtualPcDriver3()
//
//=============================================================================
VirtualPcDriver3::~VirtualPcDriver3(void)
{
    //
    // Stop and reclaim all packets
    // Actually.. we are throwing everything away so what's the point.
    //Flush();

    //
    // Close all handles
    //
    for (int i = 0; i < nEvents; i++) {
        if (hEvents[i] != INVALID_HANDLE_VALUE)
            CloseHandle(hEvents[i]);
        hEvents[i] = INVALID_HANDLE_VALUE;
    }

    if (this->hFileHandle != INVALID_HANDLE_VALUE)
        CloseHandle(hFileHandle);
    this->hFileHandle = INVALID_HANDLE_VALUE;

    if (this->AuxHandle != INVALID_HANDLE_VALUE)
        CloseHandle(AuxHandle);
    this->AuxHandle = INVALID_HANDLE_VALUE;

    bInitialized = FALSE;

}

#define OLD_DRIVER_SUPPORTED 1
#if defined(OLD_DRIVER_SUPPORTED)
//=============================================================================
//    SubSection: NdisPacketFilter::
//
//    Description: Ndis intermediate driver implementation
//    This is/was based on a sample of an NDIS intermediate driver,
//    was implemented for Win2k and still is available in many forms.
//    Our version was distributed as part of the "Invisible Computing" kit.
//=============================================================================
class NdisPacketFilter : public VirtualPcDriver2 {
public:
    NdisPacketFilter(IN int        gDebug,
                     IN BOOL       gQuiet) :
        VirtualPcDriver2(gDebug,gQuiet)
    {
    }

    virtual BOOL ChangeMacAddress( IN UINT8 *MacAddress);

    //
    // No differences
    //
private:
    //
    // Our private methods
    //
    BOOL GetSymbolicName(IN const wchar_t * AdapterName,
                         OUT wchar_t      * SymbolicName);
};


//=============================================================================
//    Method: NdisPacketFilter::ChangeMacAddress().
//
//    Description: Change the MAC address used by the packet driver.
//                 Alas.. we cannot.
//=============================================================================

BOOL
NdisPacketFilter::ChangeMacAddress(
    IN UINT8 *MacAddress
)
{
    UINT8 EthernetAddress[6];
    GetMacAddress(EthernetAddress);
    if (memcmp(EthernetAddress,MacAddress,6) == 0)
        return TRUE;
    //
    // Must really change it.. cannot do that Dave.
    //
    return FALSE;
}

//=============================================================================
//    Method: NdisPacketFilter::GetSymbolicName().
//
//    Description: Get the NT symbolic name for the packet driver we'll use.
//                 The driver supports multiple NICs, we'll go for the first
//                 that has an ethernet address assigned and is not in use.
//=============================================================================

BOOL
NdisPacketFilter::GetSymbolicName(
    IN const wchar_t * AdapterName,
    OUT wchar_t      * SymbolicName
    )
{
    HANDLE        hControl;
    UINT32        version;
    wchar_t       *pDriverName;
    BOOL          bRet = FALSE;
    
    DPRINTF(("NdisPacketFilter::GetSymbolicName()"));

    //
    // Open the driver's control point
    //
    pDriverName = OLD_DRIVER_NAME;
    hControl = CreateFileW(pDriverName,
                           GENERIC_READ | GENERIC_WRITE, 
                           0,//FILE_SHARE_READ|FILE_SHARE_WRITE
                           NULL, 
                           OPEN_EXISTING, 
                           FILE_FLAG_OVERLAPPED|FILE_FLAG_NO_BUFFERING, 
                           NULL);
                          
    if (hControl == INVALID_HANDLE_VALUE)
    {
        //
        // Did not work, warn user and fail.
        //
        DWORD le = GetLastError();
        WARN(("Failed to open NDIS filter driver (%ls) - not installed (le=%d)?",
               pDriverName,le));
        return FALSE;
    }

    //
    // There was no versioning
    //
    version = 0;
    SetVersion(version);

    //
    // Select the adapter we will use
    //
    bRet = SelectAdapter(hControl,
                         AdapterName,
                         SymbolicName);
    //
    // and done.
    //
    CloseHandle(hControl);

    return bRet;
}

#endif // defined(OLD_DRIVER_SUPPORTED)

//=============================================================================
//    Function: OpenPacketDriver().
//
//    Description: Create the proper interface to the packet driver.
//=============================================================================

PACKET_DRIVER * 
OpenPacketDriver(
    IN const wchar_t *PreferredNicName,
    IN UINT           PreferredPacketDriverVersion,
    IN BOOL           bQuiet
    )
{
    PACKET_DRIVER *Interface = NULL;

    LogIt("pkt::OpenPacketDriver(%d,%d)",PreferredPacketDriverVersion,bQuiet);

    //
    // Ack user preferences (once)
    //
    if (!bQuiet)
    {
        if (PreferredPacketDriverVersion)
            printf("Wants PacketDriverVersion %u exclusively\n",
                   PreferredPacketDriverVersion);
        if (PreferredNicName)
            printf("Wants NIC '%ls' exclusively\n",
                   PreferredNicName);
    }

#if 1
#else
    //SUPPORT_V3_ON_S2 Not quite the default yet, because of completion issues.
    if (PreferredPacketDriverVersion == 0)
        PreferredPacketDriverVersion = 2;
#endif

    switch (PreferredPacketDriverVersion)
    {
    case 0:
        //
        // Try all the things we know, in turn.
        //

    case 3:
        //
        // Try the shared-memory based VPC interface
        //
        Interface = new VirtualPcDriver3(DEBUG_LEVEL,bQuiet);
        if (Interface->Open(PreferredNicName))
        {
            if (!bQuiet)
                printf("Using PacketDriverVersion 3.\n");
            return Interface;
        }
        else
            delete Interface;
        if (PreferredPacketDriverVersion != 0)
            goto NoDice;
        // else fall-through

    case 2:
        //
        // Try the FILE I/O based VPC interface
        //
        Interface = new VirtualPcDriver2(DEBUG_LEVEL,bQuiet);
        if (Interface->Open(PreferredNicName))
        {
            if (!bQuiet)
                printf("Using PacketDriverVersion 2.\n");
            return Interface;
        }
        else
            delete Interface;
        if (PreferredPacketDriverVersion != 0)
            goto NoDice;

    case 1:
        //
        // Try the NDIS packet filter interface
        //
#if defined(OLD_DRIVER_SUPPORTED)
        Interface = new NdisPacketFilter(DEBUG_LEVEL,bQuiet);
        if (Interface->Open(PreferredNicName))
        {
            if (!bQuiet)
                printf("Using PacketDriverVersion 1.\n");
            return Interface;
        }
        else
            delete Interface;
#endif
        // else fall-through

    NoDice:
        //
        // That did not work.
        //
        if (PreferredPacketDriverVersion != 0) {
            WARN(("No suitable NIC."));
        }
        // else warned already.
        return NULL;

    default:
        //
        // Not a version we can support
        //
        WARN(("Preferred DriverVersion is not supported."));
        break;
    }

    return NULL;
}
