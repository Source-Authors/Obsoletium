//========= Copyright Valve Corporation, All rights reserved. =================
//-----------------------------------------------------------------------------
// NOTE! This should never be called directly from leaf code
// Just use new,delete,malloc,free etc. They will call into this eventually
//-----------------------------------------------------------------------------
// dimhotepus: Include guard (the header had none).
#ifndef SE_TIER0_MEMSTD_H_
#define SE_TIER0_MEMSTD_H_

#include "stdafx.h"

#if defined(_WIN32)
#include "winlite.h"
#endif

#include <algorithm>
#include <atomic>
#include <cstdio>

#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "tier0/threadtools.h"
#include "tier0/tslist.h"

#include "mem_helpers.h"

// dimhotepus: The old #pragma pack(4) around these classes is gone. With 4-byte packing
// every 8-byte member that follows a 4-byte one lands on a 4-byte boundary on 64-bit
// targets, e.g. CSmallBlockPool::m_pNextAlloc (an interlocked pointer) at offset 20.
// Misaligned interlocked operations are split locks on x86-64 (very slow, and a fault
// with split-lock detection enabled) and an alignment fault on ARM64.


// #define NO_SBH	1


enum {
  MIN_SBH_BLOCK =	8,
  MIN_SBH_ALIGN =	8,
  MAX_SBH_BLOCK =	2048,
  MAX_POOL_REGION = (4*1024*1024),
  SBH_PAGE_SIZE =		(4*1024)
};
#define COMMIT_SIZE		(16*SBH_PAGE_SIZE)

// dimhotepus: Keyed on pointer size, not _M_X64. Windows ARM64 used to get the 32-bit
// layout (8-byte granularity pools), i.e. small blocks only 8-byte aligned although
// MSVC's malloc and alignof(max_align_t) promise 16 on every 64-bit target.
#if defined( PLATFORM_64BITS )
#define NUM_POOLS		34
#else
#define NUM_POOLS		42
#endif

// SBH not enabled for LINUX right now. Unlike on Windows, we can't globally hook malloc. Well,
//  we can and did in override_init_hook(), but that unfortunately causes all malloc functions
//	to get hooked - including the nVidia driver, etc. And these hooks appear to happen after
//	nVidia has alloc'd some memory and it crashes when they try to free that.
// So we need things to work without this global hook - which means we rely on memdbgon.h / memdbgoff.h.
//  Unfortunately, that stuff always comes in source files after the headers are included, and
//  that means any alloc calls in the header files call the real libc functions. It's a mess.
// I believe I've cleaned most of it up, and it appears to be working. However right now we are totally
// 	gated on other performance issues, and the SBH doesn't give us any win, so I've disabled it for now.
// Once those perf issues are worked out, it might make sense to do perf tests with SBH, libc, and tcmalloc.
//
// dimhotepus: The SBH code itself is now portable (mmap on POSIX), so it can be enabled
// elsewhere by defining MEM_SBH_ENABLED; the default is unchanged.
//$ #if defined( _WIN32 ) || defined( _PS3 ) || defined( LINUX )
#if ( defined( _WIN32 ) || defined( _PS3 ) ) && !defined( MEM_SBH_ENABLED )
#define MEM_SBH_ENABLED 1
#endif

class ALIGN16 CSmallBlockPool
{
public:
	void Init( unsigned nBlockSize, byte *pBase, unsigned initialCommit = 0 );
	uintp GetBlockSize() const;
	bool IsOwner( void *p ) const;
	void *Alloc();
	void Free( void *p );
	int CountFreeBlocks() const;
	uintp GetCommittedSize() const;
	uintp CountCommittedBlocks() const;
	uintp CountAllocatedBlocks() const;
	intp Compact();

private:

	using FreeBlock_t = TSLNodeBase_t;
	class CFreeList : public CTSListBase
	{
	public:
		void Push( void *p ) { CTSListBase::Push( static_cast<TSLNodeBase_t *>( p ) );	}
	};

	CFreeList		m_FreeList;

	unsigned		m_nBlockSize;

	// dimhotepus: Bump pointer and commit limit only change under m_CommitMutex (see
	// CSmallBlockPool::Alloc / Compact); atomics so the statistics can read them
	// without the lock and without a data race.
	std::atomic<byte *>	m_pNextAlloc;
	std::atomic<byte *>	m_pCommitLimit;
	byte *			m_pAllocLimit;
	byte *			m_pBase;

	CThreadFastMutex m_CommitMutex;
} ALIGN16_POST;


class ALIGN16 CSmallBlockHeap
{
public:
	CSmallBlockHeap();
	bool ShouldUse( size_t nBytes ) const;
	bool IsOwner( void * p ) const;
	void *Alloc( size_t nBytes );
	void *Realloc( void *p, size_t nBytes );
	void Free( void *p );
	uintp GetSize( void *p ) const;
	void DumpStats( FILE *pFile = nullptr );
	intp Compact();

private:
	CSmallBlockPool *FindPool( size_t nBytes ) const;
	CSmallBlockPool *FindPool( void *p );
	const CSmallBlockPool *FindPool( void *p ) const;

	CSmallBlockPool *m_PoolLookup[MAX_SBH_BLOCK >> 2]{};
	CSmallBlockPool m_Pools[NUM_POOLS];
	// nullptr when the address space reservation failed: the SBH is then disabled
	// instead of claiming ownership of the low addresses [0, NUM_POOLS * 4 MiB).
	byte *m_pBase{nullptr};
	byte *m_pLimit{nullptr};
} ALIGN16_POST;


class ALIGN16 CStdMemAlloc : public IMemAlloc
{
public:
	CStdMemAlloc()
	  :	m_pfnFailHandler( DefaultFailHandler ),
		m_sMemoryAllocFailed( 0 )
	{
		// Make sure that we return 64-bit addresses in 64-bit builds.
		ReserveBottomMemory();
	}
	virtual ~CStdMemAlloc() = default;

	// Release versions
	void *Alloc( size_t nSize ) override;
	void *Realloc( void *pMem, size_t nSize ) override;
	void  Free( void *pMem ) override;
    void *Expand_NoLongerSupported( void *pMem, size_t nSize ) override;

	// Debug versions
    void *Alloc( size_t nSize, const char *pFileName, int nLine ) override;
    void *Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine ) override;
    void  Free( void *pMem, const char *pFileName, int nLine ) override;
    void *Expand_NoLongerSupported( void *pMem, size_t nSize, const char *pFileName, int nLine ) override;

	// Returns size of a particular allocation
	size_t GetSize( void *pMem ) override;

    // Force file + line information for an allocation
    void PushAllocDbgInfo( const char *pFileName, int nLine ) override;
    void PopAllocDbgInfo() override;

	long CrtSetBreakAlloc( long lNewBreakAlloc ) override;
	int CrtSetReportMode( int nReportType, int nReportMode ) override;
	int CrtIsValidHeapPointer( const void *pMem ) override;
	int CrtIsValidPointer( const void *pMem, unsigned int size, int access ) override;
	int CrtCheckMemory( ) override;
	int CrtSetDbgFlag( int nNewFlag ) override;
	void CrtMemCheckpoint( _CrtMemState *pState ) override;
	void* CrtSetReportFile( int nRptType, void* hFile ) override;
	void* CrtSetReportHook( void* pfnNewHook ) override;
	int CrtDbgReport( int nRptType, const char * szFile,
			int nLine, const char * szModule, const char * pMsg ) override;
	int heapchk() override;

	void DumpStats() override;
	void DumpStatsFileBase( char const *pchFileBase ) override;
	void GlobalMemoryStatus( size_t *pUsedMemory, size_t *pFreeMemory ) override;

	bool IsDebugHeap() override { return false; }

	void GetActualDbgInfo( const char *&, int & ) override {}
	void RegisterAllocation( const char *, int, size_t, size_t, unsigned ) override {}
	void RegisterDeallocation( const char *, int, size_t, size_t, unsigned ) override {}

	int GetVersion() override { return MEMALLOC_VERSION; }

	void CompactHeap() override;

	MemAllocFailHandler_t SetAllocFailHandler( MemAllocFailHandler_t pfnMemAllocFailHandler ) override;
	size_t CallAllocFailHandler( size_t nBytes ) { return ( *m_pfnFailHandler.load( std::memory_order_acquire ) )( nBytes ); }

	uint32 GetDebugInfoSize() override { return 0; }
	void SaveDebugInfo( void * ) override { }
	void RestoreDebugInfo( const void * ) override {}	
	void InitDebugInfo( void *, const char *, int ) override {}

	static size_t DefaultFailHandler( size_t );
	void DumpBlockStats( void * ) override {}
#ifdef MEM_SBH_ENABLED
	CSmallBlockHeap m_SmallBlockHeap;
#endif

#if defined( _MEMTEST )
	virtual void SetStatsExtraInfo( const char *pMapName, const char *pComment );
#endif

	size_t MemoryAllocFailed() override;

	void SetCRTAllocFailed( size_t nMemSize );

	// dimhotepus: Atomic: the handler can be swapped while other threads call it, and
	// allocation failures are recorded from any thread.
	std::atomic<MemAllocFailHandler_t> m_pfnFailHandler;
	std::atomic<size_t>				m_sMemoryAllocFailed;
} ALIGN16_POST;

#endif  // SE_TIER0_MEMSTD_H_