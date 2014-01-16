// Copyright: Microsoft 2009
//
// Author: Rene Mueller
//
// Created: 8/10/09 
//
// Version: 1.00
// 
// 
// Description: Contains functions for reading the current clock speed of
// of the CPU, setting the affinity of a process to a specified core and 
// reading out the time-based counter register.
//
// Changelog: 
//
//----------------------------------------------------------------------------

#ifndef DEFINECPUTOOLSH
#define DEFINECPUTOOLSH 1

#pragma intrinsic(__rdtsc)

// 
// inline function read the 64 bit cycle count register on x86/x64 CPU
//
inline UINT64 get_cyclecount(void) { return __rdtsc();}


//
// set process affinity to a specified core
// @param  core number (0: first core CPU0, 1: second core CPU1, etc.)
// @return 0 upon success, 1 in case of an error
//
int set_affinity_core(int core);



//
// return the current clock speed of CPU0
// @return clock frequency in MHz (0 in case of an error)
ULONG get_clockspeed_mhz();

//
// compute the clock value of a deadline N mseconds from now.
// @param  number of milliseconds in the future
// @return deadline in clock ticks (0 in case of an error)
//UINT64 get_clock_value(int milliseconds_to_add);

#endif //DEFINECPUTOOLSH
