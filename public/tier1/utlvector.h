//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
// A growable array class that maintains a free list and keeps elements
// in the same location
//=============================================================================//

#ifndef UTLVECTOR_H
#define UTLVECTOR_H

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"

#include "vstdlib/random.h"

#include "utlmemory.h"
#include "utlblockmemory.h"
#include "strtools.h"

#define FOR_EACH_VEC( vecName, iteratorName ) \
	for ( intp iteratorName = 0; (vecName).IsUtlVector && iteratorName < (vecName).Count(); iteratorName++ )
#define FOR_EACH_VEC_BACK( vecName, iteratorName ) \
	for ( intp iteratorName = (vecName).Count()-1; (vecName).IsUtlVector && iteratorName >= 0; iteratorName-- )

// UtlVector derives from this so we can do the type check above
struct base_vector_t
{
	enum { IsUtlVector = true }; // Used to match this at compiletime
};

namespace utlvector_detail
{
	// Allocator capabilities, matched to utlmemory.h:
	//
	//                              Swap()   Purge(n)   inline storage
	//   CUtlMemory                 yes      yes        no
	//   CUtlMemoryAligned          yes      Assert(0)  no
	//   CUtlMemoryFixedGrowable    unsafe*  yes        yes
	//   CUtlMemoryFixed            none     Assert(0)  yes
	//   CUtlMemoryConservative     none     yes        no
	//   CUtlBlockMemory            yes      yes        no
	//
	// * inherited CUtlMemory::Swap exchanges pointers to the inline buffers.

	// Allocators whose Swap() just exchanges owned heap pointers, so a move or
	// swap can steal the buffer. Everything else is moved element-wise.
	template < class A >
	struct IsStealableAllocator : std::false_type {};

	template < class T, class I >
	struct IsStealableAllocator< CUtlMemory< T, I > > : std::true_type {};

	template < class T, unsigned nAlignment >
	struct IsStealableAllocator< CUtlMemoryAligned< T, nAlignment > > : std::true_type {};

	template < class T, class... Rest >
	struct IsStealableAllocator< CUtlBlockMemory< T, Rest... > > : std::true_type {};

	// Allocators that implement Purge( numElements ) instead of asserting.
	template < class A >
	struct SupportsPartialPurge : std::true_type {};

	template < class T, size_t SIZE, unsigned nAlignment >
	struct SupportsPartialPurge< CUtlMemoryFixed< T, SIZE, nAlignment > > : std::false_type {};

	template < class T, unsigned nAlignment >
	struct SupportsPartialPurge< CUtlMemoryAligned< T, nAlignment > > : std::false_type {};

	// Allocators exposing IsExternallyAllocated().
	template < class A, class = void >
	struct HasExternalBuffer : std::false_type {};

	template < class A >
	struct HasExternalBuffer< A, std::void_t< decltype( std::declval< const A & >().IsExternallyAllocated() ) > > : std::true_type {};

	// In debug builds ::Destruct() poisons memory with 0xDD even for trivial types; keep that.
#ifdef _DEBUG
	inline constexpr bool kAlwaysDestruct = true;
#else
	inline constexpr bool kAlwaysDestruct = false;
#endif
}

//-----------------------------------------------------------------------------
// The CUtlVector class:
// A growable array class which doubles in size by default.
// It will always keep all elements consecutive in memory, and may move the
// elements around in memory (via a PvRealloc) when elements are inserted or
// removed. Clients should therefore refer to the elements of the vector
// by index (they should *never* maintain pointers to elements in the vector).
//
// dimhotepus: Like the rest of the Utl containers, elements are relocated with
// memcpy/memmove/realloc. T must be trivially relocatable (no self-pointers).
//-----------------------------------------------------------------------------
template< class T, class A = CUtlMemory<T> >
class CUtlVector : public base_vector_t
{
	using CAllocator = A;
public:
	using ElemType_t = T;

	using iterator = T *;
	using const_iterator = const T *;

	// dimhotepus: Everything except CUtlBlockMemory stores elements in one contiguous block.
	static constexpr bool IsContiguous = !std::is_same_v<A, CUtlBlockMemory<T, intp>>;

	// Set the growth policy and initial capacity. Count will always be zero. This is different from std::vector
	// where the constructor sets count as well as capacity.
	// growSize of zero implies the default growth pattern which is exponential.
	explicit CUtlVector( intp growSize = 0, intp initialCapacity = 0 );

	// Initialize with separately allocated buffer, setting the capacity and count.
	// The container will not be growable.
	CUtlVector( T* pMemory, intp initialCapacity, intp initialCount = 0 );

	// Can't copy this unless we explicitly do it! (see CCopyableUtlVector)
	CUtlVector( CUtlVector const& ) = delete;

	// dimhotepus: Moving is cheap: steals the heap allocation (CUtlMemory / CUtlBlockMemory)
	// or relocates the elements (inline-storage allocators). 'other' is left empty.
	CUtlVector( CUtlVector &&other ) noexcept;

	~CUtlVector();
	
	// Copy the array. Self-assignment safe.
	CUtlVector<T, A>& operator=( const CUtlVector<T, A> &other );
	// dimhotepus: Move the array. 'other' is left empty.
	CUtlVector<T, A>& operator=( CUtlVector<T, A> &&other ) noexcept;

	// element access
	T& operator[]( intp i );
	const T& operator[]( intp i ) const;
	T& Element( intp i );
	[[nodiscard]] const T& Element( intp i ) const;
	T& Head();
	[[nodiscard]] const T& Head() const;
	T& Tail();
	[[nodiscard]] const T& Tail() const;
	T& Random();
	[[nodiscard]] const T& Random() const;

	// STL compatible member functions. These allow easier use of std::sort
	// and they are forward compatible with the C++ 11 range-based for loops.
	std::conditional_t<IsContiguous, iterator, void*>
	begin()					{ return Base(); }
	[[nodiscard]] std::conditional_t<IsContiguous, const_iterator, const void*>
	begin() const			{ return Base(); }

	std::conditional_t<IsContiguous, iterator, void*>
	end()					{ return Base() + Count(); }
	[[nodiscard]] std::conditional_t<IsContiguous, const_iterator, const void*>
	end() const				{ return Base() + Count(); }

	// Gets the base address (can change when adding elements!)
	T* Base()								{ return m_Memory.Base(); }
	[[nodiscard]] const T* Base() const					{ return m_Memory.Base(); }

	// Returns the number of elements in the vector
	// SIZE IS DEPRECATED!
	[[nodiscard]] intp Count() const;
	[[deprecated]] [[nodiscard]] intp Size() const;	// don't use me!

	/// are there no elements? For compatibility with lists.
	[[nodiscard]] inline bool IsEmpty( ) const
	{
		return ( Count() == 0 );
	}

	// Is element index valid?
	[[nodiscard]] bool IsValidIndex( intp i ) const;
	static constexpr intp InvalidIndex();

	// Adds an element, uses default constructor
	intp AddToHead();
	intp AddToTail();
	T *AddToTailGetPtr();
	intp InsertBefore( intp elem );
	intp InsertAfter( intp elem );

	// Adds an element, uses copy constructor
	intp AddToHead( const T& src );
	intp AddToTail( const T& src );		// src may refer to an element of this vector
	intp InsertBefore( intp elem, const T& src );
	intp InsertAfter( intp elem, const T& src );

	// Adds an element, uses move constructor
	intp AddToHead( T&& src );
	intp AddToTail( T&& src );			// src may refer to an element of this vector
	intp InsertBefore( intp elem, T&& src );
	intp InsertAfter( intp elem, T&& src );

	// Adds multiple elements, uses default constructor
	intp AddMultipleToHead( intp num );
	intp AddMultipleToTail( intp num );
	intp AddMultipleToTail( intp num, const T *pToCopy );
	intp InsertMultipleBefore( intp elem, intp num );
	intp InsertMultipleBefore( intp elem, intp num, const T *pToCopy );
	intp InsertMultipleAfter( intp elem, intp num );

	// Calls RemoveAll() then AddMultipleToTail.
	// SetSize is a synonym for SetCount
	void SetSize( intp size );
	// SetCount deletes the previous contents of the container and sets the
	// container to have this many elements.
	// Use GetCount to retrieve the current count.
	void SetCount( intp count );
	void SetCountNonDestructively( intp count ); //sets count by adding or removing elements to tail TODO: This should probably be the default behavior for SetCount
	
	// Replaces the contents with a copy of pArray[0, size).
	void CopyArray( const T *pArray, intp size );

	// Fast swap
	void Swap( CUtlVector< T, A > &vec );
	
	// Add the specified array to the tail.
	intp AddVectorToTail( CUtlVector<T, A> const &src );
	
	// Move the specified array to the tail.
	intp AddVectorToTail( CUtlVector<T, A>&& src );

	// Finds an element (element needs operator== defined)
	[[nodiscard]] intp Find( const T& src ) const;

	// Helper to find using std::find_if with a predicate
	//   e.g. [] -> bool ( T &a ) { return a.IsTheThingIWant(); }
	//
	// Useful if your object doesn't define a ==
	template < typename F >
	[[nodiscard]] intp FindPredicate( F&& predicate ) const;

	void FillWithValue( const T& src );

	[[nodiscard]] bool HasElement( const T& src ) const;

	// Makes sure we have enough memory allocated to store a requested # of elements
	// Use NumAllocated() to retrieve the current capacity.
	void EnsureCapacity( intp num );

	// Makes sure we have at least this many elements
	// Use GetCount to retrieve the current count.
	void EnsureCount( intp num );

	// Element removal
	void FastRemove( intp elem );	// doesn't preserve order
	void Remove( intp elem );		// preserves order, shifts elements
	bool FindAndRemove( const T& src );	// removes first occurrence of src, preserves order, shifts elements
	bool FindAndFastRemove( const T& src );	// removes first occurrence of src, doesn't preserve order
	void RemoveMultiple( intp elem, intp num );	// preserves order, shifts elements
	void RemoveMultipleFromHead(intp num); // removes num elements from head
	void RemoveMultipleFromTail(intp num); // removes num elements from tail
	void RemoveAll();				// doesn't deallocate memory

	// dimhotepus: Removes every element for which predicate(elem) is true in a single O(n) pass,
	// preserving the order of the rest. Returns # removed.
	template < typename F >
	intp RemoveIf( F&& predicate );

	// Memory deallocation
	void Purge();

	// Purges the list and calls delete on each element in it.
	void PurgeAndDeleteElements();
	// Purges the list and calls delete[] on each element in it.
	void PurgeAndDeleteElementsArray();

	// Compacts the vector to the number of elements actually in use 
	void Compact();

	// Set the size by which it grows when it needs to allocate more memory.
	void SetGrowSize( intp size )			{ m_Memory.SetGrowSize( size ); }

	[[nodiscard]] intp NumAllocated() const;	// Only use this if you really know what you're doing!

	void Sort( int (__cdecl *pfnCompare)(const T *, const T *) );

	void Shuffle( IUniformRandomStream* pStream = nullptr );
	
	// Call this to quickly sort non-contiguously allocated vectors
	void InPlaceQuickSort( int (__cdecl *pfnCompare)(const T *, const T *) );
	// reverse the order of elements
	void Reverse( );

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, char *pchName );		// Validate our internal structures
#endif // DBGFLAG_VALIDATE

	/// sort using std:: and expecting a "<" function to be defined for the type
	void Sort( );

	/// sort using std:: with a predicate. e.g. [] -> bool ( T &a, T &b ) { return a < b; }
	template <class F> void SortPredicate( F &&predicate );

protected:

	// Grows the vector
	void GrowVector( intp num = 1 );

	// Shifts elements....
	void ShiftElementsRight( intp elem, intp num = 1 );
	void ShiftElementsLeft( intp elem, intp num = 1 );

	CAllocator m_Memory;
	intp m_Size;

	// For easier access to the elements through the debugger
	// it's in release builds so this can be used in libraries correctly
	T *m_pElements;

	inline void ResetDbgInfo()
	{
		m_pElements = Base();
	}

private:
	// Address of slot i, valid for any i < NumAllocated() (constructed or not).
	T* SlotPtr( intp i )
	{
		if constexpr ( IsContiguous ) return Base() + i;
		else return std::addressof( m_Memory[ i ] );
	}
	const T* SlotPtr( intp i ) const
	{
		if constexpr ( IsContiguous ) return Base() + i;
		else return std::addressof( m_Memory[ i ] );
	}

	// EnsureCapacity that is fatal on failure (external buffer, OOM) because the
	// caller is about to construct num elements. No-op if already big enough.
	void ReserveExact( intp num );

	// Grows by num and opens an uninitialized gap of num slots at elem.
	void MakeGap( intp elem, intp num )		{ GrowVector( num ); ShiftElementsRight( elem, num ); }

	// Destroys [first, last). Skipped for trivially destructible T in release.
	void DestructRange( intp first, intp last );

	// Copy-constructs src[0, num) into uninitialized slots [elem, elem + num).
	void CopyConstructRange( intp elem, const T *pSrc, intp num );

	// Does p point into our live elements? Exact for contiguous storage,
	// always false for block memory (used for asserts).
	[[nodiscard]] bool IsInStorage( const T *p ) const;
	// Conservative version of IsInStorage (true for non-empty block memory),
	// used to decide whether a defensive copy is needed.
	[[nodiscard]] bool MayAlias( const T *p ) const;

	// Takes other's elements; other is left empty. Implements move semantics.
	void MoveFrom( CUtlVector &other );

	template < class U >
	void AppendValue( U&& value );

	template < class Less >
	void QuickSortImpl( Less &less, intp nLeft, intp nRight );
};


template < class T >
using CUtlBlockVector = CUtlVector< T, CUtlBlockMemory< T, intp > >;

//-----------------------------------------------------------------------------
// The CUtlVectorMT class:
// An array class with spurious mutex protection. Nothing is actually protected
// unless you call Lock and Unlock. Also, the Mutex_t is actually not a type
// but a member which probably isn't used.
//-----------------------------------------------------------------------------

template< class BASE_UTLVECTOR, class MUTEX_TYPE = CThreadFastMutex >
class CUtlVectorMT : public BASE_UTLVECTOR, public MUTEX_TYPE
{
	using BaseClass = BASE_UTLVECTOR;
public:
	// MUTEX_TYPE Mutex_t;

	// constructor, destructor
	explicit CUtlVectorMT( intp growSize = 0, intp initSize = 0 ) : BaseClass( growSize, initSize ) {}
	CUtlVectorMT( typename BaseClass::ElemType_t* pMemory, intp numElements ) : BaseClass( pMemory, numElements ) {}
};


//-----------------------------------------------------------------------------
// The CUtlVectorFixed class:
// A array class with a fixed allocation scheme
//-----------------------------------------------------------------------------
template< class T, size_t TMaxSize >
class CUtlVectorFixed : public CUtlVector< T, CUtlMemoryFixed<T, TMaxSize > >
{
	using BaseClass = CUtlVector<T, CUtlMemoryFixed<T, TMaxSize>>;
public:

	// constructor, destructor
	explicit CUtlVectorFixed( intp growSize = 0, intp initSize = 0 ) : BaseClass( growSize, initSize ) {}
	// dimhotepus: No external-buffer constructor: CUtlMemoryFixed( T*, intp ) is deleted.
};


//-----------------------------------------------------------------------------
// The CUtlVectorFixedGrowable class:
// A array class with a fixed allocation scheme backed by a dynamic one
//-----------------------------------------------------------------------------
template< class T, size_t TMaxSize >
class CUtlVectorFixedGrowable : public CUtlVector< T, CUtlMemoryFixedGrowable<T, TMaxSize > >
{
	using BaseClass = CUtlVector<T, CUtlMemoryFixedGrowable<T, TMaxSize>>;

public:
	// constructor, destructor
	explicit CUtlVectorFixedGrowable( intp growSize = 0 ) : BaseClass( growSize, TMaxSize ) {}
};


//-----------------------------------------------------------------------------
// The CUtlVectorConservative class:
// A array class with a conservative allocation scheme
//-----------------------------------------------------------------------------
template< class T >
class CUtlVectorConservative : public CUtlVector< T, CUtlMemoryConservative<T> >
{
	using BaseClass = CUtlVector<T, CUtlMemoryConservative<T>>;
public:

	// constructor, destructor
	explicit CUtlVectorConservative( intp growSize = 0, intp initSize = 0 ) : BaseClass( growSize, initSize ) {}
	// dimhotepus: No external-buffer constructor: CUtlMemoryConservative( T*, intp ) is deleted.
};


//-----------------------------------------------------------------------------
// The CUtlVectorUltra Conservative class:
// A array class with a very conservative allocation scheme, with customizable allocator
// Especialy useful if you have a lot of vectors that are sparse, or if you're
// carefully packing holders of vectors
//-----------------------------------------------------------------------------
class CUtlVectorUltraConservativeAllocator
{
public:
	[[nodiscard]] ALLOC_CALL static void *Alloc( size_t nSize )
	{
		return malloc( nSize );
	}

	[[nodiscard]] ALLOC_CALL static void *Realloc( void *pMem, size_t nSize )
	{
		return realloc( pMem, nSize );
	}

	static void Free( void *pMem )
	{
		free( pMem );
	}

	[[nodiscard]] static size_t GetSize( void *pMem )
	{
		return mallocsize( pMem );
	}

};

template <typename T, typename A = CUtlVectorUltraConservativeAllocator >
class CUtlVectorUltraConservative : private A
{
	// dimhotepus: Elements live in the same malloc block as the header, which is only
	// guaranteed to be max_align_t aligned.
	static_assert( alignof( T ) <= alignof( std::max_align_t ),
		"CUtlVectorUltraConservative does not support over-aligned element types" );

public:
	// Don't inherit from base_vector_t because multiple-inheritance increases
	// class size!
	enum { IsUtlVector = true }; // Used to match this at compiletime

	struct Data_t
	{
		intp m_Size;
		T *m_Elements;
	};

	CUtlVectorUltraConservative() noexcept
		: m_pData( StaticData() )
	{
	}

	// dimhotepus: Copying would share m_pData and double-free it.
	CUtlVectorUltraConservative( const CUtlVectorUltraConservative & ) = delete;
	CUtlVectorUltraConservative &operator=( const CUtlVectorUltraConservative & ) = delete;

	// dimhotepus: Add move ctor.
	CUtlVectorUltraConservative( CUtlVectorUltraConservative &&other ) noexcept
		: m_pData( other.m_pData )
	{
		other.m_pData = StaticData();
	}

	// dimhotepus: Add move operator =.
	CUtlVectorUltraConservative &operator=( CUtlVectorUltraConservative &&other ) noexcept
	{
		if ( this != &other )
		{
			RemoveAll();
			m_pData = other.m_pData;
			other.m_pData = StaticData();
		}
		return *this;
	}

	~CUtlVectorUltraConservative()
	{
		RemoveAll();
	}

	[[nodiscard]] intp Count() const
	{
		return m_pData->m_Size;
	}

	static constexpr intp InvalidIndex()
	{
		return -1;
	}

	[[nodiscard]] inline bool IsValidIndex( intp i ) const
	{
		return (i >= 0) && (i < Count());
	}

	// dimhotepus: Add STL compatible member functions. 
	// STL compatible member functions. These allow easier use of std::sort
	// and they are forward compatible with the C++ 11 range-based for loops.
	T* begin()					{ return Base(); }
	[[nodiscard]] const T* begin() const		{ return Base(); }
	
	// dimhotepus: Add STL compatible member functions. 
	T *end()					{ return Base() + Count(); }
	[[nodiscard]] const T *end() const		{ return Base() + Count(); }

	// dimhotepus: Add CUtlVector compatible member functions. 
	// Gets the base address (can change when adding elements!)
	T* Base()								{ return m_pData->m_Elements; }
	[[nodiscard]] const T* Base() const					{ return m_pData->m_Elements; }

	T& operator[]( intp i )
	{
		Assert( IsValidIndex( i ) );
		return m_pData->m_Elements[i];
	}

	const T& operator[]( intp i ) const
	{
		Assert( IsValidIndex( i ) );
		return m_pData->m_Elements[i];
	}

	T& Element( intp i )
	{
		Assert( IsValidIndex( i ) );
		return m_pData->m_Elements[i];
	}

	[[nodiscard]] const T& Element( intp i ) const
	{
		Assert( IsValidIndex( i ) );
		return m_pData->m_Elements[i];
	}

	// dimhotepus: Makes sure there is room for num elements in total.
	void EnsureCapacity( intp num )
	{
		if ( num <= Count() )
		{
			return;
		}

		const size_t nNeeded = ElementsOffset() + static_cast<size_t>( num ) * sizeof( T ); //-V119

		Data_t *pNew;
		if ( m_pData == StaticData() )
		{
			pNew = static_cast<Data_t *>( A::Alloc( nNeeded ) );
			if ( !pNew )
			{
				Error( "CUtlVectorUltraConservative: out of memory allocating %zu bytes.\n", nNeeded );
				return;
			}
			pNew->m_Size = 0;
		}
		else
		{
			if ( nNeeded <= A::GetSize( m_pData ) )
			{
				return;
			}

			// On failure realloc leaves the original block intact and we keep owning it.
			pNew = static_cast<Data_t *>( A::Realloc( m_pData, nNeeded ) );
			if ( !pNew )
			{
				Error( "CUtlVectorUltraConservative: out of memory reallocating %zu bytes.\n", nNeeded );
				return;
			}
		}

		pNew->m_Elements = ElementsOf( pNew );
		m_pData = pNew;
	}

	intp AddToTail( const T& src )
	{
		// dimhotepus: src may live inside our block, which EnsureCapacity can realloc away.
		if ( IsInStorage( std::addressof( src ) ) )
		{
			T tmp( src );
			return AddToTail( std::move( tmp ) );
		}

		const intp iNew = Count();
		EnsureCapacity( iNew + 1 );
		CopyConstruct( std::addressof( m_pData->m_Elements[iNew] ), src );
		m_pData->m_Size++;
		return iNew;
	}

	intp AddToTail( T&& src )
	{
		// dimhotepus: src may live inside our block, which EnsureCapacity can realloc away.
		if ( IsInStorage( std::addressof( src ) ) )
		{
			T tmp( std::move( src ) );
			return AddToTail( std::move( tmp ) );
		}

		const intp iNew = Count();
		EnsureCapacity( iNew + 1 );
		MoveConstruct( std::addressof( m_pData->m_Elements[iNew] ), std::move( src ) );
		m_pData->m_Size++;
		return iNew;
	}

	void RemoveAll()
	{
		if ( m_pData == StaticData() )
		{
			return;
		}

		if constexpr ( !std::is_trivially_destructible_v<T> || utlvector_detail::kAlwaysDestruct )
		{
			for ( intp i = m_pData->m_Size; --i >= 0; )
			{
				// Global scope to resolve conflict with Scaleform 4.0
				::Destruct( std::addressof( m_pData->m_Elements[i] ) );
			}
		}

		A::Free( m_pData );
		m_pData = StaticData();
	}

	void PurgeAndDeleteElements()
	{
		if ( m_pData != StaticData() )
		{
			for( intp i=0; i < m_pData->m_Size; i++ )
			{
				delete Element(i);
			}
			RemoveAll();
		}
	}

	void FastRemove( intp elem )
	{
		Assert( IsValidIndex(elem) );

		// Global scope to resolve conflict with Scaleform 4.0
		::Destruct( std::addressof( Element(elem) ) );

		const intp last = m_pData->m_Size - 1;
		if ( elem != last )
		{
			memcpy( static_cast<void *>( std::addressof( m_pData->m_Elements[elem] ) ),
				static_cast<const void *>( std::addressof( m_pData->m_Elements[last] ) ), sizeof(T) );
		}

		if ( --m_pData->m_Size == 0 )
		{
			A::Free( m_pData );
			m_pData = StaticData();
		}
	}

	void Remove( intp elem )
	{
		Assert( IsValidIndex(elem) );

		// Global scope to resolve conflict with Scaleform 4.0
		::Destruct( std::addressof( Element(elem) ) );
		ShiftElementsLeft(elem);

		if ( --m_pData->m_Size == 0 )
		{
			A::Free( m_pData );
			m_pData = StaticData();
		}
	}

	[[nodiscard]] intp Find( const T& src ) const
	{
		const intp nCount = Count();
		const T *pElements = m_pData->m_Elements;
		for ( intp i = 0; i < nCount; ++i )
		{
			if (pElements[i] == src)
				return i;
		}
		return -1;
	}

	bool FindAndRemove( const T& src )
	{
		intp elem = Find( src );
		if ( elem != -1 )
		{
			Remove( elem );
			return true;
		}
		return false;
	}


	bool FindAndFastRemove( const T& src )
	{
		intp elem = Find( src );
		if ( elem != -1 )
		{
			FastRemove( elem );
			return true;
		}
		return false;
	}

	[[nodiscard]] bool DebugCompileError_ANonVectorIsUsedInThe_FOR_EACH_VEC_Macro( ) const { return true; }

	Data_t *m_pData;
private:
	// Byte offset of the first element after the header, honoring alignof(T).
	static constexpr size_t ElementsOffset()
	{
		return ( sizeof( Data_t ) + alignof( T ) - 1 ) & ~( alignof( T ) - 1 );
	}

	static T *ElementsOf( Data_t *pData )
	{
		return reinterpret_cast<T *>( reinterpret_cast<char *>( pData ) + ElementsOffset() );
	}

	[[nodiscard]] bool IsInStorage( const T *p ) const
	{
		const T *pBase = m_pData->m_Elements;
		return pBase && !std::less<const T *>()( p, pBase ) && std::less<const T *>()( p, pBase + Count() );
	}

	// dimhotepus: Caller must destruct elem first and decrement m_Size afterwards.
	void ShiftElementsLeft( intp elem, intp num = 1 )
	{
		const intp nSize = Count();
		Assert( IsValidIndex(elem) || ( nSize == 0 ) || ( num == 0 ));
		const intp numToMove = nSize - elem - num;
		if ((numToMove > 0) && (num > 0))
		{
			Q_memmove( static_cast<void *>( std::addressof( Element(elem) ) ), static_cast<const void *>( std::addressof( Element(elem+num) ) ), numToMove * sizeof(T) );

#ifdef _DEBUG
			Q_memset( static_cast<void *>( std::addressof( Element(nSize-num) ) ), 0xDD, num * sizeof(T) );
#endif
		}
	}

	// dimhotepus: Shared empty sentinel. A constant-initialized inline variable avoids the
	// thread-safe-static guard check a function-local static costs on every call.
	static inline Data_t s_StaticData{ 0, nullptr };

	static Data_t *StaticData()
	{
		Assert( s_StaticData.m_Size == 0 );
		return &s_StaticData;
	}
};

// Make sure nobody adds multiple inheritance and makes this class bigger.
COMPILE_TIME_ASSERT( sizeof(CUtlVectorUltraConservative<intp>) == sizeof(void*) );


//-----------------------------------------------------------------------------
// The CCopyableUtlVector class:
// A array class that allows copy construction (so you can nest a CUtlVector inside of another one of our containers)
//  WARNING - this class lets you copy construct which can be an expensive operation if you don't carefully control when it happens
// Only use this when nesting a CUtlVector() inside of another one of our container classes (i.e a CUtlMap)
//-----------------------------------------------------------------------------
template< class T >
class CCopyableUtlVector : public CUtlVector< T, CUtlMemory<T> >
{
	using BaseClass = CUtlVector<T, CUtlMemory<T>>;
public:
	explicit CCopyableUtlVector( intp growSize = 0, intp initSize = 0 ) : BaseClass( growSize, initSize ) {}
	CCopyableUtlVector( T* pMemory, intp numElements ) : BaseClass( pMemory, numElements ) {}
	virtual ~CCopyableUtlVector()  = default;
	CCopyableUtlVector( CCopyableUtlVector const& vec ) : BaseClass() { this->CopyArray( vec.Base(), vec.Count() ); }
	// dimhotepus: move.
	CCopyableUtlVector( CCopyableUtlVector&& vec ) noexcept : BaseClass( std::move( vec ) ) {}
	// dimhotepus: copy.
	CCopyableUtlVector& operator=( CCopyableUtlVector const& ) = default;
	// dimhotepus: move.
	CCopyableUtlVector& operator=( CCopyableUtlVector&& ) noexcept = default;
};

//-----------------------------------------------------------------------------
// The CCopyableUtlVectorFixed class:
// A array class that allows copy construction (so you can nest a CUtlVector inside of another one of our containers)
//  WARNING - this class lets you copy construct which can be an expensive operation if you don't carefully control when it happens
// Only use this when nesting a CUtlVector() inside of another one of our container classes (i.e a CUtlMap)
//-----------------------------------------------------------------------------
template< class T, size_t TMaxSize >
class CCopyableUtlVectorFixed : public CUtlVectorFixed< T, TMaxSize >
{
	using BaseClass = CUtlVectorFixed<T, TMaxSize>;
public:
	explicit CCopyableUtlVectorFixed( intp growSize = 0, intp initSize = 0 ) : BaseClass( growSize, initSize ) {}
	virtual ~CCopyableUtlVectorFixed() = default;
	CCopyableUtlVectorFixed( CCopyableUtlVectorFixed const& vec ) : BaseClass() { this->CopyArray( vec.Base(), vec.Count() ); }
	// dimhotepus: copy.
	CCopyableUtlVectorFixed& operator=( CCopyableUtlVectorFixed const& ) = default;
};

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
template< typename T, class A >
inline CUtlVector<T, A>::CUtlVector( intp growSize, intp initialCapacity )	: 
	m_Memory(growSize, initialCapacity), m_Size(0)
{
	ResetDbgInfo();
}

template< typename T, class A >
inline CUtlVector<T, A>::CUtlVector( T* pMemory, intp allocationCount, intp numElements )	: 
	m_Memory(pMemory, allocationCount), m_Size(numElements)
{
	Assert( numElements >= 0 && numElements <= allocationCount );
	ResetDbgInfo();
}

template< typename T, class A >
inline CUtlVector<T, A>::CUtlVector( CUtlVector &&other ) noexcept :
	m_Memory( intp( 0 ), intp( 0 ) ), m_Size(0), m_pElements(nullptr)
{
	ResetDbgInfo();
	MoveFrom( other );
}

template< typename T, class A >
inline CUtlVector<T, A>::~CUtlVector()
{
	Purge();
}

template< typename T, class A >
inline CUtlVector<T, A>& CUtlVector<T, A>::operator=( const CUtlVector<T, A> &other )
{
	// Previously SetSize() destroyed our elements before copying them, so
	// self-assignment wiped the vector.
	if ( this == &other )
		return *this;

	const intp nCount = other.Count();
	RemoveAll();
	ReserveExact( nCount );

	if constexpr ( IsContiguous )
	{
		CopyConstructRange( 0, other.Base(), nCount );
	}
	else
	{
		for ( intp i = 0; i < nCount; ++i )
		{
			CopyConstruct( SlotPtr( i ), other.m_Memory[ i ] );
		}
	}
	m_Size = nCount;
	return *this;
}

template< typename T, class A >
inline CUtlVector<T, A>& CUtlVector<T, A>::operator=( CUtlVector<T, A> &&other ) noexcept
{
	if ( this != &other )
	{
		MoveFrom( other );
	}
	return *this;
}

#ifdef STAGING_ONLY
inline void StagingUtlVectorBoundsCheck( intp i, intp size )
{
	if ( (size_t)i >= (size_t)size )
	{
		Msg( "Array access error: %zd / %zd\n", i, size );
		DebuggerBreak();
	}
}

#else
#define StagingUtlVectorBoundsCheck( _i, _size )
#endif

//-----------------------------------------------------------------------------
// element access
//-----------------------------------------------------------------------------
template< typename T, class A >
inline T& CUtlVector<T, A>::operator[]( intp i )
{
	// Do an inline unsigned check for maximum debug-build performance.
	Assert( (size_t)i < (size_t)m_Size );
	StagingUtlVectorBoundsCheck( i, m_Size );
	return m_Memory[ i ];
}

template< typename T, class A >
inline const T& CUtlVector<T, A>::operator[]( intp i ) const
{
	// Do an inline unsigned check for maximum debug-build performance.
	Assert( (size_t)i < (size_t)m_Size );
	StagingUtlVectorBoundsCheck( i, m_Size );
	return m_Memory[ i ];
}

template< typename T, class A >
inline T& CUtlVector<T, A>::Element( intp i )
{
	// Do an inline unsigned check for maximum debug-build performance.
	Assert( (size_t)i < (size_t)m_Size );
	StagingUtlVectorBoundsCheck( i, m_Size );
	return m_Memory[ i ];
}

template< typename T, class A >
inline const T& CUtlVector<T, A>::Element( intp i ) const
{
	// Do an inline unsigned check for maximum debug-build performance.
	Assert( (size_t)i < (size_t)m_Size );
	StagingUtlVectorBoundsCheck( i, m_Size );
	return m_Memory[ i ];
}

template< typename T, class A >
inline T& CUtlVector<T, A>::Head()
{
	Assert( m_Size > 0 );
	StagingUtlVectorBoundsCheck( 0, m_Size );
	return m_Memory[ 0 ];
}

template< typename T, class A >
inline const T& CUtlVector<T, A>::Head() const
{
	Assert( m_Size > 0 );
	StagingUtlVectorBoundsCheck( 0, m_Size );
	return m_Memory[ 0 ];
}

template< typename T, class A >
inline T& CUtlVector<T, A>::Tail()
{
	Assert( m_Size > 0 );
	StagingUtlVectorBoundsCheck( m_Size - 1, m_Size );
	return m_Memory[ m_Size - 1 ];
}

template< typename T, class A >
inline const T& CUtlVector<T, A>::Tail() const
{
	Assert( m_Size > 0 );
	StagingUtlVectorBoundsCheck( m_Size - 1, m_Size );
	return m_Memory[ m_Size - 1 ];
}

//-----------------------------------------------------------------------------
// Count
//-----------------------------------------------------------------------------
template< typename T, class A >
inline intp CUtlVector<T, A>::Size() const
{
	return m_Size;
}

template< typename T, class A >
inline T& CUtlVector<T, A>::Random()
{
	Assert( m_Size > 0 );
	return m_Memory[ RandomIntp( 0, m_Size - 1 ) ];
}

template< typename T, class A >
inline const T& CUtlVector<T, A>::Random() const
{
	Assert( m_Size > 0 );
	return m_Memory[ RandomIntp( 0, m_Size - 1 ) ];
}


//-----------------------------------------------------------------------------
// Shuffle - Knuth/Fisher-Yates
//-----------------------------------------------------------------------------
template< typename T, class A >
void CUtlVector<T, A>::Shuffle( IUniformRandomStream* pStream )
{
	for ( intp i = 0; i < m_Size - 1; i++ )
	{
		intp j = pStream ? pStream->RandomIntp( i, m_Size - 1 ) : RandomIntp( i, m_Size - 1 );
		if ( i != j )
		{
			V_swap( m_Memory[ i ], m_Memory[ j ] );
		}
	}
}

template< typename T, class A >
inline intp CUtlVector<T, A>::Count() const
{
	return m_Size;
}


//-----------------------------------------------------------------------------
// Reverse - reverse the order of elements, akin to std::reverse()
//-----------------------------------------------------------------------------
template< typename T, class A >
void CUtlVector<T, A>::Reverse( )
{
	if constexpr ( IsContiguous )
	{
		std::reverse( begin(), end() );
	}
	else
	{
		for ( intp i = 0; i < m_Size / 2; i++ )
		{
			V_swap( m_Memory[ i ], m_Memory[ m_Size - 1 - i ] );
		}
	}
}


//-----------------------------------------------------------------------------
// Is element index valid?
//-----------------------------------------------------------------------------
template< typename T, class A >
inline bool CUtlVector<T, A>::IsValidIndex( intp i ) const
{
	return (i >= 0) && (i < m_Size);
}
 

//-----------------------------------------------------------------------------
// Returns in invalid index
//-----------------------------------------------------------------------------
template< typename T, class A >
inline constexpr intp CUtlVector<T, A>::InvalidIndex()
{
	return -1;
}


//-----------------------------------------------------------------------------
// Internal helpers
//-----------------------------------------------------------------------------
template< typename T, class A >
inline void CUtlVector<T, A>::DestructRange( intp first, intp last )
{
	if constexpr ( !std::is_trivially_destructible_v<T> || utlvector_detail::kAlwaysDestruct )
	{
		for ( intp i = last; --i >= first; )
		{
			// Global scope to resolve conflict with Scaleform 4.0
			::Destruct( SlotPtr( i ) );
		}
	}
}

template< typename T, class A >
inline void CUtlVector<T, A>::CopyConstructRange( intp elem, const T *pSrc, intp num )
{
	if ( num <= 0 )
		return;

	if constexpr ( IsContiguous && std::is_trivially_copyable_v<T> )
	{
		memcpy( static_cast<void *>( SlotPtr( elem ) ), static_cast<const void *>( pSrc ), num * sizeof( T ) );
	}
	else
	{
		for ( intp i = 0; i < num; ++i )
		{
			CopyConstruct( SlotPtr( elem + i ), pSrc[ i ] );
		}
	}
}

template< typename T, class A >
inline bool CUtlVector<T, A>::IsInStorage( const T *p ) const
{
	if constexpr ( IsContiguous )
	{
		const T *pBase = Base();
		// std::less gives a total order even for unrelated pointers.
		return pBase && !std::less<const T *>()( p, pBase ) && std::less<const T *>()( p, pBase + m_Size );
	}
	else
	{
		return false;
	}
}

template< typename T, class A >
inline bool CUtlVector<T, A>::MayAlias( const T *p ) const
{
	if constexpr ( IsContiguous )
	{
		return IsInStorage( p );
	}
	else
	{
		// Can't cheaply tell for block memory; be conservative.
		return m_Size > 0;
	}
}

template< typename T, class A >
void CUtlVector<T, A>::MoveFrom( CUtlVector &other )
{
	if constexpr ( utlvector_detail::IsStealableAllocator<A>::value )
	{
		// Purge() frees our heap block, but leaves an external buffer attached.
		Purge();
		m_Memory.Swap( other.m_Memory );
		m_Size = other.m_Size;
		other.m_Size = 0;

		if constexpr ( utlvector_detail::HasExternalBuffer<A>::value )
		{
			// If we were attached to an external buffer, other now is. Detach it so
			// the moved-from vector is empty and can't outlive that buffer.
			if ( other.m_Memory.IsExternallyAllocated() )
			{
				A empty( intp( 0 ), intp( 0 ) );
				other.m_Memory.Swap( empty );
			}
		}

		ResetDbgInfo();
		other.ResetDbgInfo();
	}
	else
	{
		// Inline storage can't be stolen: relocate bitwise (the container
		// already requires trivially relocatable T).
		RemoveAll();
		const intp nCount = other.m_Size;
		ReserveExact( nCount );
		if constexpr ( IsContiguous )
		{
			if ( nCount > 0 )
				memcpy( static_cast<void *>( Base() ), static_cast<const void *>( other.Base() ), nCount * sizeof( T ) );
		}
		else
		{
			for ( intp i = 0; i < nCount; ++i )
				memcpy( static_cast<void *>( SlotPtr( i ) ), static_cast<const void *>( other.SlotPtr( i ) ), sizeof( T ) );
		}
		m_Size = nCount;
		other.m_Size = 0;
		ResetDbgInfo();
	}
}


//-----------------------------------------------------------------------------
// Grows the vector
//-----------------------------------------------------------------------------
template< typename T, class A >
void CUtlVector<T, A>::GrowVector( intp num )
{
	if (m_Size + num > m_Memory.NumAllocated())
	{
		MEM_ALLOC_CREDIT_CLASS();
		m_Memory.Grow( m_Size + num - m_Memory.NumAllocated() );

		// The allocators only Assert when they can't grow (external buffer, index
		// type overflow, realloc failure). Writing past the block in release builds
		// would be silent heap corruption, so fail hard instead.
		if ( m_Size + num > m_Memory.NumAllocated() ||
			( IsContiguous && !static_cast<const A &>( m_Memory ).Base() ) )
		{
			Error( "CUtlVector: failed to grow from %zd to %zd elements (external buffer or out of memory).\n",
				m_Size, m_Size + num );
		}
	}

	m_Size += num;
	ResetDbgInfo();
}

template< typename T, class A >
void CUtlVector<T, A>::ReserveExact( intp num )
{
	if ( num <= m_Memory.NumAllocated() )
		return;

	MEM_ALLOC_CREDIT_CLASS();
	m_Memory.EnsureCapacity( num );
	ResetDbgInfo();

	if ( num > m_Memory.NumAllocated() ||
		( IsContiguous && !static_cast<const A &>( m_Memory ).Base() ) )
	{
		Error( "CUtlVector: failed to reserve %zd elements (external buffer or out of memory).\n", num );
	}
}


//-----------------------------------------------------------------------------
// Sorts the vector
//-----------------------------------------------------------------------------
template< typename T, class A >
void CUtlVector<T, A>::Sort( int (__cdecl *pfnCompare)(const T *, const T *) )
{
	if ( Count() <= 1 )
		return;

	if constexpr ( IsContiguous )
	{
		std::sort( begin(), end(), [=](const T& a, const T&b) { return pfnCompare(&a, &b) < 0; } );
	}
	else
	{
		// Previously an untested bubble sort that sorted in *descending* order.
		InPlaceQuickSort( pfnCompare );
	}
}


//----------------------------------------------------------------------------------------------
// Index-based introsort-free quicksort for non-contiguously allocated vectors.
// Median-of-three pivot + 3-way (Dijkstra) partition so duplicates don't go quadratic.
// Recurses into the smaller side only, so stack depth is O(log n).
//----------------------------------------------------------------------------------------------
template< typename T, class A >
template< class Less >
void CUtlVector<T, A>::QuickSortImpl( Less &less, intp nLeft, intp nRight )
{
	while ( nLeft < nRight )
	{
		if ( nRight - nLeft < 16 )
		{
			// Insertion sort for small ranges.
			for ( intp i = nLeft + 1; i <= nRight; ++i )
			{
				for ( intp j = i; j > nLeft && less( m_Memory[ j ], m_Memory[ j - 1 ] ); --j )
				{
					V_swap( m_Memory[ j ], m_Memory[ j - 1 ] );
				}
			}
			return;
		}

		// Median of three, leaving the median at nLeft as the pivot.
		const intp nMid = nLeft + ( nRight - nLeft ) / 2;
		if ( less( m_Memory[ nMid ], m_Memory[ nLeft ] ) )		V_swap( m_Memory[ nMid ], m_Memory[ nLeft ] );
		if ( less( m_Memory[ nRight ], m_Memory[ nLeft ] ) )	V_swap( m_Memory[ nRight ], m_Memory[ nLeft ] );
		if ( less( m_Memory[ nRight ], m_Memory[ nMid ] ) )		V_swap( m_Memory[ nRight ], m_Memory[ nMid ] );
		V_swap( m_Memory[ nLeft ], m_Memory[ nMid ] );

		// Invariant: [nLeft, lt) < pivot, [lt, i) == pivot, (gt, nRight] > pivot.
		// m_Memory[lt] is always equal to the pivot.
		intp lt = nLeft, i = nLeft + 1, gt = nRight;
		while ( i <= gt )
		{
			if ( less( m_Memory[ i ], m_Memory[ lt ] ) )
			{
				V_swap( m_Memory[ lt ], m_Memory[ i ] );
				++lt; ++i;
			}
			else if ( less( m_Memory[ lt ], m_Memory[ i ] ) )
			{
				V_swap( m_Memory[ i ], m_Memory[ gt ] );
				--gt;
			}
			else
			{
				++i;
			}
		}

		if ( lt - nLeft < nRight - gt )
		{
			QuickSortImpl( less, nLeft, lt - 1 );
			nLeft = gt + 1;
		}
		else
		{
			QuickSortImpl( less, gt + 1, nRight );
			nRight = lt - 1;
		}
	}
}


//----------------------------------------------------------------------------------------------
// Call this to quickly sort non-contiguously allocated vectors.
//----------------------------------------------------------------------------------------------
template< typename T, class A >
void CUtlVector<T, A>::InPlaceQuickSort( int (__cdecl *pfnCompare)(const T *, const T *) )
{
	auto less = [pfnCompare]( const T &a, const T &b ) { return pfnCompare( &a, &b ) < 0; };
	QuickSortImpl( less, 0, Count() - 1 );
}

template< typename T, class A >
void CUtlVector<T, A>::Sort( )
{
	if constexpr ( IsContiguous )
	{
		std::sort( begin(), end() );
	}
	else
	{
		auto less = []( const T &a, const T &b ) { return a < b; };
		QuickSortImpl( less, 0, Count() - 1 );
	}
}

template< typename T, class A >
template <class F>
void CUtlVector<T, A>::SortPredicate( F &&predicate )
{
	if constexpr ( IsContiguous )
	{
		// std::ref avoids copying a (possibly stateful) predicate on every recursion.
		std::sort( begin(), end(), std::ref( predicate ) );
	}
	else
	{
		QuickSortImpl( predicate, 0, Count() - 1 );
	}
}

//-----------------------------------------------------------------------------
// Makes sure we have enough memory allocated to store a requested # of elements
//-----------------------------------------------------------------------------
template< typename T, class A >
void CUtlVector<T, A>::EnsureCapacity( intp num )
{
	// CUtlMemoryConservative::EnsureCapacity reallocs even when already big enough.
	if ( num <= m_Memory.NumAllocated() )
		return;

	MEM_ALLOC_CREDIT_CLASS();
	m_Memory.EnsureCapacity(num);
	ResetDbgInfo();
}


//-----------------------------------------------------------------------------
// Makes sure we have at least this many elements
//-----------------------------------------------------------------------------
template< typename T, class A >
void CUtlVector<T, A>::EnsureCount( intp num )
{
	if (Count() < num)
	{
		AddMultipleToTail( num - Count() );
	}
}


//-----------------------------------------------------------------------------
// Shifts elements
//-----------------------------------------------------------------------------
template< typename T, class A >
void CUtlVector<T, A>::ShiftElementsRight( intp elem, intp num )
{
	Assert( IsValidIndex(elem) || ( m_Size == 0 ) || ( num == 0 ));
	intp numToMove = m_Size - elem - num;
	if ((numToMove > 0) && (num > 0))
	{
		if constexpr ( IsContiguous )
		{
			Q_memmove( static_cast<void *>( std::addressof( Element(elem+num) ) ), static_cast<const void *>( std::addressof( Element(elem) ) ), numToMove * sizeof(T) );
		}
		else
		{
			// Block memory isn't contiguous: a single memmove would run off the end of a block.
			for ( intp i = m_Size - 1; i >= elem + num; --i )
				memcpy( static_cast<void *>( SlotPtr( i ) ), static_cast<const void *>( SlotPtr( i - num ) ), sizeof(T) );
		}
	}
}

template< typename T, class A >
void CUtlVector<T, A>::ShiftElementsLeft( intp elem, intp num )
{
	Assert( IsValidIndex(elem) || ( m_Size == 0 ) || ( num == 0 ));
	intp numToMove = m_Size - elem - num;
	if ((numToMove > 0) && (num > 0))
	{
		if constexpr ( IsContiguous )
		{
			Q_memmove( static_cast<void *>( std::addressof( Element(elem) ) ), static_cast<const void *>( std::addressof( Element(elem+num) ) ), numToMove * sizeof(T) );

#ifdef _DEBUG
			Q_memset( static_cast<void *>( std::addressof( Element(m_Size-num) ) ), 0xDD, num * sizeof(T) );
#endif
		}
		else
		{
			// Block memory isn't contiguous: a single memmove would run off the end of a block.
			for ( intp i = elem; i < elem + numToMove; ++i )
				memcpy( static_cast<void *>( SlotPtr( i ) ), static_cast<const void *>( SlotPtr( i + num ) ), sizeof(T) );

#ifdef _DEBUG
			for ( intp i = m_Size - num; i < m_Size; ++i )
				Q_memset( static_cast<void *>( SlotPtr( i ) ), 0xDD, sizeof(T) );
#endif
		}
	}
}


//-----------------------------------------------------------------------------
// Adds an element, uses default constructor
//-----------------------------------------------------------------------------
template< typename T, class A >
inline intp CUtlVector<T, A>::AddToHead()
{
	return InsertBefore(0);
}

template< typename T, class A >
inline intp CUtlVector<T, A>::AddToTail()
{
	GrowVector();
	Construct( SlotPtr( m_Size - 1 ) );
	return m_Size - 1;
}

template< typename T, class A >
inline T *CUtlVector<T, A>::AddToTailGetPtr()
{
	return SlotPtr( AddToTail() );
}

template< typename T, class A >
inline intp CUtlVector<T, A>::InsertAfter( intp elem )
{
	return InsertBefore( elem + 1 );
}

template< typename T, class A >
intp CUtlVector<T, A>::InsertBefore( intp elem )
{
	// Can insert at the end
	Assert( (elem == Count()) || IsValidIndex(elem) );

	MakeGap( elem, 1 );
	Construct( SlotPtr( elem ) );
	return elem;
}


//-----------------------------------------------------------------------------
// Adds an element, uses copy constructor
//-----------------------------------------------------------------------------
template< typename T, class A >
inline intp CUtlVector<T, A>::AddToHead( const T& src )
{
	// Can't insert something that's in the list... reallocation may hose us
	Assert( !IsInStorage( std::addressof( src ) ) );
	return InsertBefore( 0, src );
}

template< typename T, class A >
inline intp CUtlVector<T, A>::AddToTail( const T& src )
{
	AppendValue( src );
	return m_Size - 1;
}

template< typename T, class A >
inline intp CUtlVector<T, A>::InsertAfter( intp elem, const T& src )
{
	// Can't insert something that's in the list... reallocation may hose us
	Assert( !IsInStorage( std::addressof( src ) ) );
	return InsertBefore( elem + 1, src );
}

template< typename T, class A >
intp CUtlVector<T, A>::InsertBefore( intp elem, const T& src )
{
	// Can't insert something that's in the list... reallocation may hose us
	Assert( !IsInStorage( std::addressof( src ) ) );

	// Can insert at the end
	Assert( (elem == Count()) || IsValidIndex(elem) );

	MakeGap( elem, 1 );
	CopyConstruct( SlotPtr( elem ), src );
	return elem;
}


//-----------------------------------------------------------------------------
// Adds an element, uses move constructor
//-----------------------------------------------------------------------------
template< typename T, class A >
inline intp CUtlVector<T, A>::AddToHead( T&& src )
{
	// Can't insert something that's in the list... reallocation may hose us
	Assert( !IsInStorage( std::addressof( src ) ) );
	return InsertBefore( 0, std::move( src ) );
}

template< typename T, class A >
inline intp CUtlVector<T, A>::AddToTail( T&& src )
{
	AppendValue( std::move( src ) );
	return m_Size - 1;
}

template< typename T, class A >
inline intp CUtlVector<T, A>::InsertAfter( intp elem, T&& src )
{
	// Can't insert something that's in the list... reallocation may hose us
	Assert( !IsInStorage( std::addressof( src ) ) );
	return InsertBefore( elem + 1, std::move( src ) );
}

template< typename T, class A >
intp CUtlVector<T, A>::InsertBefore( intp elem, T&& src )
{
	// Can't insert something that's in the list... reallocation may hose us
	Assert( !IsInStorage( std::addressof( src ) ) );

	// Can insert at the end
	Assert( (elem == Count()) || IsValidIndex(elem) );

	MakeGap( elem, 1 );
	MoveConstruct( SlotPtr( elem ), std::move(src) );
	return elem;
}


//-----------------------------------------------------------------------------
// Fast tail append shared by the AddToTail overloads.
//-----------------------------------------------------------------------------
template< typename T, class A >
template< class U >
inline void CUtlVector<T, A>::AppendValue( U&& value )
{
	if ( m_Size < m_Memory.NumAllocated() )
	{
		// No reallocation and nothing shifts, so aliasing is harmless.
		::new ( static_cast<void *>( SlotPtr( m_Size ) ) ) T( std::forward<U>( value ) );
		++m_Size;
		return;
	}

	if ( MayAlias( std::addressof( value ) ) )
	{
		// value lives in the buffer we're about to reallocate.
		T tmp( std::forward<U>( value ) );
		GrowVector();
		::new ( static_cast<void *>( SlotPtr( m_Size - 1 ) ) ) T( std::move( tmp ) );
		return;
	}

	GrowVector();
	::new ( static_cast<void *>( SlotPtr( m_Size - 1 ) ) ) T( std::forward<U>( value ) );
}

//-----------------------------------------------------------------------------
// Adds multiple elements, uses default constructor
//-----------------------------------------------------------------------------
template< typename T, class A >
inline intp CUtlVector<T, A>::AddMultipleToHead( intp num )
{
	return InsertMultipleBefore( 0, num );
}

template< typename T, class A >
inline intp CUtlVector<T, A>::AddMultipleToTail( intp num )
{
	return InsertMultipleBefore( m_Size, num );
}

template< typename T, class A >
inline intp CUtlVector<T, A>::AddMultipleToTail( intp num, const T *pToCopy )
{
	// Can't insert something that's in the list... reallocation may hose us
	Assert( (Base() == NULL) || !pToCopy || (pToCopy + num <= Base()) || (pToCopy >= (Base() + Count()) ) ); 

	return InsertMultipleBefore( m_Size, num, pToCopy );
}

template< typename T, class A >
intp CUtlVector<T, A>::InsertMultipleAfter( intp elem, intp num )
{
	return InsertMultipleBefore( elem + 1, num );
}


template< typename T, class A >
void CUtlVector<T, A>::SetCount( intp count )
{
	RemoveAll();
	AddMultipleToTail( count );
}

template< typename T, class A >
inline void CUtlVector<T, A>::SetSize( intp size )
{
	SetCount( size );
}

template< typename T, class A >
void CUtlVector<T, A>::SetCountNonDestructively( intp count )
{
	intp delta = count - m_Size;
	if(delta > 0) AddMultipleToTail( delta );
	else if(delta < 0) RemoveMultipleFromTail( -delta );
}

template< typename T, class A >
void CUtlVector<T, A>::CopyArray( const T *pArray, intp size )
{
	// Can't insert something that's in the list... reallocation may hose us
	// dimhotepus: Fix bug in assert allowing nullptr pArray to bypass.
	Assert( size == 0 || Base() == nullptr ||
		( pArray && 
			( Base() >= ( pArray + size ) || pArray >= ( Base() + Count() ) ) ) );

	// Copy-construct directly instead of default-constructing then assigning.
	RemoveAll();
	ReserveExact( size );
	CopyConstructRange( 0, pArray, size );
	m_Size = size;
}

template< typename T, class A >
void CUtlVector<T, A>::Swap( CUtlVector< T, A > &vec )
{
	if ( this == &vec )
		return;

	if constexpr ( utlvector_detail::IsStealableAllocator<A>::value )
	{
		m_Memory.Swap( vec.m_Memory );
		V_swap( m_Size, vec.m_Size );
		ResetDbgInfo();
		vec.ResetDbgInfo();
	}
	else
	{
		// CUtlMemoryFixed / CUtlMemoryConservative have no Swap(), and the Swap()
		// CUtlMemoryFixedGrowable inherits would exchange pointers to each other's
		// inline buffers. Relocate the elements through a temporary instead: O(n).
		CUtlVector tmp( std::move( vec ) );
		vec.MoveFrom( *this );
		MoveFrom( tmp );
	}
}

template< typename T, class A >
intp CUtlVector<T, A>::AddVectorToTail( CUtlVector const &src )
{
	Assert( &src != this );

	const intp base = Count();
	const intp nSrcCount = src.Count();
	if ( nSrcCount <= 0 )
		return base;

	// GrowVector keeps geometric growth; EnsureCapacity would allocate exactly
	// and make repeated appends O(n^2).
	GrowVector( nSrcCount );

	if constexpr ( IsContiguous )
	{
		CopyConstructRange( base, src.Base(), nSrcCount );
	}
	else
	{
		for ( intp i = 0; i < nSrcCount; i++ )
		{
			CopyConstruct( SlotPtr( base + i ), src[i] );
		}
	}
	return base;
}

template< typename T, class A >
intp CUtlVector<T, A>::AddVectorToTail( CUtlVector&& src )
{
	Assert( &src != this );

	const intp base = Count();
	const intp nSrcCount = src.Count();
	if ( nSrcCount <= 0 )
		return base;

	GrowVector( nSrcCount );

	if constexpr ( IsContiguous && std::is_trivially_copyable_v<T> )
	{
		memcpy( static_cast<void *>( SlotPtr( base ) ), static_cast<const void *>( src.Base() ), nSrcCount * sizeof( T ) );
	}
	else
	{
		for ( intp i = 0; i < nSrcCount; i++ )
		{
			MoveConstruct( SlotPtr( base + i ), std::move( src[i] ) );
		}
	}
	return base;
}

template< typename T, class A >
inline intp CUtlVector<T, A>::InsertMultipleBefore( intp elem, intp num )
{
	if( num == 0 )
		return elem;

	// Can insert at the end
	Assert( (elem == Count()) || IsValidIndex(elem) );

	MakeGap( elem, num );

	// Invoke default constructors
	for (intp i = 0; i < num; ++i )
	{
		Construct( SlotPtr( elem + i ) );
	}

	return elem;
}

template< typename T, class A >
inline intp CUtlVector<T, A>::InsertMultipleBefore( intp elem, intp num, const T *pToInsert )
{
	if( num == 0 )
		return elem;
	
	// Can insert at the end
	Assert( (elem == Count()) || IsValidIndex(elem) );

	MakeGap( elem, num );

	if ( !pToInsert )
	{
		// Invoke default constructors
		for ( intp i = 0; i < num; ++i )
		{
			Construct( SlotPtr( elem + i ) );
		}
	}
	else
	{
		CopyConstructRange( elem, pToInsert, num );
	}

	return elem;
}


//-----------------------------------------------------------------------------
// Finds an element (element needs operator== defined)
//-----------------------------------------------------------------------------
template< typename T, class A >
intp CUtlVector<T, A>::Find( const T& src ) const
{
	// Index loop: works for block memory too (where begin() is void*).
	if constexpr ( IsContiguous )
	{
		const T *pElements = Base();
		for ( intp i = 0; i < m_Size; ++i )
		{
			if ( pElements[ i ] == src )
				return i;
		}
	}
	else
	{
		for ( intp i = 0; i < m_Size; ++i )
		{
			if ( m_Memory[ i ] == src )
				return i;
		}
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Finds an element using a predicate, using std::find_if
//-----------------------------------------------------------------------------
template< typename T, class A >
template< class F >
intp CUtlVector<T, A>::FindPredicate( F &&predicate ) const
{
	if constexpr ( IsContiguous )
	{
		const T *pBegin = Base();
		const T *pEnd = pBegin + Count();
		const T *pFound = std::find_if( pBegin, pEnd, std::ref( predicate ) );

		if ( pFound != pEnd )
		{
			intp idx = (intp)( pFound - pBegin );
			StagingUtlVectorBoundsCheck( idx, m_Size );
			return idx;
		}
	}
	else
	{
		// Base() is null for block memory; the old find_if never matched.
		for ( intp i = 0; i < m_Size; ++i )
		{
			if ( predicate( m_Memory[ i ] ) )
				return i;
		}
	}

	return InvalidIndex();
}

template< typename T, class A >
void CUtlVector<T, A>::FillWithValue( const T& src )
{
	for ( intp i = 0; i < m_Size; ++i )
	{
		m_Memory[ i ] = src;
	}
}

template< typename T, class A >
bool CUtlVector<T, A>::HasElement( const T& src ) const
{
	return ( Find(src) >= 0 );
}


//-----------------------------------------------------------------------------
// Element removal
//-----------------------------------------------------------------------------
template< typename T, class A >
void CUtlVector<T, A>::FastRemove( intp elem )
{
	Assert( IsValidIndex(elem) );

	// Global scope to resolve conflict with Scaleform 4.0
	::Destruct( SlotPtr( elem ) );
	if ( elem != m_Size - 1 )
	{
		memcpy( static_cast<void *>( SlotPtr( elem ) ), static_cast<const void *>( SlotPtr( m_Size - 1 ) ), sizeof(T) );
	}
	--m_Size;
}

template< typename T, class A >
void CUtlVector<T, A>::Remove( intp elem )
{
	Assert( IsValidIndex(elem) );

	// Global scope to resolve conflict with Scaleform 4.0
	::Destruct( SlotPtr( elem ) );
	ShiftElementsLeft(elem);
	--m_Size;
}

template< typename T, class A >
bool CUtlVector<T, A>::FindAndRemove( const T& src )
{
	intp elem = Find( src );
	if ( elem != -1 )
	{
		Remove( elem );
		return true;
	}
	return false;
}

template< typename T, class A >
bool CUtlVector<T, A>::FindAndFastRemove( const T& src )
{
	intp elem = Find( src );
	if ( elem != -1 )
	{
		FastRemove( elem );
		return true;
	}
	return false;
}

template< typename T, class A >
void CUtlVector<T, A>::RemoveMultiple( intp elem, intp num )
{
	Assert( elem >= 0 && num >= 0 );
	Assert( elem + num <= Count() );

	if ( num <= 0 )
		return;

	DestructRange( elem, elem + num );
	ShiftElementsLeft(elem, num);
	m_Size -= num;
}

template< typename T, class A >
void CUtlVector<T, A>::RemoveMultipleFromHead( intp num )
{
	RemoveMultiple( 0, num );
}

template< typename T, class A >
void CUtlVector<T, A>::RemoveMultipleFromTail( intp num )
{
	Assert( num >= 0 && num <= Count() );

	DestructRange( m_Size - num, m_Size );
	m_Size -= num;
}

template< typename T, class A >
void CUtlVector<T, A>::RemoveAll()
{
	DestructRange( 0, m_Size );
	m_Size = 0;
}

template< typename T, class A >
template< typename F >
intp CUtlVector<T, A>::RemoveIf( F&& predicate )
{
	intp nOut = 0;
	for ( intp i = 0; i < m_Size; ++i )
	{
		T *pElem = SlotPtr( i );
		if ( predicate( *pElem ) )
		{
			::Destruct( pElem );
		}
		else
		{
			if ( nOut != i )
			{
				// Bitwise relocate into the (already destroyed) hole.
				memcpy( static_cast<void *>( SlotPtr( nOut ) ), static_cast<const void *>( pElem ), sizeof( T ) );
			}
			++nOut;
		}
	}

	const intp nRemoved = m_Size - nOut;
#ifdef _DEBUG
	for ( intp i = nOut; i < m_Size; ++i )
	{
		Q_memset( static_cast<void *>( SlotPtr( i ) ), 0xDD, sizeof( T ) );
	}
#endif
	m_Size = nOut;
	return nRemoved;
}


//-----------------------------------------------------------------------------
// Memory deallocation
//-----------------------------------------------------------------------------

template< typename T, class A >
inline void CUtlVector<T, A>::Purge()
{
	RemoveAll();
	m_Memory.Purge();
	ResetDbgInfo();
}


template< typename T, class A >
inline void CUtlVector<T, A>::PurgeAndDeleteElements()
{
	for ( intp i = 0; i < m_Size; ++i )
	{
		delete m_Memory[ i ];
	}
	Purge();
}


template< typename T, class A >
inline void CUtlVector<T, A>::PurgeAndDeleteElementsArray()
{
	for ( intp i = 0; i < m_Size; ++i )
	{
		delete[] m_Memory[ i ];
	}
	Purge();
}

template< typename T, class A >
inline void CUtlVector<T, A>::Compact()
{
	if ( m_Size == 0 )
	{
		// Avoids CUtlMemoryConservative's realloc( p, 0 ) and works for every allocator.
		m_Memory.Purge();
	}
	else if constexpr ( utlvector_detail::SupportsPartialPurge<A>::value )
	{
		m_Memory.Purge( m_Size );
	}
	// else: CUtlMemoryFixed / CUtlMemoryAligned assert in Purge( n ); nothing to do.

	// Purge may realloc; keep the debugger pointer in sync.
	ResetDbgInfo();
}

template< typename T, class A >
inline intp CUtlVector<T, A>::NumAllocated() const
{
	return m_Memory.NumAllocated();
}


//-----------------------------------------------------------------------------
// Data and memory validation
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
template< typename T, class A >
void CUtlVector<T, A>::Validate( CValidator &validator, char *pchName )
{
	validator.Push( typeid(*this).name(), this, pchName );

	m_Memory.Validate( validator, "m_Memory" );

	validator.Pop();
}
#endif // DBGFLAG_VALIDATE

// A vector class for storing pointers, so that the elements pointed to by the pointers are deleted
// on exit.
template<class T> class CUtlVectorAutoPurge : public CUtlVector< std::enable_if_t<std::is_pointer_v<T>, T>, CUtlMemory< T, intp> >
{
public:
	~CUtlVectorAutoPurge( )
	{
		this->PurgeAndDeleteElements();
	}

};

// A vector class for storing pointers, so that the elements pointed to by the pointers are deleted
// on exit.
template<class T> class CUtlVectorAutoPurgeArray : public CUtlVector< std::enable_if_t<std::is_pointer_v<T>, T>, CUtlMemory< T, intp> >
{
public:
	~CUtlVectorAutoPurgeArray( )
	{
		this->PurgeAndDeleteElementsArray();
	}

};

// easy string list class with dynamically allocated strings. For use with V_SplitString, etc.
// Frees the dynamic strings in destructor.
class CUtlStringList : public CUtlVectorAutoPurgeArray< char *>
{
public:
	void CopyAndAddToTail( char const *pString )			// clone the string and add to the end
	{
		AddToTail( V_strdup( pString ) );
	}

	static int __cdecl SortFunc( char * const * sz1, char * const * sz2 )
	{
		return strcmp( *sz1, *sz2 );
	}

	CUtlStringList() = default;

	CUtlStringList( char const *pString, char const *pSeparator )
	{
		SplitString( pString, pSeparator );
	}

	CUtlStringList( char const *pString, const char **pSeparators, intp nSeparators )
	{
		SplitString2( pString, pSeparators, nSeparators );
	}

	void SplitString( char const *pString, char const *pSeparator )
	{
		V_SplitString( pString, pSeparator, *this );
	}

	void SplitString2( char const *pString, const char **pSeparators, intp nSeparators )
	{
		V_SplitString2( pString, pSeparators, nSeparators, *this );
	}

	template<intp separatorsSize>
	void SplitString2( char const *pString, const char * (&pSeparators)[separatorsSize] )
	{
		SplitString2( pString, pSeparators, separatorsSize );
	}

	CUtlStringList( const CUtlStringList &other ) = delete; // copying directly will cause double-release of the same strings; maybe we need to do a deep copy, but unless and until such need arises, this will guard against double-release
};



// <Sergiy> placing it here a few days before Cert to minimize disruption to the rest of codebase
class CSplitString: public CUtlVector<char*, CUtlMemory<char*, intp> >
{
public:
	CSplitString(const char *pString, const char *pSeparator);
	CSplitString(const char *pString, const char **pSeparators, intp nSeparators);
	~CSplitString();
	//
	// NOTE: If you want to make Construct() public and implement Purge() here, you'll have to free m_szBuffer there
	//
private:
	void Construct(const char *pString, const char **pSeparators, intp nSeparators);
	void PurgeAndDeleteElements();
private:
	char *m_szBuffer; // a copy of original string, with '\0' instead of separators
};


#endif // UTLVECTOR_H
