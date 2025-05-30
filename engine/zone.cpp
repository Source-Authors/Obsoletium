//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//						ZONE MEMORY ALLOCATION
//
// There is never any space between memblocks, and there will never be two
// contiguous free memblocks.
//
// The rover can be left pointing at a non-empty block
//
// The zone calls are pretty much only used for small strings and structures,
// all big things are allocated on the hunk.
//=============================================================================//

#include "basetypes.h"
#include "zone.h"
#include "host.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "memstack.h"
#include "datacache/idatacache.h"
#include "sys_dll.h"
#include "tier0/memalloc.h"

#define MINIMUM_WIN_MEMORY			0x03000000	// FIXME: copy from sys_dll.cpp, find a common header at some point

CMemoryStack g_HunkMemoryStack;

#ifdef HUNK_USE_16MB_PAGE
CMemoryStack g_HunkOverflow;
static bool g_bWarnedOverflow;
#endif

static intp GetTargetCacheSize()
{
	intp nMemLimit = host_parms.memsize - Hunk_Size();
	if ( nMemLimit < 0x100000 )
	{
		nMemLimit = 0x100000;
	}
	return nMemLimit;
}

/*
===================
Hunk_AllocName
===================
*/
void *Hunk_AllocName (size_t size, const char *name, bool bClear)
{
	MEM_ALLOC_CREDIT();
	void * p = g_HunkMemoryStack.Alloc( size, bClear );
	if ( p )
		return p;
#ifdef HUNK_USE_16MB_PAGE
	if ( !g_bWarnedOverflow )
	{
		g_bWarnedOverflow = true;
		DevMsg( "Note: Hunk base page exhausted\n" );
	}

	p = g_HunkOverflow.Alloc( size, bClear );
	if ( p )
		return p;
#endif
	Error( "Engine hunk overflow when allocating %zu bytes for '%s'. Max hunk size %zd bytes, used %zd bytes.\n",
		size, name ? name : "(null)", g_HunkMemoryStack.GetMaxSize(), g_HunkMemoryStack.GetUsed() );
	return NULL;
}

/*
===================
Hunk_Alloc
===================
*/
void *Hunk_Alloc(size_t size, bool bClear )
{
	MEM_ALLOC_CREDIT();
	return Hunk_AllocName( size, NULL, bClear );
}

intp Hunk_LowMark()
{
	return (intp)( g_HunkMemoryStack.GetCurrentAllocPoint() );
}

void Hunk_FreeToLowMark(intp mark)
{
	Assert( mark < g_HunkMemoryStack.GetSize() );
#ifdef HUNK_USE_16MB_PAGE
	g_HunkOverflow.FreeAll();
	g_bWarnedOverflow = false;
#endif
	g_HunkMemoryStack.FreeToAllocPoint( mark );
}

intp Hunk_MallocSize()
{
#ifdef HUNK_USE_16MB_PAGE
	return g_HunkMemoryStack.GetSize() + g_HunkOverflow.GetSize();
#else
	return g_HunkMemoryStack.GetSize();
#endif
}

intp Hunk_Size()
{
#ifdef HUNK_USE_16MB_PAGE
	return g_HunkMemoryStack.GetUsed() + g_HunkOverflow.GetUsed();
#else
	return g_HunkMemoryStack.GetUsed();
#endif
}

void Hunk_Print()
{
	constexpr float kMibsMuliplier{1.f / (1024.f * 1024.f)};

#ifdef HUNK_USE_16MB_PAGE
	ConMsg( "Total used memory:      %.2f MiB (%.2f MiB/%.2f MiB)\n",
			Hunk_Size() * kMibsMuliplier,
			g_HunkMemoryStack.GetUsed() * kMibsMuliplier,
			g_HunkOverflow.GetUsed() * kMibsMuliplier );
	ConMsg( "Total committed memory: %.2f MiB (%.2f MiB/%.2f MiB)\n",
			Hunk_MallocSize() * kMibsMuliplier,
			g_HunkMemoryStack.GetSize() * kMibsMuliplier,
			g_HunkOverflow.GetSize() * kMibsMuliplier );
#else
	ConMsg( "Total used memory:      %.2f MiB\n", Hunk_Size() * kMibsMuliplier );
	ConMsg( "Total committed memory: %.2f MiB\n", Hunk_MallocSize() * kMibsMuliplier );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Memory_Init()
{
    MEM_ALLOC_CREDIT();

#if defined(HUNK_USE_16MB_PAGE)
    unsigned nMaxBytes = 48 * 1024 * 1024;
#else
	// dimhotepus: With r_hunkalloclightmaps 1 (default), the hunk is not large
	// enough to alloc some lightmaps, and the game crashes with a hunk overflow
	// error.
	// We can do a modest bump up (48->64MB) to increase memory available to the hunk
	// this was also done in CS:GO for 32 bit clients (64 bit got a bump to 128MB).
#ifdef PLATFORM_64BITS
    unsigned nMaxBytes = 128 * 1024 * 1024;
#else
    unsigned nMaxBytes = 64 * 1024 * 1024;
#endif
#endif

	constexpr unsigned nMinCommitBytes = 0x8000;

#ifndef HUNK_USE_16MB_PAGE
	constexpr unsigned nInitialCommit = 0x280000;

	while ( !g_HunkMemoryStack.Init( nMaxBytes, nMinCommitBytes, nInitialCommit ) )	 
	{
		Warning( "Unable to allocate %u MiB of memory, trying %u MiB instead\n", nMaxBytes, nMaxBytes/2 );

		nMaxBytes /= 2;

		if ( nMaxBytes < MINIMUM_WIN_MEMORY )
		{
			Error( "Failed to allocate minimum memory requirement for game (%u MiB)\n", MINIMUM_WIN_MEMORY/(1024*1024));
		}
	}
#else
	if ( !g_HunkMemoryStack.InitPhysical( 16*1024*1024 ) || !g_HunkOverflow.Init( nMaxBytes - 16*1024*1024, nMinCommitBytes ) )
	{
		Error( "Failed to allocate minimum memory requirement for game (%u MiB)\n", nMaxBytes );
	}
#endif

	g_pDataCache->SetSize( GetTargetCacheSize() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Memory_Shutdown()
{
	g_HunkMemoryStack.FreeAll();

	// This disconnects the engine data cache
	g_pDataCache->SetSize( 0 );
}
