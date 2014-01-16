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
SIRC_DLL_LINKAGE SIRC * __stdcall openSirc(uint8_t *FPGA_ID, uint32_t driverVersion)
{
    // Try first for the new PCIe
    PCIE2_SIRC *pcie2 = new PCIE2_SIRC;
    if (pcie2->getLastError() == 0)
        return pcie2;
    delete pcie2;

    // Try first for the PCIe
    PCIE_SIRC *pcie = new PCIE_SIRC;
    if (pcie->getLastError() == 0)
        return pcie;
    delete pcie;

    // Then for a Pico card
	//PICO_SIRC *pico = new PICO_SIRC(driverVersion);
    //if (pico->getLastError() == 0)
    //    return pico;
    //delete pico;

    // Then for the network
	ETH_SIRC *eth = new ETH_SIRC(FPGA_ID,driverVersion,NULL);
    if (eth->getLastError() == 0)
        return eth;
    delete eth;

    // Ran out of ideas.
    return NULL;
}


