//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "bonelist.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CBoneList::CBoneList()
{
	m_bShouldDelete = false;
	m_nBones = 0;
	BitwiseClear( m_vecPos );
	BitwiseClear( m_quatRot );
}

void CBoneList::Release()
{
	if (m_bShouldDelete )
	{
		delete this;
	}
	else
	{
		Warning( "Called Release() on CBoneList not allocated via Alloc() method\n" );
	}
}

CBoneList *CBoneList::Alloc()
{
	CBoneList *newList = new CBoneList;
	Assert( newList );
	if ( newList )
	{
		newList->m_bShouldDelete = true;
	}
	return newList;
}

CFlexList::CFlexList()
{
	m_bShouldDelete = false;
	m_nNumFlexes = 0;
	BitwiseClear( m_flexWeights );
}

void CFlexList::Release()
{
	if (m_bShouldDelete )
	{
		delete this;
	}
	else
	{
		Warning( "Called Release() on CFlexList not allocated via Alloc() method\n" );
	}
}

CFlexList *CFlexList::Alloc()
{
	CFlexList *newList = new CFlexList;
	Assert( newList );
	if ( newList )
	{
		newList->m_bShouldDelete = true;
	}
	return newList;
}