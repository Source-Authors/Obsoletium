//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================

#include "characterset.h"

#include <cstring>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: builds a simple lookup table of a group of important characters
// Input  : *pParseGroup - pointer to the buffer for the group
//			*pGroupString - null terminated list of characters to flag
//-----------------------------------------------------------------------------
void CharacterSetBuild( characterset_t *pSetBuffer, const char *pszSetString )
{
	// Test our pointers
	if ( !pSetBuffer || !pszSetString )
		return;

	memset( pSetBuffer->set, 0, sizeof(pSetBuffer->set) );
	
	ptrdiff_t i = 0;
	while ( pszSetString[i] )
	{
		pSetBuffer->set[ (unsigned)pszSetString[i] ] = 1;
		i++;
	}

}
