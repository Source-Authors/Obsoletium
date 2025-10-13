// Copyright Valve Corporation, All rights reserved.
//
// Memory allocation!

#include "stdafx.h"
#include "tier0/mem.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include "tier0/minidump.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef STEAM
#define PvRealloc realloc
#define PvAlloc malloc
#define PvExpand _expand
#endif

// dimhotepus: Memory allocation should be multithreaded.
static CThreadMutex s_allocMutex;
static uint8 *s_pBuf = nullptr;
static size_t s_pBufStackDepth[32];
static int s_nBufDepth = -1;
static size_t s_nBufCurSize = 0;
static size_t s_nBufAllocSize = 0;
static std::atomic_bool s_oomerror_called = false;

void MemAllocOOMError( size_t nSize )
{
	bool expected = false;
	if ( !s_oomerror_called.compare_exchange_strong(expected, true, std::memory_order_acq_rel, std::memory_order_relaxed) )
	{
		MinidumpUserStreamInfoAppend( "MemAllocOOMError: %zu bytes\n", nSize );

		//$ TODO: Need a good error message here.
		// A basic advice to try lowering texture settings is just most-likely to help users who are exhausting address
		// space, but not necessarily the cause.  Ideally the engine wouldn't let you get here because of too-high settings.
		Error( "Out of memory or address space.  Texture quality setting may be too high.\n" );
	}
}

//-----------------------------------------------------------------------------
// Other DLL-exported methods for particular kinds of memory
//-----------------------------------------------------------------------------
void *MemAllocScratch( size_t nMemSize )
{	
	// dimhotepus: Make thread-safe.
	AUTO_LOCK(s_allocMutex);

	const size_t newAllocSize = s_nBufCurSize + nMemSize;
	// Minimally allocate 1M scratch
	if (s_nBufAllocSize < newAllocSize)
	{
		s_nBufAllocSize = newAllocSize;
		if (s_nBufAllocSize < 1024 * 1024)
		{
			s_nBufAllocSize = 1024 * 1024;
		}

		if (s_pBuf)
		{
			s_pBuf = (uint8*)PvRealloc( s_pBuf, s_nBufAllocSize );
			Assert( s_pBuf );
		}
		else
		{
			s_pBuf = (uint8*)PvAlloc( s_nBufAllocSize );
		}
	}

	size_t nBase = std::exchange(s_nBufCurSize, s_nBufCurSize + nMemSize);

	++s_nBufDepth;
	Assert( s_nBufDepth < ssize(s_pBufStackDepth) );
	if ( s_nBufDepth >= ssize(s_pBufStackDepth) )
		Error("Stack overflow during scratch memory allocation. Only %zd allocations allowed.", ssize(s_pBufStackDepth));

	s_pBufStackDepth[s_nBufDepth] = nMemSize;

	return &s_pBuf[nBase];
}

void MemFreeScratch()
{
	// dimhotepus: Make thread-safe.
	AUTO_LOCK(s_allocMutex);

	Assert( s_nBufDepth >= 0 );
	if ( s_nBufDepth < 0 )
		Error("Stack underflow during scratch memory free. Free called when no memory allocated.");

	s_nBufCurSize -= s_pBufStackDepth[s_nBufDepth];
	--s_nBufDepth;
}

#ifdef POSIX
void ZeroMemory( void *mem, size_t length )
{
	memset( mem, 0x0, length );
}
#endif
