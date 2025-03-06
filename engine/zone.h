//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef ZONE_H
#define ZONE_H
#pragma once

#include "tier0/dbg.h"

void Memory_Init();
void Memory_Shutdown();

void* Hunk_Alloc(size_t size, bool bClear = true);

template<typename T>
T* Hunk_Alloc(size_t count, bool bClear = true)
{
  return static_cast<T*>(Hunk_Alloc(count * sizeof(T), bClear));
}

void *Hunk_AllocName(size_t size, const char *name, bool bClear = true );

template<typename T>
T* Hunk_AllocName(size_t count, const char *name, bool bClear = true)
{
  return static_cast<T*>(Hunk_AllocName(count * sizeof(T), name, bClear));
}

intp Hunk_LowMark();
void Hunk_FreeToLowMark (intp mark);

intp Hunk_MallocSize();
intp Hunk_Size();

void Hunk_Print();

template< typename T >
class CHunkMemory
{
public:
	// constructor, destructor
	CHunkMemory( int nGrowSize = 0, int nInitSize = 0 )		{ m_pMemory = NULL; m_nAllocated = 0; if ( nInitSize ) Grow( nInitSize ); }
	CHunkMemory( T* pMemory, int numElements )				{ Assert( 0 ); }

	// Can we use this index?
	bool IsIdxValid( int i ) const							{ return (i >= 0) && (i < m_nAllocated); }

	// Gets the base address
	T* Base()												{ return m_pMemory; }
	const T* Base() const									{ return m_pMemory; }

	// element access
	T& operator[]( int i )									{ Assert( IsIdxValid(i) ); return Base()[i];	}
	const T& operator[]( int i ) const						{ Assert( IsIdxValid(i) ); return Base()[i];	}
	T& Element( int i )										{ Assert( IsIdxValid(i) ); return Base()[i];	}
	const T& Element( int i ) const							{ Assert( IsIdxValid(i) ); return Base()[i];	}

	// Attaches the buffer to external memory....
	void SetExternalBuffer( T* pMemory, int numElements )	{ Assert( 0 ); }

	// Size
	int NumAllocated() const								{ return m_nAllocated; }
	int Count() const										{ return m_nAllocated; }

	// Grows the memory, so that at least allocated + num elements are allocated
	void Grow( int num = 1 )								{ Assert( !m_nAllocated ); m_pMemory = Hunk_Alloc<T>( num, false ); m_nAllocated = num; }

	// Makes sure we've got at least this much memory
	void EnsureCapacity( int num )							{ Assert( num <= m_nAllocated ); }

	// Memory deallocation
	void Purge()											{ m_nAllocated = 0; }

	// Purge all but the given number of elements (NOT IMPLEMENTED IN )
	void Purge( int numElements )							{ Assert( 0 ); }

	// is the memory externally allocated?
	bool IsExternallyAllocated() const						{ return false; }

	// Set the size by which the memory grows
	void SetGrowSize( int size )							{}

private:
	T *m_pMemory;
	int m_nAllocated;
};


#endif // ZONE_H


