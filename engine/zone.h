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
	CHunkMemory( intp nGrowSize = 0, intp nInitSize = 0 )	{ m_pMemory = NULL; m_nAllocated = 0; if ( nInitSize ) Grow( nInitSize ); }
	CHunkMemory( T* pMemory, intp numElements )				= delete;

	// Can we use this index?
	bool IsIdxValid( intp i ) const							{ return (i >= 0) && (i < m_nAllocated); }

	// Gets the base address
	T* Base()												{ return m_pMemory; }
	const T* Base() const									{ return m_pMemory; }

	// element access
	T& operator[]( intp i )									{ Assert( IsIdxValid(i) ); return Base()[i];	}
	const T& operator[]( intp i ) const						{ Assert( IsIdxValid(i) ); return Base()[i];	}
	T& Element( intp i )									{ Assert( IsIdxValid(i) ); return Base()[i];	}
	const T& Element( intp i ) const						{ Assert( IsIdxValid(i) ); return Base()[i];	}

	// Attaches the buffer to external memory....
	void SetExternalBuffer( T* pMemory, intp numElements )	{ Assert( 0 ); }

	// Size
	intp NumAllocated() const								{ return m_nAllocated; }
	intp Count() const										{ return m_nAllocated; }

	// Grows the memory, so that at least allocated + num elements are allocated
	void Grow( intp num = 1 )								{ Assert( !m_nAllocated ); m_pMemory = Hunk_Alloc<T>( num, false ); m_nAllocated = num; }

	// Makes sure we've got at least this much memory
	void EnsureCapacity( intp num )							{ Assert( num <= m_nAllocated ); }

	// Memory deallocation
	void Purge()											{ m_nAllocated = 0; }

	// Purge all but the given number of elements (NOT IMPLEMENTED IN )
	void Purge( intp numElements )							{ Assert( 0 ); }

	// is the memory externally allocated?
	bool IsExternallyAllocated() const						{ return false; }

	// Set the size by which the memory grows
	void SetGrowSize( intp size )							{}

private:
	T *m_pMemory;
	intp m_nAllocated;
};


#endif // ZONE_H


