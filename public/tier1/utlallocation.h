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

class CUtlAllocation
{
public:

	// constructor, destructor
	CUtlAllocation()			
	{ 
		m_pMemory = nullptr;
	}

	CUtlAllocation( const void *pMemory, int cub )			
	{ 
		m_pMemory = nullptr;
		Copy( pMemory, cub );
	}

	CUtlAllocation( CUtlAllocation const &src )
	{ 
		m_pMemory = nullptr;
		Copy( src );
	}

	~CUtlAllocation()
	{
		Purge();
	}

	CUtlAllocation &operator=( CUtlAllocation const &src )
	{
		Copy( src );
		return *this;
	}

	bool operator==( CUtlAllocation const &src )
	{
		if ( Count() != src.Count() )
			return false;
		return Q_memcmp( Base(), src.Base(), Count() ) == 0;
	}

	void Copy( const void *pMemory, int cub )
	{
		if ( cub == 0 || pMemory == nullptr )
		{
			Purge();
			return;
		}
		if ( cub != Count() )
		{
			Purge();
			m_pMemory = (ActualMemory_t *)malloc( cub + sizeof( int ) ); 										
			m_pMemory->cub = cub;
		}
		Q_memcpy( Base(), pMemory, cub );
	}

	// Gets the base address
	uint8* Base()
	{ 
		if ( m_pMemory == nullptr ) 
			return nullptr; 
		return m_pMemory->rgub; 
	}

	[[nodiscard]] const uint8* Base() const
	{ 
		if ( m_pMemory == nullptr ) 
			return nullptr; 
		return m_pMemory->rgub; 
	}

	// Size
	[[nodiscard]] int Count() const
	{ 
		if ( m_pMemory == nullptr ) 
			return 0; 
		return m_pMemory->cub; 
	}

	// Memory deallocation
	void Purge()											
	{ 
		if ( m_pMemory )
			free(m_pMemory); 
		m_pMemory = nullptr; 
	}

	void Copy( const CUtlAllocation &alloc )
	{
		Copy( alloc.Base(), alloc.Count() );
	}

	void Swap( CUtlAllocation &alloc )
	{
		ActualMemory_t *pTemp = m_pMemory;
		m_pMemory = alloc.m_pMemory;
		alloc.m_pMemory = pTemp;
	}

	void Alloc( int cub )
	{
		Purge();
		m_pMemory = (ActualMemory_t *)malloc( cub + sizeof( int ) ); 										
		m_pMemory->cub = cub;
	}

private:
	struct ActualMemory_t
	{
		int cub;
		uint8 rgub[4];	// i'd prefer to make this 0 but the compiler whines when i do
	};

	ActualMemory_t *m_pMemory;
};

#endif // UTLALLOCATION_H
