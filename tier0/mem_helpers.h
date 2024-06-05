// Copyright Valve Corporation, All rights reserved.

#ifndef TIER0_MEM_HELPERS_H_
#define TIER0_MEM_HELPERS_H_

// Normally, the runtime libraries like to mess with the memory returned by malloc(), 
// which can create problems trying to repro bugs in debug builds or in the debugger.
//
// If the debugger is present, it initializes data to 0xbaadf00d, which makes floating
// point numbers come out to about 0.1.
//
// If the debugger is not present, and it's a debug build, then you get 0xcdcdcdcd,
// which is about 25 million.
//
// Otherwise, you get uninitialized memory.
//
// In here, we make sure the memory is either random garbage, or it's set to
// 0xffeeffee, which casts to a NAN.
extern bool g_bInitMemory;
#define ApplyMemoryInitializations( pMem, nSize ) do { if ( !g_bInitMemory ) ; else { DoApplyMemoryInitializations( pMem, nSize ); } } while (false)
void DoApplyMemoryInitializations( void *pMem, size_t nSize );

size_t CalcHeapUsed();

// Call this to reserve the bottom 4 GiB of memory in order to ensure that we will
// get crashes if we put pointers in DWORDs or ints. This will be a NOP on some
// platforms (Xbox 360, PS3, 32-bit Windows, etc.)
void ReserveBottomMemory();

#endif  // TIER0_MEM_HELPERS_H_
