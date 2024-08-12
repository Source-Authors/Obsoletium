//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds the enumerated list of default cursors
//
// $NoKeywords: $
//=============================================================================//

#ifndef DAR_H
#define DAR_H

#ifdef _WIN32
#pragma once
#endif

#include <stdlib.h>
#include <string.h>
#include <vgui/VGUI.h>
#include "tier1/utlvector.h"

#include "tier0/memdbgon.h"

namespace vgui
{

//-----------------------------------------------------------------------------
// Purpose: Simple lightweight dynamic array implementation
//-----------------------------------------------------------------------------
template<class ELEMTYPE> class Dar : public CUtlVector< ELEMTYPE >
{
	typedef CUtlVector< ELEMTYPE > BaseClass;
	
public:
	Dar() = default;
	Dar(intp initialCapacity) :
		BaseClass( 0, initialCapacity )
	{
	}

public:
	void SetCount(intp count)
	{
		this->EnsureCount( count );
	}
	intp GetCount()
	{
		return this->Count();
	}
	intp AddElement(ELEMTYPE elem)
	{
		return this->AddToTail( elem );
	}
	void MoveElementToEnd( ELEMTYPE elem )
	{
		if ( this->Count() == 0 )
			return;

		// quick check to see if it's already at the end
		if ( this->Element( this->Count() - 1 ) == elem )
			return;

		intp idx = this->Find( elem );
		if ( idx == this->InvalidIndex() )
			return;

		this->Remove( idx );
		this->AddToTail( elem );
	}
	// returns the index of the element in the array, -1 if not found
	intp FindElement(ELEMTYPE elem)
	{
		return this->Find( elem );
	}
	bool HasElement(ELEMTYPE elem)
	{
		if ( this->FindElement(elem) != this->InvalidIndex() )
		{
			return true;
		}
		return false;
	}
	intp PutElement(ELEMTYPE elem)
	{
		intp index = this->FindElement(elem);
		if (index >= 0)
		{
			return index;
		}
		return this->AddElement(elem);
	}
	// insert element at index and move all the others down 1
	void InsertElementAt(ELEMTYPE elem,intp index)
	{
		this->InsertBefore( index, elem );
	}
	void SetElementAt(ELEMTYPE elem,intp index)
	{
		this->EnsureCount( index + 1 );
		this->Element( index ) = elem;
	}
	void RemoveElementAt(intp index)
	{
		this->Remove( index );
	} 

	void RemoveElementsBefore(intp index)
	{
		if ( index <= 0 )
			return;
		this->RemoveMultiple( 0, index - 1 );
	}  

	void RemoveElement(ELEMTYPE elem)
	{
		this->FindAndRemove( elem );
	}

	void *GetBaseData()
	{
		return this->Base();
	}

	void CopyFrom(Dar<ELEMTYPE> &dar)
	{
		this->CopyArray( dar.Base(), dar.Count() );
	}
};

}

#include "tier0/memdbgoff.h"

#endif // DAR_H
