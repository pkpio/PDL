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

#include <windows.h>
#include <stdio.h>
#include "cputools.h"
#include "log.h"

#ifndef LOGIT
#define LOGIT 1
#endif
#ifndef MTSAFE
#define MTSAFE 0
#endif

//For our uses...
typedef UINT64 TIMESTAMP;
#define TIMESTAMP_FORMAT_STRING "%I64u"
#define CurrentTime() get_cyclecount()
#define SubtractTimestamps(_a_,_b_) ((TIMESTAMP)((_a_)-(_b_)))
#if MTSAFE
#define AtomicCmpAndSwap InterlockedCompareExchange
#define AtomicInc(_ptr_)  (UINT32) InterlockedIncrement((LONG*)(_ptr_))
#else
static inline bool
AtomicCmpAndSwap(UINT32 *Where, UINT32 OldValue, UINT32 NewValue)
{
    if (*Where != OldValue)
        return false; //and this is fatal, will spin forever.
    *Where = NewValue;
    return true;
}
#define AtomicInc(_ptr_) ++(*(_ptr_))
#endif

#if LOGIT

#ifndef LOGSIZE
#define LOGSIZE (1024*1024) // at 16b/entry
#endif

#define NOLOG ((UINT32)(~0))
static UINT32 LogP = 0;
static struct {
    char * Format;
    TIMESTAMP When;
    UINT_PTR Info[2];
} LogBuf[LOGSIZE];

void StartLog(UINT32 where)
{
    if (where == 0)
        //Might want to do this to make sure all buffer pages are faulted in
        memset(LogBuf,0,sizeof LogBuf);
    else{
        if (where >= LOGSIZE)
            where = 0; //sanity
    }
    LogP = where;
}

UINT32 StopLog(void)
{
    UINT32 r = LogP;
    LogP = NOLOG;
    return r;
}

void
PrintZeLog(void)
{
    UINT index, LogIs;
    char *Format;
    TIMESTAMP Now = CurrentTime(), t, t0 = 0;
    static UINT StartCount = 0;

    /* Block the log while we print it
     */
    for (;;) {
        LogIs = LogP;
        if (AtomicCmpAndSwap(&LogP,
                             LogIs,
                             NOLOG))
            break;
    }
    /* LogIs the oldest entry when full
     * When non-full it points to a NULL entry.
     */
    if (LogIs == NOLOG)
        return;//not now, and no harm done
    index = LogIs;
    t = 0;
    do {

        /* Skip NULL entries (non-full and sanity)
         */
        Format = LogBuf[index].Format;
        if (Format == NULL)
            goto Next;

        /* Time-base resynch?
         */
        if (t0 == 0)
            t0 = LogBuf[index].When;
        if (Format == LOGIT_TIME_MARKER) {
            printf("[%8u] TimeReset at " TIMESTAMP_FORMAT_STRING
                   " +" TIMESTAMP_FORMAT_STRING
                   " +" TIMESTAMP_FORMAT_STRING ")\n",
                   StartCount++,
                   LogBuf[index].When,
                   SubtractTimestamps(LogBuf[index].When,t0),
                   SubtractTimestamps(LogBuf[index].When,t));
            t0 = LogBuf[index].When;
            t  = t0;
            goto Next;
        }

        /* Print this one
         */
        printf("[%8u] ",StartCount++);
        printf(Format,LogBuf[index].Info[0],LogBuf[index].Info[1]);
#define PRINT_TIMESTAMPS 3
#if (PRINT_TIMESTAMPS==1)
        printf("  (" TIMESTAMP_FORMAT_STRING 
               " +" TIMESTAMP_FORMAT_STRING ")\n",
                 LogBuf[index].When,
                 SubtractTimestamps(LogBuf[index].When,t));
#elif (PRINT_TIMESTAMPS==2)
        printf("  (+" TIMESTAMP_FORMAT_STRING ")\n",
                 SubtractTimestamps(LogBuf[index].When,t));
#elif (PRINT_TIMESTAMPS==3)
        printf("  (" TIMESTAMP_FORMAT_STRING 
               " +" TIMESTAMP_FORMAT_STRING ")\n",
                 SubtractTimestamps(LogBuf[index].When,t0),
                 SubtractTimestamps(LogBuf[index].When,t));
#else
        printf("\n");
#endif

        /* Keep track of delta-T
         */
        t = LogBuf[index].When;

        /* Next index, plus wraps
         */
    Next:
        if (++index >= LOGSIZE) index = 0;

    } while (index != LogIs);
    printf("Time now  " TIMESTAMP_FORMAT_STRING 
           " (+" TIMESTAMP_FORMAT_STRING ")\n",
           Now, SubtractTimestamps(Now, t) );
    /* Let the log go
     */
    LogP = LogIs;
}

void 
LogIt(char *Format, UINT_PTR Info0, UINT_PTR Info1)
{
    TIMESTAMP Now = CurrentTime();
    INT i;
    BOOL Wrapped = 0;

    for (;;) {
        /* Disabled ?
         */
        if (LogP == NOLOG)
            return;
        /* Get a number, make sure its good.
         */
    Retry:
        i = AtomicInc(&LogP) - 1;
        if (i >= LOGSIZE) {
            /* Roundabout, with care
             */
            Wrapped = TRUE;
            if (!AtomicCmpAndSwap(&LogP,
                                  i+1,
                                  0))
                goto Retry;
        } else
            /* Good one.
             */
            break;
    }
    LogBuf[i].Format  = Format;
    LogBuf[i].When = Now;
    LogBuf[i].Info[0] = Info0;
    LogBuf[i].Info[1] = Info1;
#if 0
    printf("LOG '%s' %08x %08x " TIMESTAMP_FORMAT_STRING "\n",
           LogBuf[i].Format,
           LogBuf[i].Info[0],
           LogBuf[i].Info[1],
           LogBuf[i].When);

#endif
    if (Wrapped)
        PrintZeLog();
}

#else
#define LogIt(x,y,z)
#define PrintZeLog()
#endif

