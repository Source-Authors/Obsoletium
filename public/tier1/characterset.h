//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared code for parsing / searching for characters in a string
//			using lookup tables

#ifndef CHARACTERSET_H
#define CHARACTERSET_H

#ifdef _WIN32
#pragma once
#endif

#include <climits>

struct characterset_t
{
	char set[1 << (sizeof(unsigned char) * CHAR_BIT)];
};


// This is essentially a strpbrk() using a precalculated lookup table
//-----------------------------------------------------------------------------
// Purpose: builds a simple lookup table of a group of important characters
// Input  : *pSetBuffer - pointer to the buffer for the group
//			*pSetString - list of characters to flag
//-----------------------------------------------------------------------------
extern void CharacterSetBuild( characterset_t *pSetBuffer, const char *pSetString );


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSetBuffer - pre-build group buffer
//			character - character to lookup
// Output : int - 1 if the character was in the set
//-----------------------------------------------------------------------------
[[nodiscard]] constexpr inline int IN_CHARACTERSET( const characterset_t& SetBuffer, int character )
{
  return SetBuffer.set[static_cast<unsigned char>(character)];
}


#endif // CHARACTERSET_H
