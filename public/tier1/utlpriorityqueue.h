//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Binary-heap priority queue. The head (index 0) holds the element
//          with GREATEST priority.
//
// $NoKeywords: $
//=============================================================================//

#ifndef UTLPRIORITYQUEUE_H
#define UTLPRIORITYQUEUE_H

#include <utility>  // std::move

#include "utlvector.h"
#include "tier0/dbg.h"

// T is the type stored in the queue, it must include the priority
// The head of the list contains the element with GREATEST priority
// configure the LessFunc_t to get the desired queue order
template< class T >
class CUtlPriorityQueue
{
public:
	// Returns true if the first parameter is "less priority" than the second
	// Items that are "less priority" sort toward the tail of the queue
	using LessFunc_t = bool (*)(const T &, const T &);

	using ElemType_t = T;

	// lessfunc is required, but may be set after construction with SetLessFunc
	CUtlPriorityQueue( intp growSize = 0, intp initSize = 0, LessFunc_t lessfunc = 0 );
	CUtlPriorityQueue( T *pMemory, intp numElements, LessFunc_t lessfunc = 0 );

	// Gets particular elements
	[[nodiscard]] inline T const &ElementAtHead() const
	{
		Assert( Count() > 0 );
		return m_heap.Element( 0 );
	}

	[[nodiscard]] inline bool IsValidIndex( intp index ) const { return m_heap.IsValidIndex( index ); }

	// O(lgn) to rebalance the heap
	void		RemoveAtHead();
	void		RemoveAt( intp index );

	// O(lgn) to rebalance the heap
	void		Insert( T const &element );

	// Sets the less func
	void		SetLessFunc( LessFunc_t func );

	// Returns the count of elements in the queue
	[[nodiscard]] inline intp	Count() const { return m_heap.Count(); }

	// Doesn't deallocate memory
	void		RemoveAll() { m_heap.RemoveAll(); }

	// Memory deallocation
	void		Purge() { m_heap.Purge(); }

	[[nodiscard]] inline const T &Element( intp index ) const { return m_heap.Element( index ); }

protected:
	CUtlVector<T>	m_heap;

	// Used for sorting.
	LessFunc_t		m_LessFunc;

	void		Swap( intp index1, intp index2 );
	void		SiftUp( intp index );
	void		SiftDown( intp index );
};

template< class T >
inline CUtlPriorityQueue<T>::CUtlPriorityQueue( intp growSize, intp initSize, LessFunc_t lessfunc ) :
	m_heap( growSize, initSize ), m_LessFunc( lessfunc )
{
}

template< class T >
inline CUtlPriorityQueue<T>::CUtlPriorityQueue( T *pMemory, intp numElements, LessFunc_t lessfunc ) :
	m_heap( pMemory, numElements ), m_LessFunc( lessfunc )
{
}

// dimhotepus: Swap.
template <class T>
void CUtlPriorityQueue<T>::Swap( intp index1, intp index2 )
{
	T tmp = std::move( m_heap[index1] );
	m_heap[index1] = std::move( m_heap[index2] );
	m_heap[index2] = std::move( tmp );
}

// dimhotepus: Moves the element at index toward the head until its parent is not less priority.
template <class T>
void CUtlPriorityQueue<T>::SiftUp( intp index )
{
	while ( index > 0 )
	{
		intp parent = ( index - 1 ) / 2;
		// Child is less priority than parent: heap property holds, stop.
		if ( m_LessFunc( m_heap[index], m_heap[parent] ) )
			break;

		Swap( parent, index );
		index = parent;
	}
}

// dimhotepus: Moves the element at index toward the tail until both children are less priority.
template <class T>
void CUtlPriorityQueue<T>::SiftDown( intp index )
{
	const intp count = Count();
	while ( true )
	{
		intp left = 2 * index + 1;
		if ( left >= count )
			break;

		// Pick the higher-priority child
		intp larger = left;
		intp right = left + 1;
		if ( right < count && m_LessFunc( m_heap[left], m_heap[right] ) )
			larger = right;

		// Current element already outranks its higher child: heap property holds
		if ( !m_LessFunc( m_heap[index], m_heap[larger] ) )
			break;

		Swap( index, larger );
		index = larger;
	}
}

template <class T>
void CUtlPriorityQueue<T>::RemoveAtHead()
{
	RemoveAt( 0 );
}

template <class T>
void CUtlPriorityQueue<T>::RemoveAt( intp index )
{
	Assert( IsValidIndex( index ) );
	if ( !IsValidIndex( index ) )
		return;

	// FastRemove moves the last element into the hole
	m_heap.FastRemove( index );

	// Removed the last element: nothing to rebalance
	if ( index >= Count() )
		return;

	// The moved element may outrank its new parent (sift up)
	// or be outranked by its children (sift down). Only one can happen,
	// so calling both is safe.
	SiftUp( index );
	SiftDown( index );
}

template <class T>
void CUtlPriorityQueue<T>::Insert( T const &element )
{
	// dimhotepus: Copy first: if element refers to an item inside m_heap,
	// AddToTail may reallocate and invalidate the reference.
	T tmp = element;

	intp index = m_heap.AddToTail();
	m_heap[index] = tmp;
	SiftUp( index );
}

template <class T>
void CUtlPriorityQueue<T>::SetLessFunc( LessFunc_t lessfunc )
{
	m_LessFunc = lessfunc;
}

#endif // UTLPRIORITYQUEUE_H
