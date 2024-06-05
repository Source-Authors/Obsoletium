// Copyright Valve Corporation, All rights reserved.

#ifndef TIER0_PMELIB_H_
#define TIER0_PMELIB_H_

#include "tier0/platform.h"

constexpr inline char VERSION[]{"1.0.2"};

// uncomment this list to add some runtime checks
//#define PME_DEBUG   

#include "tier0/valve_off.h"
#include <string>
#include "tier0/valve_on.h"

// RDTSC Instruction macro
#define RDTSC(var) (var = __rdtsc())
#define RDTSCP(var, aux) (var = __rdtscp(aux))

// RDPMC Instruction macro
#define RDPMC(counter, var) (var = __readpmc(counter))

// RDPMC Instruction macro, for performance counter 1 (ecx = 1)
#define RDPMC0(var) (var = __readpmc(0))
#define RDPMC1(var) (var = __readpmc(1))

#define EVENT_TYPE(mode) EventType##mode
#define EVENT_MASK(mode) EventMask##mode

#include "tier0/ia32detect.h"

enum ProcessPriority
{
    ProcessPriorityNormal,
    ProcessPriorityHigh,
};

enum PrivilegeCapture
{
    OS_Only,  // ring 0, kernel level
    USR_Only, // app level
    OS_and_USR, // all levels
};

enum CompareMethod
{
    CompareGreater,  // 
    CompareLessEqual, // 
};

enum EdgeState
{
    RisingEdgeDisabled, // 
    RisingEdgeEnabled,  // 
};

enum CompareState
{
    CompareDisable, // 
    CompareEnable,  // 
};

// Singletion Class
class PME : public ia32detect
{
public:
//private:

    static PME*		_singleton;

    void*			hFile;
    bool			bDriverOpen;
    double			m_CPUClockSpeed;

    //ia32detect detect;
    long Init();
    long Close();

protected:

    PME()
    {
        hFile = NULL;
        bDriverOpen = FALSE;
        m_CPUClockSpeed = 0;
        Init(); 
    }

public:

    static PME* Instance();  // gives back a real object

    ~PME()
    {
        Close();
    }

    double GetCPUClockSpeedSlow( void );
    double GetCPUClockSpeedFast( void );

    long SelectP5P6PerformanceEvent( uint32 dw_event, uint32 dw_counter, bool b_user, bool b_kernel );

    long ReadMSR(uint32 dw_reg, int64* pi64_value);
    long ReadMSR(uint32 dw_reg, uint64* pi64_value);

    long WriteMSR(uint32 dw_reg, const int64& i64_value);
    long WriteMSR(uint32 dw_reg, const uint64& i64_value);

    void SetProcessPriority(ProcessPriority priority);

    //---------------------------------------------------------------------------
    // Return the family of the processor
    //---------------------------------------------------------------------------
    CPUVendor GetVendor() const
    {
        return vendor;
    }

    int GetProcessorFamily() const
    {
        return version.Family;
    }

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, tchar *pchName );		// Validate our internal structures
#endif // DBGFLAG_VALIDATE

};

#include "tier0/P5P6PerformanceCounters.h"    
#include "tier0/P4PerformanceCounters.h"    
#include "tier0/K8PerformanceCounters.h"    

enum PerfErrors
{
    E_UNKNOWN_CPU_VENDOR	= -1,
    E_BAD_COUNTER			= -2,
    E_UNKNOWN_CPU			= -3,
    E_CANT_OPEN_DRIVER		= -4,
    E_DRIVER_ALREADY_OPEN	= -5,
    E_DRIVER_NOT_OPEN		= -6,
    E_DISABLED				= -7,
    E_BAD_DATA				= -8,
    E_CANT_CLOSE			= -9,
    E_ILLEGAL_OPERATION		= -10,
};

#endif  // TIER0_PMELIB_H_
