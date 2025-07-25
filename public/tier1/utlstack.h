//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
// A stack based on a growable array
//=============================================================================//

#ifndef UTLSTACK_H
#define UTLSTACK_H

#include <cstring>
#include "utlmemory.h"
#include "tier0/dbg.h"


//-----------------------------------------------------------------------------
// The CUtlStack class:
// A growable stack class which doubles in size by default.
// It will always keep all elements consecutive in memory, and may move the
// elements around in memory (via a realloc) when elements are pushed or
// popped. Clients should therefore refer to the elements of the stack
// by index (they should *never* maintain pointers to elements in the stack).
//-----------------------------------------------------------------------------

template< class T, class M = CUtlMemory< T > > 
class CUtlStack
{
public:
	// constructor, destructor
	CUtlStack( intp growSize = 0, intp initSize = 0 );
	~CUtlStack();

	void CopyFrom( const CUtlStack<T, M> &from );

	// element access
	T& operator[]( intp i );
	T const& operator[]( intp i ) const;
	T& Element( intp i );
	T const& Element( intp i ) const;

	// Gets the base address (can change when adding elements!)
	T* Base();
	T const* Base() const;

	// Looks at the stack top
	T& Top();
	T const& Top() const;

	// Size
	intp Count() const;

	// Is element index valid?
	bool IsIdxValid( intp i ) const;

	// Adds an element, uses default constructor
	intp Push();

	// Adds an element, uses copy constructor
	intp Push( T const& src );

	// Adds an element, uses move constructor
	intp Push( T&& src );

	// Pops the stack
	void Pop();
	void Pop( T& oldTop );
	void PopMultiple( intp num );

	// Makes sure we have enough memory allocated to store a requested # of elements
	void EnsureCapacity( intp num );

	// Clears the stack, no deallocation
	void Clear();

	// Memory deallocation
	void Purge();

private:
	// Grows the stack allocation
	void GrowStack();

	// For easier access to the elements through the debugger
	void ResetDbgInfo();

	M m_Memory;
	intp m_Size;

	// For easier access to the elements through the debugger
	T* m_pElements;
};


//-----------------------------------------------------------------------------
// For easier access to the elements through the debugger
//-----------------------------------------------------------------------------

template< class T, class M >
inline void CUtlStack<T,M>::ResetDbgInfo()
{
	m_pElements = m_Memory.Base();
}

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------

template< class T, class M >
CUtlStack<T,M>::CUtlStack( intp growSize, intp initSize )	: 
	m_Memory(growSize, initSize), m_Size(0)
{
	ResetDbgInfo();
}

template< class T, class M >
CUtlStack<T,M>::~CUtlStack()
{
	Purge();
}


//-----------------------------------------------------------------------------
// copy into
//-----------------------------------------------------------------------------

template< class T, class M >
void CUtlStack<T,M>::CopyFrom( const CUtlStack<T, M> &from )
{
	Purge();
	EnsureCapacity( from.Count() );
	for ( intp i = 0; i < from.Count(); i++ )
	{
		Push( from[i] );
	}
}

//-----------------------------------------------------------------------------
// element access
//-----------------------------------------------------------------------------

template< class T, class M >
inline T& CUtlStack<T,M>::operator[]( intp i )
{
	Assert( IsIdxValid(i) );
	return m_Memory[i];
}

template< class T, class M >
inline T const& CUtlStack<T,M>::operator[]( intp i ) const
{
	Assert( IsIdxValid(i) );
	return m_Memory[i];
}

template< class T, class M >
inline T& CUtlStack<T,M>::Element( intp i )
{
	Assert( IsIdxValid(i) );
	return m_Memory[i];
}

template< class T, class M >
inline T const& CUtlStack<T,M>::Element( intp i ) const
{
	Assert( IsIdxValid(i) );
	return m_Memory[i];
}


//-----------------------------------------------------------------------------
// Gets the base address (can change when adding elements!)
//-----------------------------------------------------------------------------

template< class T, class M >
inline T* CUtlStack<T,M>::Base()
{
	return m_Memory.Base();
}

template< class T, class M >
inline T const* CUtlStack<T,M>::Base() const
{
	return m_Memory.Base();
}

//-----------------------------------------------------------------------------
// Returns the top of the stack
//-----------------------------------------------------------------------------

template< class T, class M >
inline T& CUtlStack<T,M>::Top()
{
	Assert( m_Size > 0 );
	return Element(m_Size-1);
}

template< class T, class M >
inline T const& CUtlStack<T,M>::Top() const
{
	Assert( m_Size > 0 );
	return Element(m_Size-1);
}

//-----------------------------------------------------------------------------
// Size
//-----------------------------------------------------------------------------

template< class T, class M >
inline intp CUtlStack<T,M>::Count() const
{
	return m_Size;
}


//-----------------------------------------------------------------------------
// Is element index valid?
//-----------------------------------------------------------------------------

template< class T, class M >
inline bool CUtlStack<T,M>::IsIdxValid( intp i ) const
{
	return (i >= 0) && (i < m_Size);
}
 
//-----------------------------------------------------------------------------
// Grows the stack
//-----------------------------------------------------------------------------

template< class T, class M >
void CUtlStack<T,M>::GrowStack()
{
	if (m_Size >= m_Memory.NumAllocated())
		m_Memory.Grow();

	++m_Size;

	ResetDbgInfo();
}

//-----------------------------------------------------------------------------
// Makes sure we have enough memory allocated to store a requested # of elements
//-----------------------------------------------------------------------------

template< class T, class M >
void CUtlStack<T,M>::EnsureCapacity( intp num )
{
	m_Memory.EnsureCapacity(num);
	ResetDbgInfo();
}


//-----------------------------------------------------------------------------
// Adds an element, uses default constructor
//-----------------------------------------------------------------------------

template< class T, class M >
intp CUtlStack<T,M>::Push()
{
	GrowStack();
	Construct( std::addressof( Element(m_Size-1) ) );
	return m_Size - 1;
}

//-----------------------------------------------------------------------------
// Adds an element, uses copy constructor
//-----------------------------------------------------------------------------

template< class T, class M >
intp CUtlStack<T,M>::Push( T const& src )
{
	GrowStack();
	CopyConstruct( std::addressof( Element(m_Size-1) ), src );
	return m_Size - 1;
}

//-----------------------------------------------------------------------------
// Adds an element, uses copy constructor
//-----------------------------------------------------------------------------

template< class T, class M >
intp CUtlStack<T,M>::Push( T&& src )
{
	GrowStack();
	MoveConstruct( std::addressof( Element(m_Size-1) ), std::move( src ) );
	return m_Size - 1;
}


//-----------------------------------------------------------------------------
// Pops the stack
//-----------------------------------------------------------------------------

template< class T, class M >
void CUtlStack<T,M>::Pop()
{
	Assert( m_Size > 0 );
	Destruct( std::addressof( Element(m_Size-1) ) );
	--m_Size;
}

template< class T, class M >
void CUtlStack<T,M>::Pop( T& oldTop )
{
	Assert( m_Size > 0 );
	oldTop = Top();
	Pop();
}

template< class T, class M >
void CUtlStack<T,M>::PopMultiple( intp num )
{
	Assert( m_Size >= num );
	for ( intp i = 0; i < num; ++i )
		Destruct( std::addressof( Element( m_Size - i - 1 ) ) );
	m_Size -= num;
}


//-----------------------------------------------------------------------------
// Element removal
//-----------------------------------------------------------------------------

template< class T, class M >
void CUtlStack<T,M>::Clear()
{
	for (intp i = m_Size; --i >= 0; )
		Destruct( std::addressof( Element(i) ) );

	m_Size = 0;
}


//-----------------------------------------------------------------------------
// Memory deallocation
//-----------------------------------------------------------------------------

template< class T, class M >
void CUtlStack<T,M>::Purge()
{
	Clear();
	m_Memory.Purge( );
	ResetDbgInfo();
}

#endif // UTLSTACK_H
