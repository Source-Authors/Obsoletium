//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "tier1/memstack.h"

#if defined( _WIN32 ) && !defined( _X360 )
#include "winlite.h"
#define VA_COMMIT_FLAGS MEM_COMMIT
#define VA_RESERVE_FLAGS MEM_RESERVE
#endif

#include "tier0/dbg.h"
#include "tier1/utlmap.h"
#include "tier0/memdbgon.h"

#ifdef _WIN32
#pragma warning(disable:4073)
#pragma init_seg(lib)
#endif

//-----------------------------------------------------------------------------

MEMALLOC_DEFINE_EXTERNAL_TRACKING(CMemoryStack);

//-----------------------------------------------------------------------------

CMemoryStack::CMemoryStack()
 : 	m_pNextAlloc( NULL ),
	m_pCommitLimit( NULL ),
	m_pAllocLimit( NULL ),
	m_pBase( NULL ),
 	m_maxSize( 0 ),
	m_alignment( 16 )
#if defined(_WIN32)
 	, m_commitSize( 0 )
	, m_minCommit( 0 )
#endif
{
}
	
//-------------------------------------

CMemoryStack::~CMemoryStack()
{
	if ( m_pBase )
		Term();
}

//-------------------------------------

bool CMemoryStack::Init( size_t maxSize, size_t commitSize, size_t initialCommit, size_t alignment )
{
	Assert( !m_pBase );

	m_maxSize = maxSize;
	m_alignment = AlignValue( alignment, 4 );

	Assert( m_alignment == alignment );
	Assert( m_maxSize > 0 );

#if defined(_WIN32)
	if ( commitSize != 0 )
	{
		m_commitSize = commitSize;
	}

	unsigned pageSize;

	SYSTEM_INFO sysInfo;
	GetNativeSystemInfo( &sysInfo );
	Assert( !( sysInfo.dwPageSize & (sysInfo.dwPageSize-1)) );
	pageSize = sysInfo.dwPageSize;

	if ( m_commitSize == 0 )
	{
		m_commitSize = pageSize;
	}
	else
	{
		m_commitSize = AlignValue( m_commitSize, pageSize );
	}

	m_maxSize = AlignValue( m_maxSize, m_commitSize );
	
	Assert( m_maxSize % pageSize == 0 && m_commitSize % pageSize == 0 && m_commitSize <= m_maxSize );

	m_pBase = (unsigned char *)VirtualAlloc( NULL, m_maxSize, VA_RESERVE_FLAGS, PAGE_NOACCESS );
	Assert( m_pBase );
	m_pCommitLimit = m_pNextAlloc = m_pBase;

	if ( initialCommit )
	{
		initialCommit = AlignValue( initialCommit, m_commitSize );
		Assert( initialCommit < m_maxSize );
		if ( !VirtualAlloc( m_pCommitLimit, initialCommit, VA_COMMIT_FLAGS, PAGE_READWRITE ) )
			return false;
		m_minCommit = initialCommit;
		m_pCommitLimit += initialCommit;
		MemAlloc_RegisterExternalAllocation( CMemoryStack, GetBase(), GetSize() );
	}

#else
	m_pBase = (byte *)MemAlloc_AllocAligned( m_maxSize, alignment ? alignment : 1 );
	m_pNextAlloc = m_pBase;
	m_pCommitLimit = m_pBase + m_maxSize;
#endif

	m_pAllocLimit = m_pBase + m_maxSize;

	return ( m_pBase != NULL );
}

//-------------------------------------

void CMemoryStack::Term()
{
	FreeAll();
	if ( m_pBase )
	{
#if defined(_WIN32)
		VirtualFree( m_pBase, 0, MEM_RELEASE );
#else
		MemAlloc_FreeAligned( m_pBase );
#endif
		m_pBase = NULL;
	}
}

//-------------------------------------

intp CMemoryStack::GetSize() const
{ 
#ifdef _WIN32
	return m_pCommitLimit - m_pBase; 
#else
	return m_maxSize;
#endif
}


//-------------------------------------

bool CMemoryStack::CommitTo( byte * RESTRICT pNextAlloc )
{
#if defined(_WIN32)
	unsigned char *	pNewCommitLimit = AlignValue( pNextAlloc, m_commitSize );
	size_t 		commitSize 		= pNewCommitLimit - m_pCommitLimit;
	
	if ( GetSize() )
	{
		MemAlloc_RegisterExternalDeallocation( CMemoryStack, GetBase(), GetSize() );
	}

	if( m_pCommitLimit + commitSize > m_pAllocLimit )
	{
		return false;
	}

	if ( !VirtualAlloc( m_pCommitLimit, commitSize, VA_COMMIT_FLAGS, PAGE_READWRITE ) )
	{
		Assert( 0 );
		return false;
	}
	m_pCommitLimit = pNewCommitLimit;

	if ( GetSize() )
	{
		MemAlloc_RegisterExternalAllocation( CMemoryStack, GetBase(), GetSize() );
	}
	return true;
#else
	Assert( 0 );
	return false;
#endif
}

//-------------------------------------

void CMemoryStack::FreeToAllocPoint( MemoryStackMark_t mark, bool bDecommit )
{
	void *pAllocPoint = m_pBase + mark;
	Assert( pAllocPoint >= m_pBase && pAllocPoint <= m_pNextAlloc );
	
	if ( pAllocPoint >= m_pBase && pAllocPoint < m_pNextAlloc )
	{
		if ( bDecommit )
		{
#if defined(_WIN32)
			unsigned char *pDecommitPoint = AlignValue( (unsigned char *)pAllocPoint, m_commitSize );

			if ( pDecommitPoint < m_pBase + m_minCommit )
			{
				pDecommitPoint = m_pBase + m_minCommit;
			}

			size_t decommitSize = m_pCommitLimit - pDecommitPoint;

			if ( decommitSize > 0 )
			{
				MemAlloc_RegisterExternalDeallocation( CMemoryStack, GetBase(), GetSize() );

				VirtualFree( pDecommitPoint, decommitSize, MEM_DECOMMIT );
				m_pCommitLimit = pDecommitPoint;

				if ( mark > 0 )
				{
					MemAlloc_RegisterExternalAllocation( CMemoryStack, GetBase(), GetSize() );
				}
			}
#endif
		}
		m_pNextAlloc = (unsigned char *)pAllocPoint;
	}
}

//-------------------------------------

void CMemoryStack::FreeAll( bool bDecommit )
{
	if ( m_pBase && m_pCommitLimit - m_pBase > 0 )
	{
		if ( bDecommit )
		{
#if defined(_WIN32)
			MemAlloc_RegisterExternalDeallocation( CMemoryStack, GetBase(), GetSize() );

			VirtualFree( m_pBase, m_pCommitLimit - m_pBase, MEM_DECOMMIT );
			m_pCommitLimit = m_pBase;
#endif
		}
		m_pNextAlloc = m_pBase;
	}
}

//-------------------------------------

void CMemoryStack::Access( void **ppRegion, size_t *pBytes )
{
	*ppRegion = m_pBase;
	*pBytes = ( m_pNextAlloc - m_pBase);
}

//-------------------------------------

void CMemoryStack::PrintContents() const
{
	Msg( "Total used memory:      %zd\n", GetUsed() );
	Msg( "Total committed memory: %zd\n", GetSize() );
}

//-----------------------------------------------------------------------------
