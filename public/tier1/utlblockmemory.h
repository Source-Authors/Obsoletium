//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
// A growable memory class.
//===========================================================================//

#ifndef UTLBLOCKMEMORY_H
#define UTLBLOCKMEMORY_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/dbg.h"
#include "tier0/platform.h"
#include "mathlib/mathlib.h"

#include "tier0/memalloc.h"
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------

#ifdef UTBLOCKLMEMORY_TRACK
#define UTLBLOCKMEMORY_TRACK_ALLOC()		MemAlloc_RegisterAllocation( "Sum of all UtlBlockMemory", 0, NumAllocated() * sizeof(T), NumAllocated() * sizeof(T), 0 )
#define UTLBLOCKMEMORY_TRACK_FREE()		if ( !m_pMemory ) ; else MemAlloc_RegisterDeallocation( "Sum of all UtlBlockMemory", 0, NumAllocated() * sizeof(T), NumAllocated() * sizeof(T), 0 )
#else
#define UTLBLOCKMEMORY_TRACK_ALLOC()		((void)0)
#define UTLBLOCKMEMORY_TRACK_FREE()		((void)0)
#endif


//-----------------------------------------------------------------------------
// The CUtlBlockMemory class:
// A growable memory class that allocates non-sequential blocks, but is indexed sequentially
//-----------------------------------------------------------------------------
template< typename T, typename I >
class CUtlBlockMemory
{
public:
	// constructor, destructor
	CUtlBlockMemory( intp nGrowSize = 0, intp nInitSize = 0 );
	~CUtlBlockMemory();

	// Set the size by which the memory grows - round up to the next power of 2
	void Init( intp nGrowSize = 0, intp nInitSize = 0 );

	// here to match CUtlMemory, but only used by ResetDbgInfo, so it can just return NULL
	[[nodiscard]] T* Base() { return nullptr; }
	[[nodiscard]] const T* Base() const { return nullptr; }

	class Iterator_t
	{
	public:
		constexpr Iterator_t( I i ) : index( i ) {}
		I index;

		[[nodiscard]] constexpr bool operator==( const Iterator_t it ) const	{ return index == it.index; }
		[[nodiscard]] constexpr bool operator!=( const Iterator_t it ) const	{ return index != it.index; }
	};
	[[nodiscard]] Iterator_t First() const							{ return Iterator_t( IsIdxValid( 0 ) ? 0 : InvalidIndex() ); }
	[[nodiscard]] Iterator_t Next( const Iterator_t &it ) const		{ return Iterator_t( IsIdxValid( it.index + 1 ) ? it.index + 1 : InvalidIndex() ); }
	[[nodiscard]] I GetIndex( const Iterator_t &it ) const			{ return it.index; }
	[[nodiscard]] bool IsIdxAfter( I i, const Iterator_t &it ) const	{ return i > it.index; }
	[[nodiscard]] bool IsValidIterator( const Iterator_t &it ) const	{ return IsIdxValid( it.index ); }
	[[nodiscard]] constexpr Iterator_t InvalidIterator() const		{ return Iterator_t( InvalidIndex() ); }

	// element access
	[[nodiscard]] T& operator[]( I i );
	[[nodiscard]] const T& operator[]( I i ) const;
	[[nodiscard]] T& Element( I i );
	[[nodiscard]] const T& Element( I i ) const;

	// Can we use this index?
	[[nodiscard]] bool IsIdxValid( I i ) const;
	[[nodiscard]] static constexpr I InvalidIndex() { return static_cast<I>(-1); }

	void Swap( CUtlBlockMemory< T, I > &mem );

	// Size
	[[nodiscard]] intp NumAllocated() const;
	[[nodiscard]] intp Count() const { return NumAllocated(); }

	// Grows memory by max(num,growsize) rounded up to the next power of 2, and returns the allocation index/ptr
	void Grow( intp num = 1 );

	// Makes sure we've got at least this much memory
	void EnsureCapacity( intp num );

	// Memory deallocation
	void Purge();

	// Purge all but the given number of elements
	void Purge( intp numElements );

protected:
	[[nodiscard]] intp Index( intp major, intp minor ) const { return ( major << m_nIndexShift ) | minor; }
	[[nodiscard]] intp MajorIndex( intp i ) const { return i >> m_nIndexShift; }
	[[nodiscard]] intp MinorIndex( intp i ) const { return i & m_nIndexMask; }
	void ChangeSize( intp nBlocks );
	[[nodiscard]] intp NumElementsInBlock() const { return m_nIndexMask + 1; }

	T** m_pMemory;
	intp m_nBlocks;

#ifdef PLATFORM_64BITS
	intp m_nIndexMask : 58;
	intp m_nIndexShift : 6;
#else
	int m_nIndexMask : 27;
	int m_nIndexShift : 5;
#endif
};

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------

template< class T, class I >
CUtlBlockMemory<T,I>::CUtlBlockMemory( intp nGrowSize, intp nInitAllocationCount )
: m_pMemory( 0 ), m_nBlocks( 0 ), m_nIndexMask( 0 ), m_nIndexShift( 0 )
{
	Init( nGrowSize, nInitAllocationCount );
}

template< class T, class I >
CUtlBlockMemory<T,I>::~CUtlBlockMemory()
{
	Purge();
}


//-----------------------------------------------------------------------------
// Fast swap
//-----------------------------------------------------------------------------
template< class T, class I >
void CUtlBlockMemory<T,I>::Swap( CUtlBlockMemory< T, I > &mem )
{
	using std::swap;
	swap( m_pMemory, mem.m_pMemory );
	swap( m_nBlocks, mem.m_nBlocks );
	swap( m_nIndexMask, mem.m_nIndexMask );
	swap( m_nIndexShift, mem.m_nIndexShift );
}


//-----------------------------------------------------------------------------
// Set the size by which the memory grows - round up to the next power of 2
//-----------------------------------------------------------------------------
template< class T, class I >
void CUtlBlockMemory<T,I>::Init( intp nGrowSize /* = 0 */, intp nInitSize /* = 0 */ )
{
	Purge();

	if ( nGrowSize == 0)
	{
		// default grow size is smallest size s.t. c++ allocation overhead is ~6% of block size
		nGrowSize = ( 127 + sizeof( T ) ) / sizeof( T );
	}
	nGrowSize = SmallestPowerOfTwoGreaterOrEqual( (uintp)nGrowSize );
	m_nIndexMask = nGrowSize - 1;

	m_nIndexShift = 0;
	while ( nGrowSize > 1 )
	{
		nGrowSize >>= 1;
		++m_nIndexShift;
	}
	Assert( m_nIndexMask + 1 == ( (intp)1 << m_nIndexShift ) );

	Grow( nInitSize );
}


//-----------------------------------------------------------------------------
// element access
//-----------------------------------------------------------------------------
template< class T, class I >
inline T& CUtlBlockMemory<T,I>::operator[]( I i )
{
	Assert( IsIdxValid(i) );
	T *pBlock = m_pMemory[ MajorIndex( i ) ];
	return pBlock[ MinorIndex( i ) ];
}

template< class T, class I >
inline const T& CUtlBlockMemory<T,I>::operator[]( I i ) const
{
	Assert( IsIdxValid(i) );
	const T *pBlock = m_pMemory[ MajorIndex( i ) ];
	return pBlock[ MinorIndex( i ) ];
}

template< class T, class I >
inline T& CUtlBlockMemory<T,I>::Element( I i )
{
	Assert( IsIdxValid(i) );
	T *pBlock = m_pMemory[ MajorIndex( i ) ];
	return pBlock[ MinorIndex( i ) ];
}

template< class T, class I >
inline const T& CUtlBlockMemory<T,I>::Element( I i ) const
{
	Assert( IsIdxValid(i) );
	const T *pBlock = m_pMemory[ MajorIndex( i ) ];
	return pBlock[ MinorIndex( i ) ];
}


//-----------------------------------------------------------------------------
// Size
//-----------------------------------------------------------------------------
template< class T, class I >
inline intp CUtlBlockMemory<T,I>::NumAllocated() const
{
	return m_nBlocks * NumElementsInBlock();
}


//-----------------------------------------------------------------------------
// Is element index valid?
//-----------------------------------------------------------------------------
template< class T, class I >
inline bool CUtlBlockMemory<T,I>::IsIdxValid( I i ) const
{
	return ( i >= 0 ) && ( MajorIndex( i ) < m_nBlocks );
}

template< class T, class I >
void CUtlBlockMemory<T,I>::Grow( intp num )
{
	if ( num <= 0 )
		return;

	intp nBlockSize = NumElementsInBlock();
	intp nBlocks = ( num + nBlockSize - 1 ) / nBlockSize;

	ChangeSize( m_nBlocks + nBlocks );
}

template< class T, class I >
void CUtlBlockMemory<T,I>::ChangeSize( intp nBlocks )
{
	UTLBLOCKMEMORY_TRACK_FREE(); // this must stay before the recalculation of m_nBlocks, since it implicitly uses the old value

	intp nBlocksOld = m_nBlocks;
	m_nBlocks = nBlocks;

	UTLBLOCKMEMORY_TRACK_ALLOC(); // this must stay after the recalculation of m_nBlocks, since it implicitly uses the new value

	if ( m_pMemory )
	{
		// free old blocks if shrinking
		// Only possible if m_pMemory is non-NULL (and avoids PVS-Studio warning)
		for ( intp i = m_nBlocks; i < nBlocksOld; ++i )
		{
			UTLBLOCKMEMORY_TRACK_FREE();
			free( (void*)m_pMemory[ i ] );
		}

		MEM_ALLOC_CREDIT_CLASS();
		m_pMemory = (T**)realloc( m_pMemory, m_nBlocks * sizeof(T*) );
		Assert( m_pMemory );
	}
	else
	{
		MEM_ALLOC_CREDIT_CLASS();
		m_pMemory = (T**)malloc( m_nBlocks * sizeof(T*) );
		Assert( m_pMemory );
	}

	if ( !m_pMemory )
	{
		Error( "CUtlBlockMemory overflow!\n" );
	}

	// allocate new blocks if growing
	intp nBlockSize = NumElementsInBlock();
	for ( intp i = nBlocksOld; i < m_nBlocks; ++i )
	{
		MEM_ALLOC_CREDIT_CLASS();
		m_pMemory[ i ] = (T*)malloc( nBlockSize * sizeof( T ) );
		Assert( m_pMemory[ i ] );
	}
}


//-----------------------------------------------------------------------------
// Makes sure we've got at least this much memory
//-----------------------------------------------------------------------------
template< class T, class I >
inline void CUtlBlockMemory<T,I>::EnsureCapacity( intp num )
{
	Grow( num - NumAllocated() );
}


//-----------------------------------------------------------------------------
// Memory deallocation
//-----------------------------------------------------------------------------
template< class T, class I >
void CUtlBlockMemory<T,I>::Purge()
{
	if ( !m_pMemory )
		return;

	for ( intp i = 0; i < m_nBlocks; ++i )
	{
		UTLBLOCKMEMORY_TRACK_FREE();
		free( (void*)m_pMemory[ i ] );
	}
	m_nBlocks = 0;

	UTLBLOCKMEMORY_TRACK_FREE();
	free( (void*)m_pMemory );
	m_pMemory = 0;
}

template< class T, class I >
void CUtlBlockMemory<T,I>::Purge( intp numElements )
{
	Assert( numElements >= 0 );

	intp nAllocated = NumAllocated();
	if ( numElements > nAllocated )
	{
		// Ensure this isn't a grow request in disguise.
		Assert( numElements <= nAllocated ); //-V547
		return;
	}

	if ( numElements <= 0 )
	{
		Purge();
		return;
	}

	intp nBlockSize = NumElementsInBlock();
	intp nBlocksOld = m_nBlocks;
	intp nBlocks = ( numElements + nBlockSize - 1 ) / nBlockSize;

	// If the number of blocks is the same as the allocated number of blocks, we are done.
	if ( nBlocks == m_nBlocks )
		return;

	ChangeSize( nBlocks );
}

#include "tier0/memdbgoff.h"

#endif // UTLBLOCKMEMORY_H
