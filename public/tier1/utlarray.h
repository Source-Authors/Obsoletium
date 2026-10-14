//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
// A growable array class that maintains a free list and keeps elements
// in the same location
//=============================================================================//

#ifndef UTLARRAY_H
#define UTLARRAY_H

#include "tier0/dbg.h"
#include "vstdlib/random.h"

#include <algorithm>  // std::sort
#include <type_traits>

#define FOR_EACH_ARRAY( vecName, iteratorName ) \
	for ( intp iteratorName = 0; (vecName).IsUtlArray && iteratorName < (vecName).Count(); ++iteratorName )
#define FOR_EACH_ARRAY_BACK( vecName, iteratorName ) \
	for ( intp iteratorName = (vecName).Count() - 1; (vecName).IsUtlArray && iteratorName >= 0; --iteratorName )

struct base_array_t
{
	static constexpr bool IsUtlArray = true;
};


template< class T, intp MAX_SIZE >
class CUtlArray : public base_array_t
{
	static_assert(MAX_SIZE > 0, "CUtlArray requires MAX_SIZE > 0");

public:
	using ElemType_t = T;
	using iterator = T *;
	using const_iterator = const T *;

	CUtlArray()
		: m_Memory{}
	{
	}

	CUtlArray( T *pMemory, intp count )
		: CUtlArray{}
	{
		CopyArray(pMemory, count);
	}

	~CUtlArray() = default;

	CUtlArray &operator=( const CUtlArray &other )
	{
		if ( this != &other )
		{
			for ( intp i = 0; i < MAX_SIZE; ++i )
				m_Memory[i] = other.m_Memory[i];
		}
		return *this;
	}

	CUtlArray( const CUtlArray &other )
		: CUtlArray{}
	{
		for ( intp i = 0; i < MAX_SIZE; ++i )
			m_Memory[i] = other.m_Memory[i];
	}

	[[nodiscard]] T &operator[]( intp i )
	{
		Assert( IsValidIndex( i ) );
		return m_Memory[i];
	}

	[[nodiscard]] const T &operator[]( intp i ) const
	{
		Assert( IsValidIndex( i ) );
		return m_Memory[i];
	}

	[[nodiscard]] T &Element( intp i )
	{
		Assert( IsValidIndex( i ) );
		return m_Memory[i];
	}

	[[nodiscard]] const T &Element(intp i) const
	{
		Assert( IsValidIndex( i ) );
		return m_Memory[i];
	}

	[[nodiscard]] T &Random()
	{
		return m_Memory[RandomIntp( 0, MAX_SIZE - 1 )];
	}

	[[nodiscard]] const T &Random() const
	{
		return m_Memory[RandomIntp( 0, MAX_SIZE - 1 )];
	}

	[[nodiscard]] iterator begin() { return Base(); }
	[[nodiscard]] const_iterator begin() const { return Base(); }
	[[nodiscard]] iterator end() { return Base() + Count(); }
	[[nodiscard]] const_iterator end() const { return Base() + Count(); }

	[[nodiscard]] T *Base() { return m_Memory; }
	[[nodiscard]] const T *Base() const { return m_Memory; }

	[[nodiscard]] constexpr intp Count() const
	{
		return MAX_SIZE;
	}

	[[nodiscard]] constexpr intp NumAllocated() const
	{
		return MAX_SIZE;
	}

	[[nodiscard]] constexpr bool IsValidIndex( intp i ) const
	{
		return i >= 0 && i < MAX_SIZE;
	}

	[[nodiscard]] static constexpr intp InvalidIndex() { return -1; }

	void CopyArray( const T *pArray, intp count )
	{
		if ( count < 0 )
		{
			Assert( count >= 0 );
			return;
		}
		if ( count > MAX_SIZE )
		{
			Assert( count <= MAX_SIZE );
			return;
		}
		if ( count != 0 && pArray == nullptr )
		{
			Assert( pArray != nullptr );
			return;
		}

		// If the source begins inside this array, make sure the requested
		// range remains within it.  Forward copying is safe because the
		// destination begins at index 0, at or before the source.
		for ( intp offset = 0; offset < MAX_SIZE; ++offset )
		{
			if ( pArray == m_Memory + offset )
			{
				if ( count > MAX_SIZE - offset )
				{
					Assert( count <= MAX_SIZE - offset );
					return;
				}
				break;
			}
		}

		for ( intp i = 0; i < count; ++i )
			m_Memory[i] = pArray[i];
	}

	void Swap( CUtlArray &other )
	{
		for ( intp i = 0; i < MAX_SIZE; ++i )
			V_swap( m_Memory[i], other.m_Memory[i] );
	}

	[[nodiscard]] intp Find( const T &src ) const
	{
		for ( intp i = 0; i < Count(); ++i )
		{
			if ( Element( i ) == src )
				return i;
		}
		return InvalidIndex();
	}

	void FillWithValue( const T &src )
	{
		for ( intp i = 0; i < Count(); ++i )
			Element( i ) = src;
	}

	[[nodiscard]] bool HasElement( const T &src ) const
	{
		return Find( src ) >= 0;
	}

	void DeleteElements()
	{
		static_assert(std::is_pointer_v<T> &&
			std::is_object_v<std::remove_pointer_t<T>>,
			"DeleteElements requires pointers to object types");

		// dimhotepus: Null duplicate entries too, so repeated pointers are deleted only once.
		for ( intp i = 0; i < Count(); ++i )
		{
			T ptr = Element( i );
			Element( i ) = nullptr;

			if ( ptr == nullptr )
				continue;

			for ( intp j = i + 1; j < Count(); ++j )
			{
				if ( Element( j ) == ptr )
					Element( j ) = nullptr;
			}

			delete ptr;
		}
	}

	void Sort(int (__cdecl *pfnCompare)( const T *, const T * ))
	{
		if ( Count() <= 1 )
			return;

		Assert( pfnCompare != nullptr );
		if ( pfnCompare == nullptr )
			return;

		std::sort( begin(), end(),
			[pfnCompare]( const T &a, const T &b )
			{
				return pfnCompare( &a, &b ) < 0;
			} );
	}

protected:
	T m_Memory[MAX_SIZE];
};

#endif // UTLARRAY_H
