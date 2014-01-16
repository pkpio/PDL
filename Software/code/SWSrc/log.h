// Title:  Logging
//
// Copyright: Microsoft 2009
//
// Author: Alessandro Forin
//
// Created: 8/10/11 
//
// Version: 1.00
// 
// 
// Description: Simple logging package. Can be MT-safe and lock-free.
//
// Changelog: 
//
//----------------------------------------------------------------------------

#ifndef DEFINELOGH
#define DEFINELOGH 1

//Simple logging package. Can be MT-safe and lock-free.

//define this to 0 to wipe out all log calls & functions
#ifndef LOGIT
#define LOGIT 0
#endif

//define this to 1 to get the mt-safe version
#ifndef MTSAFE
#define MTSAFE 0
#endif

#define LOGIT_TIME_MARKER ((char *)0xbadbabe)

#if LOGIT
extern void LogIt(char *Format, UINT_PTR Info0 = 0, UINT_PTR Info1 = 0);
extern void PrintZeLog(void);
extern void StartLog(UINT32 where = 0);
extern UINT32 StopLog(void);
#else
inline void DontLogIt(char *Format, UINT_PTR Info0 = 0, UINT_PTR Info1 = 0) {}
#define LogIt DontLogIt //static lib link issues
#define PrintZeLog()
inline void DontStartLog(UINT32 where = 0) {}
#define StartLog DontStartLog //static lib link issues
#define StopLog() 0
#endif

#endif // DEFINELOGH
