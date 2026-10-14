//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
// The CUtlAllocation class:
// A single allocation in the style of CUtlMemory/CUtlString/CUtlBuffer
//			as compact as possible, no virtuals or extraneous data
//			to be used primarily to replace CUtlBuffer
//=============================================================================

#ifndef UTLALLOCATION_H
#define UTLALLOCATION_H

#include "utlmemory.h"
#include "strtools.h"

#include <cstddef>  // std::size_t
#include <cstdlib>  // malloc, free

class CUtlAllocation
{
public:
	// constructor, destructor
	CUtlAllocation()
		: m_pMemory{nullptr}
	{
	}

	CUtlAllocation( const void *pMemory, intp cub )
		: CUtlAllocation{}
	{
		Copy( pMemory, cub );
	}

	CUtlAllocation( const CUtlAllocation &src )
		: CUtlAllocation{}
	{
		Copy( src );
	}

	~CUtlAllocation()
	{
		Purge();
	}

	CUtlAllocation &operator=( const CUtlAllocation &src )
	{
		Copy( src );
		return *this;
	}

	[[nodiscard]] bool operator==( const CUtlAllocation &src ) const
	{
		const intp count{Count()};
		if ( count != src.Count() )
			return false;

		// dimhotepus: Speedup empty equality comparison.
		if ( count == 0 )
			return true;

		return Q_memcmp( Base(), src.Base(), count ) == 0;
	}

	void Copy( const void *pMemory, intp cub )
	{
		// dimhotepus: Check we have smth to copy.
		Assert( cub >= 0 );

		if ( pMemory == nullptr || cub <= 0 )
		{
			Purge();
			return;
		}

		if ( cub == Count() )
		{
			// dimhotepus: Q_memcpy -> Q_memmove. Handles self-copy and overlapping ranges.
			Q_memmove( Base(), pMemory, cub );
			return;
		}

		// dimhotepus: Allocate and copy before freeing the old block. This also supports
		// pMemory pointing into the current allocation when the size changes.
		const std::size_t allocationSize{
			sizeof(ActualMemory_t) + static_cast<std::size_t>( cub )};
		ActualMemory_t *pNewMemory =
			static_cast<ActualMemory_t *>( malloc( allocationSize ) );

		// dimhotepus: Preserve the old allocation if allocation fails.
		if ( pNewMemory == nullptr )
			return;

		pNewMemory->cub = cub;
		Q_memmove( reinterpret_cast<uint8 *>( pNewMemory + 1 ),
			pMemory, static_cast<std::size_t>( cub ) );

		Purge();
		m_pMemory = pNewMemory;
	}

	[[nodiscard]] uint8* Base()
	{
		return m_pMemory == nullptr
			? nullptr
			: reinterpret_cast<uint8 *>( m_pMemory + 1 );
	}

	[[nodiscard]] const uint8* Base() const
	{
		return m_pMemory == nullptr
			? nullptr
			: reinterpret_cast<const uint8 *>( m_pMemory + 1 );
	}

	[[nodiscard]] intp Count() const
	{
		return m_pMemory == nullptr ? 0 : m_pMemory->cub;
	}

	void Purge()
	{
		free( m_pMemory );
		m_pMemory = nullptr;
	}

	void Copy( const CUtlAllocation &alloc )
	{
		Copy( alloc.Base(), alloc.Count() );
	}

	void Swap( CUtlAllocation &alloc )
	{
		std::swap( m_pMemory, alloc.m_pMemory );
	}

	void Alloc( intp cub )
	{
		Purge();

		// dimhotepus: Check we allocate smth.
		Assert( cub >= 0 );
		if ( cub <= 0 )
			return;

		const std::size_t allocationSize{
			sizeof(ActualMemory_t) + static_cast<std::size_t>( cub )};
		m_pMemory =
			static_cast<ActualMemory_t *>( malloc( allocationSize ) );

		// dimhotepus: Honor allocation failures.
		if ( m_pMemory != nullptr )
			m_pMemory->cub = cub;
	}

private:
	// dimhotepus: Header only. Payload begins immediately after it.
	struct ActualMemory_t
	{
		// dimhotepus: x86-64 support.
		intp cub;
		// dimhotepus: Drop not needed rgub.
	};

	ActualMemory_t *m_pMemory;
};

#endif // UTLALLOCATION_H
