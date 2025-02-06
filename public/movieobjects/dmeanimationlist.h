//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Snapshot of 
//
//===========================================================================//

#ifndef DMEANIMATIONLIST_H
#define DMEANIMATIONLIST_H

#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmattributevar.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeChannelsClip;


//-----------------------------------------------------------------------------
// A class representing a list of animations
//-----------------------------------------------------------------------------
class CDmeAnimationList : public CDmElement
{
	DEFINE_ELEMENT( CDmeAnimationList, CDmElement );

public:
	intp GetAnimationCount() const;
	CDmeChannelsClip *GetAnimation( intp nIndex );
	int FindAnimation( const char *pAnimName );
	void SetAnimation( intp nIndex, CDmeChannelsClip *pAnimation );
	intp AddAnimation( CDmeChannelsClip *pAnimation );
	void RemoveAnimation( intp nIndex );

private:
	CDmaElementArray<CDmeChannelsClip> m_Animations;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline intp CDmeAnimationList::GetAnimationCount() const
{
	return m_Animations.Count();
}

inline CDmeChannelsClip *CDmeAnimationList::GetAnimation( intp nIndex )
{
	return m_Animations[nIndex];
}


#endif // DMEANIMATIONLIST_H
