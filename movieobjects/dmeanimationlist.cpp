//========= Copyright Valve Corporation, All rights reserved. ============//
//
// An element that contains a list of animations
//
//=============================================================================
#include "movieobjects/dmeanimationlist.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects/dmeclip.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeAnimationList, CDmeAnimationList );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeAnimationList::OnConstruction()
{
	m_Animations.Init( this, "animations" );
}

void CDmeAnimationList::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Adds, removes animations
//-----------------------------------------------------------------------------
intp CDmeAnimationList::AddAnimation( CDmeChannelsClip *pAnimation )
{
	return m_Animations.AddToTail( pAnimation );
}

void CDmeAnimationList::RemoveAnimation( intp nIndex )
{
	m_Animations.Remove( nIndex );
}


//-----------------------------------------------------------------------------
// Sets the transform
//-----------------------------------------------------------------------------
void CDmeAnimationList::SetAnimation( intp nIndex, CDmeChannelsClip *pAnimation )
{
	m_Animations.Set( nIndex, pAnimation );
}


//-----------------------------------------------------------------------------
// Finds an animation by name
//-----------------------------------------------------------------------------
intp CDmeAnimationList::FindAnimation( const char *pAnimName ) const
{
	intp nCount = m_Animations.Count();
	for ( intp i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( m_Animations[i]->GetName(), pAnimName ) )
			return i;
	}
	return -1;
}
