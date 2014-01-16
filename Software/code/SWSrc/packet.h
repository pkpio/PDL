/* Copyright (c) Microsoft Corporation. All rights reserved. */
/*++

Module Name:

    packet.h

Abstract:

Author:

Revision History:

    Converted to Windows 2000 - Eliyas Yakub 
    Added Virtual PC NIC interfaces - sandrof

--*/
#ifndef __PACKET_H
#define __PACKET_H

#if 0
#include <ntddndis.h>
#else
//
// This is the type of an NDIS OID value.
//

typedef ULONG NDIS_OID, *PNDIS_OID;


//
// General Objects
//

#define OID_GEN_LINK_SPEED                      0x00010107
#define OID_GEN_CURRENT_PACKET_FILTER           0x0001010E

//
// 802.3 Objects (Ethernet)
//

#define OID_802_3_PERMANENT_ADDRESS             0x01010101
#define OID_802_3_CURRENT_ADDRESS               0x01010102

//
// Ndis Packet Filter Bits (OID_GEN_CURRENT_PACKET_FILTER).
//

#define NDIS_PACKET_TYPE_DIRECTED           0x0001
#define NDIS_PACKET_TYPE_MULTICAST          0x0002
#define NDIS_PACKET_TYPE_ALL_MULTICAST      0x0004
#define NDIS_PACKET_TYPE_BROADCAST          0x0008
#define NDIS_PACKET_TYPE_PROMISCUOUS        0x0020

#define  MAX_LINK_NAME_LENGTH   124

typedef struct _PACKET_OID_DATA {

    ULONG           Oid;
    ULONG           Length;
    UCHAR           Data[1];

}   PACKET_OID_DATA, *PPACKET_OID_DATA;


#define FILE_DEVICE_PROTOCOL        0x8000
#define IOCTL_PROTOCOL_SET_OID      (DWORD)CTL_CODE(FILE_DEVICE_PROTOCOL, 0 , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PROTOCOL_QUERY_OID    (DWORD)CTL_CODE(FILE_DEVICE_PROTOCOL, 1 , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PROTOCOL_RESET        (DWORD)CTL_CODE(FILE_DEVICE_PROTOCOL, 2 , METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_ENUM_ADAPTERS         (DWORD)CTL_CODE(FILE_DEVICE_PROTOCOL, 3 , METHOD_BUFFERED, FILE_ANY_ACCESS)

#endif

//
// PACKET class
//
typedef enum _PACKET_MODE {
    PacketModeInvalid = 0,
    PacketModeReceiving = 1,
    PacketModeTransmitting = 2,
    PacketModeTransmittingBuffer = 3
} PACKET_MODE;

class PACKET {
    //
    // Methods
    //
 public:
    //
    // Re-Initialize a PACKET.
    //
    void Init(BYTE *Buffer,
              UINT  Length)
    {
#ifdef ZERO_INITIALIZE_ALL_PACKETS //normally off for better perf.
        memset(this, 0, sizeof(PACKET));
#else
        this->Next = NULL;
#endif

        this->Length = Length;
        this->Buffer = Buffer;
        this->Mode = PacketModeInvalid;
        this->Flush = TRUE;
        this->KernelOwned = FALSE;
    }

    //
    // State
    //
    OVERLAPPED   Overlapped;                   
    PACKET      *Next;
    void        *UserState;
    void        *UserState2;
    PACKET_MODE  Mode;
    HRESULT      Result;
    UINT32       Length;
    UINT32       nBytesAvail;
    void        *DriverState;
    UINT8       *Buffer;
    BOOL         Flush;
    BOOL         KernelOwned;
};

//
// Virtual Interface to the packet driver implementation
//
class PACKET_DRIVER {
    //
    // All methods are public and must be subclassed
    //
public:
    virtual ~PACKET_DRIVER(void)
    {
    }

    virtual BOOL Open(IN const wchar_t *AdapterName) = 0;

    virtual BOOL Flush(void) = 0;

    virtual PACKET * AllocatePacket(IN BYTE *Buffer,
                                    IN UINT Length,
                                    IN BOOL bForReceive
                                    ) = 0;
    virtual void FreePacket(IN PACKET *Packet,
                            IN BOOL bForReceiving) = 0;

    virtual HRESULT PostReceivePacket(IN PACKET *Packet) = 0;

    virtual HRESULT PostTransmitPacket(IN PACKET *Packet) = 0;

    virtual PACKET_MODE GetNextCompletedPacket(OUT PACKET ** pPacket,
                                               IN  UINT32 TimeOutInMsec
                                               ) = 0;

    virtual PACKET *GetNextReceivedPacket(IN  UINT32 TimeOutInMsec) = 0;

    virtual BOOL GetMacAddress( OUT UINT8 *MacAddress) = 0;

    virtual BOOL ChangeMacAddress( IN UINT8 *MacAddress) = 0;

    virtual HRESULT SetFilter(IN UINT32 Filter) = 0;

    virtual BOOL GetMaxOutstanding(OUT UINT32 *NumReads,
                                   OUT UINT32 *NumWrites) = 0;

};

// Contructor function
extern PACKET_DRIVER * OpenPacketDriver(const wchar_t *PreferredNicName,
                                        UINT PreferredPacketDriverVersion,
                                        BOOL bQuiet);

#endif
