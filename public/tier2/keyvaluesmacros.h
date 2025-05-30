//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//==================================================================================================

#ifndef SE_TIER2_KEYVALUESMACROS_H_
#define SE_TIER2_KEYVALUESMACROS_H_
#pragma once


//--------------------------------------------------------------------------------------------------
// Returns true if the passed string matches the filename style glob, false otherwise
// * matches any characters, ? matches any single character, otherwise case insensitive matching
//--------------------------------------------------------------------------------------------------
[[nodiscard]] bool GlobMatch( const char *pszGlob, const char *pszString );

class KeyValues;

//--------------------------------------------------------------------------------------------------
// Processes #insert and #update KeyValues macros
//
// #insert inserts a new KeyValues file replacing the KeyValues #insert with the new file
//
// #update updates sibling KeyValues blocks subkeys with its subkeys, overwriting and adding
// KeyValues as necessary
//--------------------------------------------------------------------------------------------------
KeyValues *HandleKeyValuesMacros( KeyValues *kv, KeyValues *pkvParent = nullptr );

#endif  // SE_TIER2_KEYVALUESMACROS_H_
