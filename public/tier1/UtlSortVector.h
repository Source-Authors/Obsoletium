//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// $Header: $
// $NoKeywords: $
//
// A growable array class that keeps all elements in order using binary search
//===========================================================================//

#ifndef UTLSORTVECTOR_H
#define UTLSORTVECTOR_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// class CUtlSortVector:
// description:
//   This in an sorted order-preserving vector. Items may be inserted or removed
//   at any point in the vector. When an item is inserted, all elements are
//   moved down by one element using memmove. When an item is removed, all 
//   elements are shifted back down. Items are searched for in the vector
//   using a binary search technique. Clients must pass in a Less() function
//   into the constructor of the vector to determine the sort order.
//-----------------------------------------------------------------------------

template <class T>
class CUtlSortVectorDefaultLess
{
public:
	bool Less( const T& lhs, const T& rhs, void * )
	{
		return lhs < rhs;
	}
};

template <class T, class LessFunc = CUtlSortVectorDefaultLess<T>, class BaseVector = CUtlVector<T> >
class CUtlSortVector : public BaseVector
{
	typedef BaseVector BaseClass;
public:
	/// constructor
	CUtlSortVector( intp nGrowSize = 0, intp initSize = 0 );
	CUtlSortVector( T* pMemory, intp numElements );
	CUtlSortVector& operator=( const CUtlSortVector<T, LessFunc> &v )
	{
		BaseVector::operator=(v);
		return *this;
	}
	
	/// inserts (copy constructs) an element in sorted order into the list
	intp		Insert( const T& src );
	
	/// inserts (move constructs) an element in sorted order into the list
	intp		Insert( T&& src );
	
	/// inserts (copy constructs) an element in sorted order into the list if it isn't already in the list
	intp		InsertIfNotFound( const T& src );

	/// inserts (move constructs) an element in sorted order into the list if it isn't already in the list
	intp		InsertIfNotFound( T&& src );

	/// Finds an element within the list using a binary search. These are templatized based upon the key
	/// in which case the less function must handle the Less function for key, T and T, key
	template< typename TKey >
	intp		Find( const TKey& search ) const;
	template< typename TKey >
	intp		FindLessOrEqual( const TKey& search ) const;
	template< typename TKey >
	intp		FindLess( const TKey& search ) const;
	
	/// Removes a particular element
	void	Remove( const T& search );
	void	Remove( intp i );
	
	/// Allows methods to set a context to be used with the less function..
	void	SetLessContext( void *pCtx );

	/// A version of insertion that will produce an un-ordered list.
	/// Note that you can only use this index until sorting is redone with RedoSort!!!
	intp	InsertNoSort( const T& src );
	
	/// A version of insertion that will produce an un-ordered list.
	/// Note that you can only use this index until sorting is redone with RedoSort!!!
	intp	InsertNoSort( T&& src );

	void	RedoSort( bool bForceSort = false );

	/// Use this to insert at a specific insertion point; using FindLessOrEqual
	/// is required for use this this. This will test that what you've inserted
	/// produces a correctly ordered list.
	intp	InsertAfter( intp nElemIndex, const T &src );

	/// Use this to insert at a specific insertion point; using FindLessOrEqual
	/// is required for use this this. This will test that what you've inserted
	/// produces a correctly ordered list.
	intp	InsertAfter( intp nElemIndex, T&& src );

	/// finds a particular element using a linear search. Useful when used
	/// in between calls to InsertNoSort and RedoSort
	template< typename TKey >
	intp	FindUnsorted( const TKey &src ) const;

protected:
	// No copy constructor
	CUtlSortVector( const CUtlSortVector<T, LessFunc> & ) = delete;

	// never call these; illegal for this class
	intp AddToHead() = delete;
	intp AddToTail() = delete;
	intp InsertBefore( intp elem ) = delete;
	intp InsertAfter( intp elem ) = delete;
	
	intp AddToHead( const T& src ) = delete;
	intp AddToTail( const T& src ) = delete;
	intp InsertBefore( intp elem, const T& src ) = delete;

	intp AddToHead( T&& src ) = delete;
	intp AddToTail( T&& src ) = delete;
	intp InsertBefore( intp elem, T&& src ) = delete;

	intp AddMultipleToHead( intp num ) = delete;
	intp AddMultipleToTail( intp num, const T *pToCopy=nullptr ) = delete;
	intp InsertMultipleBefore( intp elem, intp num, const T *pToCopy=nullptr ) = delete;
	intp InsertMultipleAfter( intp elem, intp num ) = delete;
	intp AddVectorToTail( CUtlVector<T> const &src ) = delete;

	struct QSortContext_t
	{
		void		*m_pLessContext;
		LessFunc	*m_pLessFunc;
	};

	// dimhotepus: pointers to references for performance.
	static int CompareHelper( const QSortContext_t &ctx, const T &lhs, const T &rhs )
	{
		if ( ctx.m_pLessFunc->Less( lhs, rhs, ctx.m_pLessContext ) )
			return -1;
		if ( ctx.m_pLessFunc->Less( rhs, lhs, ctx.m_pLessContext ) )
			return 1;
		return 0;
	}

	void *m_pLessContext;
	bool	m_bNeedsSort;

private:
	template< typename TKey >
	intp	FindLessOrEqual( const TKey& search, bool *pFound ) const;

	void QuickSort( LessFunc& less, intp X, intp I );
};


//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
template <class T, class LessFunc, class BaseVector> 
CUtlSortVector<T, LessFunc, BaseVector>::CUtlSortVector( intp nGrowSize, intp initSize ) : 
	BaseVector( nGrowSize, initSize ), m_pLessContext(NULL), m_bNeedsSort( false )
{
}

template <class T, class LessFunc, class BaseVector> 
CUtlSortVector<T, LessFunc, BaseVector>::CUtlSortVector( T* pMemory, intp numElements ) :
	BaseVector( pMemory, numElements ), m_pLessContext(NULL), m_bNeedsSort( false )
{
}

//-----------------------------------------------------------------------------
// Allows methods to set a context to be used with the less function..
//-----------------------------------------------------------------------------
template <class T, class LessFunc, class BaseVector> 
void CUtlSortVector<T, LessFunc, BaseVector>::SetLessContext( void *pCtx )
{
	m_pLessContext = pCtx;
}

//-----------------------------------------------------------------------------
// grows the vector
//-----------------------------------------------------------------------------
template <class T, class LessFunc, class BaseVector> 
intp CUtlSortVector<T, LessFunc, BaseVector>::Insert( const T& src )
{
	AssertFatal( !m_bNeedsSort );

	intp pos = FindLessOrEqual( src ) + 1;
	this->GrowVector();
	this->ShiftElementsRight(pos);
	CopyConstruct<T>( std::addressof( this->Element(pos) ), src );
	return pos;
}

template <class T, class LessFunc, class BaseVector> 
intp CUtlSortVector<T, LessFunc, BaseVector>::Insert( T&& src )
{
	AssertFatal( !m_bNeedsSort );

	intp pos = FindLessOrEqual( src ) + 1;
	this->GrowVector();
	this->ShiftElementsRight(pos);
	MoveConstruct<T>( std::addressof( this->Element(pos) ), std::move( src ) );
	return pos;
}

template <class T, class LessFunc, class BaseVector> 
intp CUtlSortVector<T, LessFunc, BaseVector>::InsertNoSort( const T& src )
{
	m_bNeedsSort = true;
	intp lastElement = BaseVector::m_Size;
	// Just stick the new element at the end of the vector, but don't do a sort
	this->GrowVector();
	this->ShiftElementsRight(lastElement);
	CopyConstruct( std::addressof( this->Element(lastElement) ), src );
	return lastElement;
}

template <class T, class LessFunc, class BaseVector> 
intp CUtlSortVector<T, LessFunc, BaseVector>::InsertNoSort( T&& src )
{
	m_bNeedsSort = true;
	intp lastElement = BaseVector::m_Size;
	// Just stick the new element at the end of the vector, but don't do a sort
	this->GrowVector();
	this->ShiftElementsRight(lastElement);
	MoveConstruct( std::addressof( this->Element(lastElement) ), std::move( src ) );
	return lastElement;
}

/// inserts (copy constructs) an element in sorted order into the list if it isn't already in the list
template <class T, class LessFunc, class BaseVector> 
intp CUtlSortVector<T, LessFunc, BaseVector>::InsertIfNotFound( const T& src )
{
	AssertFatal( !m_bNeedsSort );
	bool bFound;
	intp pos = FindLessOrEqual( src, &bFound );
	if ( bFound )
		return pos;

	++pos;
	this->GrowVector();
	this->ShiftElementsRight(pos);
	CopyConstruct<T>( std::addressof( this->Element(pos) ), src );
	return pos;
}

/// inserts (move constructs) an element in sorted order into the list if it isn't already in the list
template <class T, class LessFunc, class BaseVector> 
intp CUtlSortVector<T, LessFunc, BaseVector>::InsertIfNotFound( T&& src )
{
	AssertFatal( !m_bNeedsSort );
	bool bFound;
	intp pos = FindLessOrEqual( src, &bFound );
	if ( bFound )
		return pos;

	++pos;
	this->GrowVector();
	this->ShiftElementsRight(pos);
	MoveConstruct<T>( std::addressof( this->Element(pos) ), std::move( src ) );
	return pos;
}

template <class T, class LessFunc, class BaseVector> 
intp CUtlSortVector<T, LessFunc, BaseVector>::InsertAfter( intp nIndex, const T &src )
{
	intp nInsertedIndex = this->BaseClass::InsertAfter( nIndex, src );

#ifdef DEBUG
	LessFunc less;
	if ( nInsertedIndex > 0 )
	{
		Assert( less.Less( this->Element(nInsertedIndex-1), src, m_pLessContext ) );
	}
	if ( nInsertedIndex < BaseClass::Count()-1 )
	{
		Assert( less.Less( src, this->Element(nInsertedIndex+1), m_pLessContext ) );
	}
#endif
	return nInsertedIndex;
}


template <class T, class LessFunc, class BaseVector> 
intp CUtlSortVector<T, LessFunc, BaseVector>::InsertAfter( intp nIndex, T &&src )
{
#ifdef DEBUG
	// In DEBUG mode we copy, not move as we check invariant using src later.
	intp nInsertedIndex = this->BaseClass::InsertAfter( nIndex, src );

	LessFunc less;
	if ( nInsertedIndex > 0 )
	{
		Assert( less.Less( this->Element(nInsertedIndex-1), src, m_pLessContext ) );
	}
	if ( nInsertedIndex < BaseClass::Count()-1 )
	{
		Assert( less.Less( src, this->Element(nInsertedIndex+1), m_pLessContext ) );
	}
#else
	intp nInsertedIndex = this->BaseClass::InsertAfter( nIndex, std::move( src ) );
#endif

	return nInsertedIndex;
}


template <class T, class LessFunc, class BaseVector> 
void CUtlSortVector<T, LessFunc, BaseVector>::QuickSort( LessFunc& less, intp nLower, intp nUpper )
{
	if ( this->Count() > 1 )
	{
		QSortContext_t ctx;
		ctx.m_pLessContext = m_pLessContext;
		ctx.m_pLessFunc = &less;

		// dimhotepus: qsort -> std::sort
		std::sort( this->Base(), this->Base() + this->Count(), [&](const T &a1, const T &a2) {
			return CUtlSortVector<T, LessFunc>::CompareHelper(ctx, a1, a2) < 0;
		});
	}
}

template <class T, class LessFunc, class BaseVector> 
void CUtlSortVector<T, LessFunc, BaseVector>::RedoSort( bool bForceSort /*= false */ )
{
	if ( !m_bNeedsSort && !bForceSort )
		return;

	m_bNeedsSort = false;
	LessFunc less;
	QuickSort( less, 0, this->Count() - 1 );
}

//-----------------------------------------------------------------------------
// finds a particular element
//-----------------------------------------------------------------------------
template <class T, class LessFunc, class BaseVector> 
template < typename TKey >
intp CUtlSortVector<T, LessFunc, BaseVector>::Find( const TKey& src ) const
{
	AssertFatal( !m_bNeedsSort );

	LessFunc less;

	intp start = 0, end = this->Count() - 1;
	while (start <= end)
	{
		intp mid = (start + end) >> 1;
		if ( less.Less( this->Element(mid), src, m_pLessContext ) )
		{
			start = mid + 1;
		}
		else if ( less.Less( src, this->Element(mid), m_pLessContext ) )
		{
			end = mid - 1;
		}
		else
		{
			return mid;
		}
	}
	return -1;
}


//-----------------------------------------------------------------------------
// finds a particular element using a linear search. Useful when used
// in between calls to InsertNoSort and RedoSort
//-----------------------------------------------------------------------------
template< class T, class LessFunc, class BaseVector > 
template < typename TKey >
intp CUtlSortVector<T, LessFunc, BaseVector>::FindUnsorted( const TKey &src ) const
{
	LessFunc less;
	intp nCount = this->Count();
	for ( intp i = 0; i < nCount; ++i )
	{
		if ( less.Less( this->Element(i), src, m_pLessContext ) )
			continue;
		if ( less.Less( src, this->Element(i), m_pLessContext ) )
			continue;
		return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// finds a particular element
//-----------------------------------------------------------------------------
template <class T, class LessFunc, class BaseVector> 
template < typename TKey >
intp CUtlSortVector<T, LessFunc, BaseVector>::FindLessOrEqual( const TKey& src, bool *pFound ) const
{
	AssertFatal( !m_bNeedsSort );

	LessFunc less;
	intp start = 0, end = this->Count() - 1;
	while (start <= end)
	{
		intp mid = (start + end) >> 1;
		if ( less.Less( this->Element(mid), src, m_pLessContext ) )
		{
			start = mid + 1;
		}
		else if ( less.Less( src, this->Element(mid), m_pLessContext ) )
		{
			end = mid - 1;
		}
		else
		{
			*pFound = true;
			return mid;
		}
	}

	*pFound = false;
	return end;
}

template <class T, class LessFunc, class BaseVector> 
template < typename TKey >
intp CUtlSortVector<T, LessFunc, BaseVector>::FindLessOrEqual( const TKey& src ) const
{
	bool bFound;
	return FindLessOrEqual( src, &bFound );
}

template <class T, class LessFunc, class BaseVector> 
template < typename TKey >
intp CUtlSortVector<T, LessFunc, BaseVector>::FindLess( const TKey& src ) const
{
	AssertFatal( !m_bNeedsSort );

	LessFunc less;
	intp start = 0, end = this->Count() - 1;
	while (start <= end)
	{
		intp mid = (start + end) >> 1;
		if ( less.Less( this->Element(mid), src, m_pLessContext ) )
		{
			start = mid + 1;
		}
		else
		{
			end = mid - 1;
		}
	}
	return end;
}


//-----------------------------------------------------------------------------
// Removes a particular element
//-----------------------------------------------------------------------------
template <class T, class LessFunc, class BaseVector> 
void CUtlSortVector<T, LessFunc, BaseVector>::Remove( const T& search )
{
	AssertFatal( !m_bNeedsSort );

	intp pos = Find(search);
	if (pos != -1)
	{
		BaseVector::Remove(pos);
	}
}

template <class T, class LessFunc, class BaseVector> 
void CUtlSortVector<T, LessFunc, BaseVector>::Remove( intp i )
{
	BaseVector::Remove( i );
}

#endif // UTLSORTVECTOR_H
