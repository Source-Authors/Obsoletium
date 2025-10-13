//========= Copyright Valve Corporation, All rights reserved. ============//
//-----------------------------------------------------------------------------
// NOTE! This should never be called directly from leaf code
// Just use new,delete,malloc,free etc. They will call into this eventually
//-----------------------------------------------------------------------------
#include "stdafx.h"

#if defined(_WIN32)
#include "winlite.h"
#endif

#include <algorithm>

#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "tier0/threadtools.h"
#include "tier0/tslist.h"

#include "mem_helpers.h"

// dimhotepus: Apply alignment only here.
#pragma pack(push)
#pragma pack(4)


// #define NO_SBH	1


enum {
  MIN_SBH_BLOCK =	8,
  MIN_SBH_ALIGN =	8,
  MAX_SBH_BLOCK =	2048,
  MAX_POOL_REGION = (4*1024*1024),
  SBH_PAGE_SIZE =		(4*1024)
};
#define COMMIT_SIZE		(16*SBH_PAGE_SIZE)

#ifdef _M_X64
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
//$ #if defined( _WIN32 ) || defined( _PS3 ) || defined( LINUX )
#if defined( _WIN32 ) || defined( _PS3 )
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
		void Push( void *p ) { CTSListBase::Push( (TSLNodeBase_t *)p );	}
	};

	CFreeList		m_FreeList;

	unsigned		m_nBlockSize;

	CInterlockedPtr<byte> m_pNextAlloc;
	byte *			m_pCommitLimit;
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

	CSmallBlockPool *m_PoolLookup[MAX_SBH_BLOCK >> 2];
	CSmallBlockPool m_Pools[NUM_POOLS];
	byte *m_pBase;
	byte *m_pLimit;
} ALIGN16_POST;


class ALIGN16 CStdMemAlloc : public IMemAlloc
{
public:
	CStdMemAlloc()
	  :	m_pfnFailHandler( DefaultFailHandler ),
		m_sMemoryAllocFailed( (size_t)0 )
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
	size_t CallAllocFailHandler( size_t nBytes ) { return (*m_pfnFailHandler)( nBytes); }

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

	MemAllocFailHandler_t m_pfnFailHandler;
	size_t				m_sMemoryAllocFailed;
} ALIGN16_POST;

// dimhotepus: Apply alignment only here.
#pragma pack(pop)
