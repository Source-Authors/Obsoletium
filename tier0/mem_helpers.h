// Copyright Valve Corporation, All rights reserved.

#ifndef TIER0_MEM_HELPERS_H_
#define TIER0_MEM_HELPERS_H_

#include <atomic>
#include <cstddef>  // size_t

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
//
// Command line switches (case-insensitive, read once on first allocation):
//   -noinitmemory     never touch new allocations
//   -randomizememory  fill new allocations with a repeating pseudo-random pattern
//
// dimhotepus: Same behavior on Windows, Linux and macOS (x86, x86-64, arm64).

// dimhotepus: Thread-safe init. Master on/off switch; it publishes no other data,
// so relaxed loads/stores are sufficient.
extern std::atomic<bool> g_bInitMemory;

// Fills pMem[0, nSize) according to the mode above. Thread-safe, lock-free,
// never allocates (safe to call from inside the allocator while it holds its lock).
// Caller must own pMem exclusively for the duration of the call.
void DoApplyMemoryInitializations( void *pMem, size_t nSize );

// dimhotepus: Inline function instead of a macro: arguments are evaluated once and
// type-checked; call sites are unchanged ( ApplyMemoryInitializations( p, n ); ).
inline void ApplyMemoryInitializations( void *pMem, size_t nSize )
{
	if ( g_bInitMemory.load( std::memory_order_relaxed ) )
		DoApplyMemoryInitializations( pMem, nSize );
}

// Reserves (no access) the low addresses that 32-bit fill patterns turn into when they
// are used as pointers, e.g. 0x00000000FFEEFFEE after a truncated or 32-bit-loaded pointer,
// so dereferences crash right away instead of reading whatever is mapped there.
// Windows (x86, x64, ARM64) and Linux; macOS needs nothing (__PAGEZERO covers 0-4 GiB).
// Best effort, idempotent, thread-safe, never allocates. Called automatically the first
// time the 0xFFEEFFEE fill is used.
void ReserveFillPatternAddresses();

// Bytes currently in use by the C runtime heap. Returns SIZE_MAX if the heap is
// found corrupted (Windows) and 0 where the platform cannot report it.
// Thread-safe, never allocates.
[[nodiscard]] size_t CalcHeapUsed();

// Call this to reserve the bottom 4 GiB of memory in order to ensure that we will
// get crashes if we put pointers in DWORDs or ints. This will be a NOP on some
// platforms (Xbox 360, PS3, 32-bit Windows, etc.)
void ReserveBottomMemory();

#endif  // TIER0_MEM_HELPERS_H_