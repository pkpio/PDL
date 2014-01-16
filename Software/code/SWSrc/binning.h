// Title:  Simple timing classes.
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
// Description: 
//     High-resolution, modeled after a chronometer.
//     You can Start+Stop as many times as you wish, then Read the total.
//     Reset before you start another timing session.
//     Binning class collects a set of samples, distributed in bins.
//     Print the bins at the end of the sampling session.
//     Additional bonus: only one conditional to zap it out of the code.
//
// Changelog: 
//
//----------------------------------------------------------------------------

#ifndef DEFINEBINNINGH
#define DEFINEBINNINGH 1

#ifndef NULLIFY_TIMERS
#define NULLIFY_TIMERS 0
#endif
#ifndef NULLIFY_BINNING
#define NULLIFY_BINNING 0
#endif

#if NULLIFY_TIMERS
// Compiles to null.
class Chrono {
 public:
    inline void Calibrate(void) {return;}
    inline void Reset(void) {return;}
    inline void Start(void) {return;}
    inline void Stop(void) {return;}
    inline uint64_t ReadRaw(void) {return 0;}
    double Read(void) {return 0.0;}
};

#else
// Compiles to real code
//

class Chrono {
 public:
    Chrono(void)
    {
        Calibrate();
        Reset();
    }

    void Calibrate(void)
    {
        LARGE_INTEGER Frequency;
        QueryPerformanceFrequency(&Frequency);
        ProcessorFrequency = Frequency.QuadPart / 1000000.0;
    }

    void Reset(void)
    {
        TimeStarted = 0;
        TimeAccumulated = 0;
    }

    inline void Start(void)
    {
        QueryPerformanceCounter((LARGE_INTEGER*)&TimeStarted);
    }

    inline void Stop(void)
    {
        LARGE_INTEGER Now;
        QueryPerformanceCounter(&Now);
        TimeAccumulated += Now.QuadPart - TimeStarted;
    }

    uint64_t ReadRaw(void)
    {
        return TimeAccumulated;
    }

    //returned time is in microseconds.
    double Read(void)
    {
        return TimeAccumulated / ProcessorFrequency;
    }

 private:
    uint64_t TimeStarted;
    uint64_t TimeAccumulated;
    double ProcessorFrequency;
};

#endif //!NULLIFY_TIMERS

#if NULLIFY_BINNING
// Compiles to null.
class Binning {
 public:
    Binning(uint32_t BinCount, uint64_t Granularity){return;}
    inline void Sample(uint64_t Value){return;}
    inline void Reset(void){return;}
    inline void Print(void){return;}
};

#else
// Compiles to real code
//

class Binning {
 public:
    Binning(uint32_t BinCount, uint64_t Granularity)
    {
        Size = BinCount;
        Weight = Granularity;
        Bins = new uint64_t[Size];
        Reset();
    }
    ~Binning(void)
    {
        delete Bins;
    }
    void Sample(uint64_t Value)
    {
        Value = Value / Weight;
        if (Value >= Size)
            Bins[Size-1]++;
        else
            Bins[Value]++;
    }
    void Reset(void)
    {
        memset(Bins,0,Size * sizeof *Bins);
    }
    void Print(void)
    {
        for (uint32_t i = 0; i < Size; i++)
        {
            if (Bins[i] > 0)
            {
                if (i < (Size-1))
                    printf("\t%8I64u in Bin %u [%I64u..%I64u]\n",Bins[i],i,
                           Weight*i,Weight*(i+1));
                else
                    printf("\t%8I64u in Bin %u [ >= %I64u]\n",Bins[i],i,
                           Weight*i);
            }
        }
    }
 private:
    uint64_t *Bins;
    uint64_t  Weight;
    uint32_t  Size;
};

#endif
#endif //DEFINEBINNINGH
