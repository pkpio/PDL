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
// Description:
//----------------------------------------------------------------------------
//

#include "sirc_internal.h"

//Open the first valid SIRC interface
SIRC_DLL_LINKAGE SIRC_SERVER * __stdcall openSircServer(
    uint32_t **registerFile,
    uint8_t **inputBuffer,
    uint8_t **outputBuffer,
    uint32_t driverVersion,
    wchar_t *nicName)
{
    // Try first for this
    SRV_SIRC *srv = new SRV_SIRC(registerFile,inputBuffer,outputBuffer,driverVersion,nicName);
    if (srv->getLastError() == 0)
        return srv;
    delete srv;

    // Ran out of ideas.
    return NULL;
}


