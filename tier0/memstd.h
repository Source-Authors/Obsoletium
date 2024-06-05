//========= Copyright Valve Corporation, All rights reserved. ============//
//-----------------------------------------------------------------------------
// NOTE! This should never be called directly from leaf code
// Just use new,delete,malloc,free etc. They will call into this eventually
//-----------------------------------------------------------------------------
#include "pch_tier0.h"

#if defined(_WIN32)
#if !defined(_X360)
#define WIN_32_LEAN_AND_MEAN
#include <Windows.h>
#else
#undef Verify
#define _XBOX
#include <xtl.h>
#undef _XBOX
#include "xbox/xbox_win32stubs.h"
#endif
#endif

#include <malloc.h>
#include <algorithm>
#include "tier0/dbg.h"
#include "tier0/memalloc.h"
#include "tier0/threadtools.h"
#include "tier0/tslist.h"
#include "mem_helpers.h"

#pragma pack(4)

#ifdef _X360
#define USE_PHYSICAL_SMALL_BLOCK_HEAP 1
#endif


// #define NO_SBH	1


#define MIN_SBH_BLOCK	8
#define MIN_SBH_ALIGN	8
#define MAX_SBH_BLOCK	2048
#define MAX_POOL_REGION (4*1024*1024)
#if !defined(_X360)
#define SBH_PAGE_SIZE		(4*1024)
#define COMMIT_SIZE		(16*SBH_PAGE_SIZE)
#else
#define SBH_PAGE_SIZE		(64*1024)
#define COMMIT_SIZE		(SBH_PAGE_SIZE)
#endif
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
	size_t GetBlockSize();
	bool IsOwner( void *p );
	void *Alloc();
	void Free( void *p );
	int CountFreeBlocks();
	intp GetCommittedSize();
	int CountCommittedBlocks();
	int CountAllocatedBlocks();
	int Compact();

private:

	typedef TSLNodeBase_t FreeBlock_t;
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
	bool ShouldUse( size_t nBytes );
	bool IsOwner( void * p );
	void *Alloc( size_t nBytes );
	void *Realloc( void *p, size_t nBytes );
	void Free( void *p );
	size_t GetSize( void *p );
	void DumpStats( FILE *pFile = NULL );
	int Compact();

private:
	CSmallBlockPool *FindPool( size_t nBytes );
	CSmallBlockPool *FindPool( void *p );

	CSmallBlockPool *m_PoolLookup[MAX_SBH_BLOCK >> 2];
	CSmallBlockPool m_Pools[NUM_POOLS];
	byte *m_pBase;
	byte *m_pLimit;
} ALIGN16_POST;

#ifdef USE_PHYSICAL_SMALL_BLOCK_HEAP
#define BYTES_X360_SBH (32*1024*1024)
#define PAGESIZE_X360_SBH (64*1024)
class CX360SmallBlockPool
{
public:
	void Init( unsigned nBlockSize );
	size_t GetBlockSize();
	bool IsOwner( void *p );
	void *Alloc();
	void Free( void *p );
	int CountFreeBlocks();
	intp GetCommittedSize();
	int CountCommittedBlocks();
	int CountAllocatedBlocks();

	static CX360SmallBlockPool *FindPool( void *p )
	{
		int index = (size_t)((byte *)p - gm_pPhysicalBase) / PAGESIZE_X360_SBH;
		if ( index < 0 || index >= ARRAYSIZE(gm_AddressToPool) )
			return NULL;
		return gm_AddressToPool[ index ];
	}

private:
	friend class CX360SmallBlockHeap;

	typedef TSLNodeBase_t FreeBlock_t;
	class CFreeList : public CTSListBase
	{
	public:
		void Push( void *p ) { CTSListBase::Push( (TSLNodeBase_t *)p );	}
	};

	CFreeList		m_FreeList;

	unsigned		m_nBlockSize;
	unsigned		m_CommittedSize;

	CInterlockedPtr<byte> m_pNextAlloc;
	byte *			m_pCurBlockEnd;

	CThreadFastMutex m_CommitMutex;

	static CX360SmallBlockPool *gm_AddressToPool[BYTES_X360_SBH/PAGESIZE_X360_SBH];

	static byte *gm_pPhysicalBlock;
	static byte *gm_pPhysicalBase;
	static byte *gm_pPhysicalLimit;
};


class CX360SmallBlockHeap
{
public:
	CX360SmallBlockHeap();
	bool ShouldUse( size_t nBytes );
	bool IsOwner( void * p );
	void *Alloc( size_t nBytes );
	void *Realloc( void *p, size_t nBytes );
	void Free( void *p );
	size_t GetSize( void *p );
	void DumpStats( FILE *pFile = NULL );

	CSmallBlockHeap *GetStandardSBH();

private:
	CX360SmallBlockPool *FindPool( size_t nBytes );
	CX360SmallBlockPool *FindPool( void *p );

	CX360SmallBlockPool *m_PoolLookup[MAX_SBH_BLOCK >> 2];
	CX360SmallBlockPool m_Pools[NUM_POOLS];
};
#endif


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
	virtual ~CStdMemAlloc() {}

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
	int CrtCheckMemory( void ) override;
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
#ifdef USE_PHYSICAL_SMALL_BLOCK_HEAP
	CX360SmallBlockHeap m_LargePageSmallBlockHeap;
#endif
#endif

#if defined( _MEMTEST )
	virtual void SetStatsExtraInfo( const char *pMapName, const char *pComment );
#endif

	size_t MemoryAllocFailed() override;

	void SetCRTAllocFailed( size_t nMemSize );

	MemAllocFailHandler_t m_pfnFailHandler;
	size_t				m_sMemoryAllocFailed;
} ALIGN16_POST;


