//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef UTLQUEUE_H
#define UTLQUEUE_H

#include <limits> // QueueIter_t invalid value
#include <memory> // std::addressof
#include <type_traits>
#include <utility> // std::move

#include "utlmemory.h"

//#define TEST_UTLQUEUE

// dimhotepus: Do not use an enum for the invalid value: assigning an out-of-range value is UB.
using QueueIter_t = intp;
constexpr inline QueueIter_t QUEUE_ITERATOR_INVALID = std::numeric_limits< intp >::max();

// T is the type stored in the queue.
template< class T, class M = CUtlMemory< T > >
class CUtlQueue
{
public:
	// Growth relocates elements as bytes, so T must support that operation.
	static_assert( std::is_trivially_copyable< T >::value,
	               "CUtlQueue byte-relocates elements during growth; T must be trivially copyable." );
	static_assert( std::is_trivially_destructible< T >::value,
	               "CUtlQueue does not destroy the old instances during byte relocation; "
	               "T must be trivially destructible." );

	CUtlQueue( intp growSize = 0, intp initSize = 0 );
	CUtlQueue( T *pMemory, intp numElements );

	// dimhotepus: Destroys elements still in the queue.
	~CUtlQueue();

	// dimhotepus: Prevent accidental shallow copies of the queue and its memory.
	CUtlQueue( const CUtlQueue & ) = delete;
	CUtlQueue &operator=( const CUtlQueue & ) = delete;

	// Return the item from the front of the queue and delete it.
	[[nodiscard]] T    RemoveAtHead();
	[[nodiscard]] bool RemoveAtHead( T &removedElement );

	// Return the item from the end of the queue and delete it.
	[[nodiscard]] T    RemoveAtTail();
	[[nodiscard]] bool RemoveAtTail( T &removedElement );

	// Return item at the front/end of the queue.
	[[nodiscard]] T const &Head() const;
	[[nodiscard]] T const &Tail() const;

	// Add a new item to the end of the queue.
	void Insert( T const &element );
	void Insert( T &&element );

	// Check if an element with this value already exists in the queue.
	[[nodiscard]] bool Check( T const &element ) const;

	// Iterators may be invalidated by Insert().
	[[nodiscard]] QueueIter_t First() const;
	[[nodiscard]] QueueIter_t Next( QueueIter_t it ) const;
	[[nodiscard]] QueueIter_t Last() const;
	[[nodiscard]] QueueIter_t Previous( QueueIter_t it ) const;
	[[nodiscard]] bool        IsValid( QueueIter_t it ) const;
	[[nodiscard]] T const    &Element( QueueIter_t it ) const;

	// Return the count of elements in the queue.
	[[nodiscard]] intp Count() const;

	// Return whether the queue is empty or not, faster than Count().
	[[nodiscard]] bool IsEmpty() const;

	// Doesn't deallocate memory.
	void RemoveAll();

	// Deallocate memory.
	void Purge();

protected:
	[[nodiscard]] QueueIter_t Next_Unchecked( QueueIter_t it ) const;
	[[nodiscard]] QueueIter_t Previous_Unchecked( QueueIter_t it ) const;
	// dimhotepus: Check intersections.
	[[nodiscard]] bool        ContainsElementAddress( const T *p ) const;

	M m_memory;

	// If m_head == m_tail == QUEUE_ITERATOR_INVALID, then the queue is empty.
	QueueIter_t m_head;
	QueueIter_t m_tail;

#ifdef TEST_UTLQUEUE
	friend void CUtlQueue_Test();
#endif
};

//-----------------------------------------------------------------------------
// CUtlQueueFixed: a queue class with a fixed allocation scheme.
//-----------------------------------------------------------------------------
template< class T, size_t MAX_SIZE >
class CUtlQueueFixed : public CUtlQueue< T, CUtlMemoryFixed< T, MAX_SIZE > >
{
	using BaseClass = CUtlQueue< T, CUtlMemoryFixed< T, MAX_SIZE > >;

public:
	CUtlQueueFixed( intp growSize = 0, intp initSize = 0 ) :
		BaseClass( growSize, initSize )
	{
	}

	CUtlQueueFixed( T *pMemory, intp numElements ) :
		BaseClass( pMemory, numElements )
	{
	}
};

template< class T, class M >
inline CUtlQueue< T, M >::CUtlQueue( intp growSize, intp initSize ) :
	m_memory( growSize, initSize ),
	m_head( QUEUE_ITERATOR_INVALID ),
	m_tail( QUEUE_ITERATOR_INVALID )
{
}

template< class T, class M >
inline CUtlQueue< T, M >::CUtlQueue( T *pMemory, intp numElements ) :
	m_memory( pMemory, numElements ),
	m_head( QUEUE_ITERATOR_INVALID ),
	m_tail( QUEUE_ITERATOR_INVALID )
{
}

template< class T, class M >
inline CUtlQueue< T, M >::~CUtlQueue()
{
	RemoveAll();
}

template< class T, class M >
inline T CUtlQueue< T, M >::RemoveAtHead()
{
	static_assert( std::is_default_constructible< T >::value,
	               "RemoveAtHead() requires a default-constructible T." );
	static_assert( std::is_move_constructible< T >::value,
	               "RemoveAtHead() requires T to be constructible from T&&." );

	T temp;
	// dimhotepus: Check element is removed.
	[[maybe_unused]] const bool ok = RemoveAtHead( temp );
	Assert( ok );
	return temp;
}

template< class T, class M >
inline bool CUtlQueue< T, M >::RemoveAtHead( T &removedElement )
{
	static_assert( std::is_copy_assignable< T >::value,
	               "RemoveAtHead(T&) requires copy-assignable T." );

	Assert( m_head != QUEUE_ITERATOR_INVALID );
	if ( m_head == QUEUE_ITERATOR_INVALID )
	{
		// dimhotepus: Leave the caller's existing object unchanged.
		return false;
	}

	const QueueIter_t it = m_head;
	removedElement = m_memory[it];
	Destruct( std::addressof( m_memory[it] ) );

	if ( m_head == m_tail )
	{
		m_head = m_tail = QUEUE_ITERATOR_INVALID;
	}
	else
	{
		m_head = Next_Unchecked( m_head );
	}

	return true;
}

template< class T, class M >
inline T CUtlQueue< T, M >::RemoveAtTail()
{
	static_assert( std::is_default_constructible< T >::value,
	               "RemoveAtTail() requires a default-constructible T." );
	static_assert( std::is_move_constructible< T >::value,
	               "RemoveAtTail() requires T to be constructible from T&&." );

	T temp;
	// dimhotepus: Check element is removed.
	[[maybe_unused]] const bool ok = RemoveAtTail( temp );
	Assert( ok );
	return temp;
}

template< class T, class M >
inline bool CUtlQueue< T, M >::RemoveAtTail( T &removedElement )
{
	static_assert( std::is_copy_assignable< T >::value,
	               "RemoveAtTail(T&) requires copy-assignable T." );

	Assert( m_tail != QUEUE_ITERATOR_INVALID );
	if ( m_tail == QUEUE_ITERATOR_INVALID )
	{
		// dimhotepus: Leave the caller's existing object unchanged.
		return false;
	}

	const QueueIter_t it = m_tail;
	removedElement = m_memory[it];
	Destruct( std::addressof( m_memory[it] ) );

	if ( m_head == m_tail )
	{
		m_head = m_tail = QUEUE_ITERATOR_INVALID;
	}
	else
	{
		m_tail = Previous_Unchecked( m_tail );
	}

	return true;
}

template< class T, class M >
inline T const &CUtlQueue< T, M >::Head() const
{
	static_assert( std::is_default_constructible< T >::value,
	               "Head() requires a default-constructible T for its empty-queue fallback." );

	Assert( m_head != QUEUE_ITERATOR_INVALID );
	if ( m_head == QUEUE_ITERATOR_INVALID )
	{
		static T dummy;
		return dummy;
	}

	return m_memory[m_head];
}

template< class T, class M >
inline T const &CUtlQueue< T, M >::Tail() const
{
	static_assert( std::is_default_constructible< T >::value,
	               "Tail() requires a default-constructible T for its empty-queue fallback." );

	Assert( m_tail != QUEUE_ITERATOR_INVALID );
	if ( m_tail == QUEUE_ITERATOR_INVALID )
	{
		static T dummy;
		return dummy;
	}

	return m_memory[m_tail];
}

template< class T, class M >
void CUtlQueue< T, M >::Insert( T const &element )
{
	static_assert( std::is_copy_constructible< T >::value,
	               "Insert(const T&) requires copy-constructible T." );

	if ( m_tail == QUEUE_ITERATOR_INVALID )
	{
		m_memory.EnsureCapacity( 1 );

		// dimhotepus: Fixed or failed allocation may still have no usable slot.
		if ( m_memory.NumAllocated() < 1 )
		{
			Assert( m_memory.NumAllocated() >= 1 );
			return;
		}

		CopyConstruct( std::addressof( m_memory[0] ), element );
		// dimhotepus: Commit only after construction.
		m_head = m_tail = QueueIter_t( 0 );
		return;
	}

	QueueIter_t nextTail = Next_Unchecked( m_tail );

	if ( nextTail != m_head )
	{
		// Not full: construct first, then update the tail.
		CopyConstruct( std::addressof( m_memory[nextTail] ), element );
		m_tail = nextTail;
		return;
	}

	// Full: stage the source before Grow(), which may relocate the queue.
	T temp( element );

	const intp oldAllocCount = m_memory.NumAllocated();
	m_memory.Grow();
	const intp newAllocCount = m_memory.NumAllocated();

	// dimhotepus: Avoid writing out of bounds if this memory type cannot grow.
	if ( newAllocCount <= oldAllocCount )
	{
		Assert( newAllocCount > oldAllocCount );
		return;
	}

	const intp        growAmount = newAllocCount - oldAllocCount;
	const QueueIter_t oldHead = m_head;

	if ( oldHead != QueueIter_t( 0 ) )
	{
		// Relocate the wrapped segment into the newly added portion.
		Q_memmove(
		&m_memory[oldHead + growAmount],
		&m_memory[oldHead],
		( oldAllocCount - oldHead ) * sizeof( T ) );

#ifndef NDEBUG
		Q_memset( &m_memory[oldHead], 0xdd, growAmount * sizeof( T ) );
#endif

		m_head = QueueIter_t( oldHead + growAmount );
	}

	nextTail = Next_Unchecked( m_tail );
	CopyConstruct( std::addressof( m_memory[nextTail] ), temp );

	// dimhotepus: Commit only after construction.
	m_tail = nextTail;
}

template< class T, class M >
void CUtlQueue< T, M >::Insert( T &&element )
{
	static_assert( std::is_move_constructible< T >::value,
	               "Insert(T&&) requires T to be constructible from T&&." );

	if ( m_tail == QUEUE_ITERATOR_INVALID )
	{
		m_memory.EnsureCapacity( 1 );

		// dimhotepus: Fixed or failed allocation may still have no usable slot.
		if ( m_memory.NumAllocated() < 1 )
		{
			Assert( m_memory.NumAllocated() >= 1 );
			return;
		}

		MoveConstruct( std::addressof( m_memory[0] ), std::move( element ) );
		m_head = m_tail = QueueIter_t( 0 );
		return;
	}

	QueueIter_t nextTail = Next_Unchecked( m_tail );

	if ( nextTail != m_head )
	{
		MoveConstruct( std::addressof( m_memory[nextTail] ), std::move( element ) );
		m_tail = nextTail;
		return;
	}

	// dimhotepus: Full queue. If element aliases a queued item, remember its iterator
	// before growth so the source can be found after relocation.
	const bool  aliasesQueue = ContainsElementAddress( std::addressof( element ) );
	QueueIter_t aliasedIt = QUEUE_ITERATOR_INVALID;

	if ( aliasesQueue )
	{
		for ( QueueIter_t it = First();
		      it != QUEUE_ITERATOR_INVALID;
		      it = Next( it ) )
		{
			if ( std::addressof( m_memory[it] ) == std::addressof( element ) )
			{
				aliasedIt = it;
				break;
			}
		}

		Assert( aliasedIt != QUEUE_ITERATOR_INVALID );
		if ( aliasedIt == QUEUE_ITERATOR_INVALID )
		{
			return;
		}
	}

	const intp        oldAllocCount = m_memory.NumAllocated();
	const QueueIter_t oldHead = m_head;

	m_memory.Grow();
	const intp newAllocCount = m_memory.NumAllocated();

	if ( newAllocCount <= oldAllocCount )
	{
		Assert( newAllocCount > oldAllocCount );
		return;
	}

	const intp growAmount = newAllocCount - oldAllocCount;

	if ( oldHead != QueueIter_t( 0 ) )
	{
		Q_memmove(
		&m_memory[oldHead + growAmount],
		&m_memory[oldHead],
		( oldAllocCount - oldHead ) * sizeof( T ) );

#ifndef NDEBUG
		Q_memset( &m_memory[oldHead], 0xdd, growAmount * sizeof( T ) );
#endif

		m_head = QueueIter_t( oldHead + growAmount );

		// The high wrapped segment moved; adjust an aliased source iterator.
		if ( aliasesQueue && aliasedIt >= oldHead )
		{
			aliasedIt = QueueIter_t( aliasedIt + growAmount );
		}
	}

	nextTail = Next_Unchecked( m_tail );

	if ( aliasesQueue )
	{
		// dimhotepus: Move only after growth succeeds, so a failed growth leaves the
		// existing queued value unchanged.
		MoveConstruct(
		std::addressof( m_memory[nextTail] ),
		std::move( m_memory[aliasedIt] ) );
	}
	else
	{
		MoveConstruct( std::addressof( m_memory[nextTail] ), std::move( element ) );
	}

	m_tail = nextTail;
}

template< class T, class M >
bool CUtlQueue< T, M >::Check( T const &element ) const
{
	for ( QueueIter_t it = First(); it != QUEUE_ITERATOR_INVALID; it = Next( it ) )
	{
		if ( m_memory[it] == element )
		{
			return true;
		}
	}

	return false;
}

template< class T, class M >
QueueIter_t CUtlQueue< T, M >::First() const
{
	return m_head;
}

template< class T, class M >
QueueIter_t CUtlQueue< T, M >::Next( QueueIter_t it ) const
{
	if ( it == QUEUE_ITERATOR_INVALID )
	{
		return QUEUE_ITERATOR_INVALID;
	}

	if ( it == m_tail )
	{
		return QUEUE_ITERATOR_INVALID;
	}

	Assert( IsValid( it ) );
	if ( !IsValid( it ) )
	{
		return QUEUE_ITERATOR_INVALID;
	}

	return Next_Unchecked( it );
}

template< class T, class M >
QueueIter_t CUtlQueue< T, M >::Last() const
{
	return m_tail;
}

template< class T, class M >
QueueIter_t CUtlQueue< T, M >::Previous( QueueIter_t it ) const
{
	if ( it == QUEUE_ITERATOR_INVALID )
	{
		return QUEUE_ITERATOR_INVALID;
	}

	if ( it == m_head )
	{
		return QUEUE_ITERATOR_INVALID;
	}

	Assert( IsValid( it ) );
	if ( !IsValid( it ) )
	{
		return QUEUE_ITERATOR_INVALID;
	}

	return Previous_Unchecked( it );
}

template< class T, class M >
QueueIter_t CUtlQueue< T, M >::Next_Unchecked( QueueIter_t it ) const
{
	return it == m_memory.Count() - 1 ? QueueIter_t( 0 ) : QueueIter_t( it + 1 );
}

template< class T, class M >
QueueIter_t CUtlQueue< T, M >::Previous_Unchecked( QueueIter_t it ) const
{
	return it == 0 ? QueueIter_t( m_memory.Count() - 1 ) : QueueIter_t( it - 1 );
}

template< class T, class M >
bool CUtlQueue< T, M >::ContainsElementAddress( const T *p ) const
{
	for ( QueueIter_t it = First(); it != QUEUE_ITERATOR_INVALID; it = Next( it ) )
	{
		if ( std::addressof( m_memory[it] ) == p )
		{
			return true;
		}
	}

	return false;
}

template< class T, class M >
bool CUtlQueue< T, M >::IsValid( QueueIter_t it ) const
{
	if ( it == QUEUE_ITERATOR_INVALID || m_head == QUEUE_ITERATOR_INVALID )
	{
		return false;
	}

	if ( m_head <= m_tail )
	{
		return it >= m_head && it <= m_tail;
	}

	return ( it >= m_head && it < m_memory.Count() ) || ( it >= 0 && it <= m_tail );
}

template< class T, class M >
T const &CUtlQueue< T, M >::Element( QueueIter_t it ) const
{
	static_assert( std::is_default_constructible< T >::value,
	               "Element() requires a default-constructible T for its invalid-iterator fallback." );

	Assert( IsValid( it ) );

	// dimhotepus: Avoid out-of-range access in release builds.
	if ( !IsValid( it ) )
	{
		static T dummy;
		return dummy;
	}

	return m_memory[it];
}

template< class T, class M >
intp CUtlQueue< T, M >::Count() const
{
	if ( m_head == QUEUE_ITERATOR_INVALID )
	{
		Assert( m_tail == QUEUE_ITERATOR_INVALID );
		return 0;
	}

	Assert( m_tail != QUEUE_ITERATOR_INVALID );

	if ( m_head <= m_tail )
	{
		return m_tail + 1 - m_head;
	}

	return m_tail + 1 - m_head + m_memory.Count();
}

template< class T, class M >
bool CUtlQueue< T, M >::IsEmpty() const
{
	Assert( ( m_head == QUEUE_ITERATOR_INVALID ) ==
	        ( m_tail == QUEUE_ITERATOR_INVALID ) );

	return m_head == QUEUE_ITERATOR_INVALID;
}

template< class T, class M >
void CUtlQueue< T, M >::RemoveAll()
{
	// dimhotepus: Destruct elements.
	QueueIter_t it = m_head;

	while ( it != QUEUE_ITERATOR_INVALID )
	{
		const QueueIter_t next =
		( it == m_tail ) ? QUEUE_ITERATOR_INVALID : Next_Unchecked( it );

		Destruct( std::addressof( m_memory[it] ) );
		it = next;
	}

	m_head = m_tail = QUEUE_ITERATOR_INVALID;
}

template< class T, class M >
void CUtlQueue< T, M >::Purge()
{
	RemoveAll();
	m_memory.Purge();
}

#ifdef TEST_UTLQUEUE

#include <cstdlib>

struct Data_t
{
	constexpr Data_t( int i = 0xffffffff ) :
		m_id( i )
	{
	}
	constexpr Data_t( const Data_t &that ) :
		m_id( that.m_id )
	{
	}
	~Data_t() { m_id = 0xdddddddd; }
	Data_t &operator=( const Data_t &that )
	{
		m_id = that.m_id;
		return *this;
	}

	int m_id;
};

inline void CUtlQueue_Test()
{
	CUtlQueue< Data_t > queue;

	for ( int n = 1; n < 100; ++n )
	{
		Assert( queue.Count() == 0 );
		Assert( queue.m_head == QUEUE_ITERATOR_INVALID );
		Assert( queue.m_tail == QUEUE_ITERATOR_INVALID );

		int w = rand() % n;
		for ( int i = 0; i < w; ++i )
		{
			queue.Insert( Data_t( i ) );
		}

		if ( w > 0 )
		{
			Assert( queue.Head().m_id == queue.First() );
			Assert( queue.Tail().m_id == queue.Last() );
			Assert( queue.Head().m_id == 0 );
			Assert( queue.Tail().m_id == w - 1 );
		}
		Assert( queue.Count() == w );

		for ( int j = 0; j < n; ++j )
		{
			queue.Insert( Data_t( w + j ) );

			if ( j == 0 )
			{
				Assert( queue.Count() == w + j + 1 );

				for ( int i = 0; i < w; ++i )
				{
					queue.RemoveAtHead();
				}
			}

			Assert( queue.Count() == j + 1 );

			Assert( queue.m_head != QUEUE_ITERATOR_INVALID );
			Assert( queue.m_tail != QUEUE_ITERATOR_INVALID );

			int id = queue.Head().m_id % queue.m_memory.Count();
			for ( QueueIter_t it = queue.First(); it != QUEUE_ITERATOR_INVALID; it = queue.Next( it ) )
			{
				Assert( queue.Element( it ).m_id % queue.m_memory.Count() == id );
				id = ( id + 1 ) % queue.m_memory.Count();
			}

			id = queue.Tail().m_id % queue.m_memory.Count();
			for ( QueueIter_t it = queue.Last(); it != QUEUE_ITERATOR_INVALID; it = queue.Previous( it ) )
			{
				Assert( queue.Element( it ).m_id % queue.m_memory.Count() == id );
				id = ( id + queue.m_memory.Count() - 1 ) % queue.m_memory.Count();
			}

			for ( int i = 0; i < j; ++i )
			{
				int id = queue.m_memory[i].m_id;
				if ( queue.IsValid( QueueIter_t( i ) ) )
				{
					Assert( ( id & 0xff000000 ) == 0 );
				}
				else
				{
					Assert( id == 0xdddddddd );
				}
			}
		}

		Assert( queue.Count() == n );
#if 0
		for ( int j = 0; j < n; ++j )
		{
			Assert( queue.m_head != QUEUE_ITERATOR_INVALID );
			Assert( queue.m_tail != QUEUE_ITERATOR_INVALID );

			Assert( queue.Count() == n - j );

			Data_t data = queue.RemoveAtHead();

			Assert( queue.Count() == n - j - 1 );

			if ( queue.Count() > 0 )
			{
				int id = queue.Head().m_id % queue.m_memory.Count();
				for ( QueueIter_t it = queue.First(); it != QUEUE_ITERATOR_INVALID; it = queue.Next( it ) )
				{
					Assert( queue.Element( it ).m_id % queue.m_memory.Count() == id );
					id = ( id + 1 ) % queue.m_memory.Count();
				}

				id = queue.Tail().m_id % queue.m_memory.Count();
				for ( QueueIter_t it = queue.Last(); it != QUEUE_ITERATOR_INVALID; it = queue.Previous( it ) )
				{
					Assert( queue.Element( it ).m_id % queue.m_memory.Count() == id );
					id = ( id + queue.m_memory.Count() - 1 ) % queue.m_memory.Count();
				}
			}

			for ( int i = 0; i < j; ++i )
			{
				int id = queue.m_memory[ i ].m_id;
				if ( queue.IsValid( QueueIter_t( i ) ) )
				{
					Assert( ( id & 0xff000000 ) == 0 );
				}
				else
				{
					Assert( id == 0xdddddddd );
				}
			}
		}
#else
		for ( int j = n - 1; j >= 0; --j )
		{
			Assert( queue.m_head != QUEUE_ITERATOR_INVALID );
			Assert( queue.m_tail != QUEUE_ITERATOR_INVALID );

			Assert( queue.Count() == j + 1 );

			Data_t data = queue.RemoveAtTail();

			Assert( queue.Count() == j );

			if ( queue.Count() > 0 )
			{
				int id = queue.Head().m_id % queue.m_memory.Count();
				for ( QueueIter_t it = queue.First(); it != QUEUE_ITERATOR_INVALID; it = queue.Next( it ) )
				{
					Assert( queue.Element( it ).m_id % queue.m_memory.Count() == id );
					id = ( id + 1 ) % queue.m_memory.Count();
				}

				id = queue.Tail().m_id % queue.m_memory.Count();
				for ( QueueIter_t it = queue.Last(); it != QUEUE_ITERATOR_INVALID; it = queue.Previous( it ) )
				{
					Assert( queue.Element( it ).m_id % queue.m_memory.Count() == id );
					id = ( id + queue.m_memory.Count() - 1 ) % queue.m_memory.Count();
				}
			}

			for ( int i = 0; i < j; ++i )
			{
				int id = queue.m_memory[i].m_id;
				if ( queue.IsValid( QueueIter_t( i ) ) )
				{
					Assert( ( id & 0xff000000 ) == 0 );
				}
				else
				{
					Assert( id == 0xdddddddd );
				}
			}
		}
#endif

		Assert( queue.Count() == 0 );
		Assert( queue.m_head == QUEUE_ITERATOR_INVALID );
		Assert( queue.m_tail == QUEUE_ITERATOR_INVALID );
	}
}

#endif // TEST_UTLQUEUE

#endif // UTLQUEUE_H
