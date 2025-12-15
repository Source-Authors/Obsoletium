//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "filesystem_helpers.h"
#include "filesystem.h"
#include "tier1/characterset.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// wordbreak parsing set
static constexpr characterset_t g_BreakSetIncludingColons{"{}()':"};


const char* ParseFileInternal( const char* pFileBytes, OUT_Z_CAP(nMaxTokenLen) char* pTokenOut, bool* pWasQuoted, const characterset_t *pCharSet, size_t nMaxTokenLen )
{
	pTokenOut[0] = '\0';

	if (pWasQuoted)
		*pWasQuoted = false;

	if (!pFileBytes)
		return nullptr;

	// dimhotepus: TF2 backport.
	if ( nMaxTokenLen <= 1 )
		return nullptr;

	// YWB:  Ignore colons as token separators in COM_Parse
	const characterset_t& breaks = pCharSet ? *pCharSet : g_BreakSetIncludingColons;
	
	char c;
	size_t len = 0, maxLen = nMaxTokenLen - 1;
	
// skip whitespace
skipwhite:

	while ( (c = *pFileBytes) <= ' ')
	{
		if (c == 0)
			return nullptr;                    // end of file;
		pFileBytes++;
	}
	
// skip // comments
	if (c=='/' && pFileBytes[1] == '/')
	{
		while (*pFileBytes && *pFileBytes != '\n')
			pFileBytes++;
		goto skipwhite;
	}
	
// skip c-style comments
	if (c=='/' && pFileBytes[1] == '*' )
	{
		// Skip "/*"
		pFileBytes += 2;

		while ( *pFileBytes  )
		{
			if ( *pFileBytes == '*' &&
				 pFileBytes[1] == '/' )
			{
				pFileBytes += 2;
				break;
			}

			pFileBytes++;
		}

		goto skipwhite;
	}

// handle quoted strings specially
	if (c == '\"')
	{
		if (pWasQuoted)
			*pWasQuoted = true;

		pFileBytes++;
		while (1)
		{
			c = *pFileBytes++;
			if (c=='\"' || !c)
			{
				pTokenOut[len] = '\0';
				return pFileBytes;
			}
			pTokenOut[len] = c;
			len++;
			
			// dimhotepus: TF2: Ensure buffer length is not overrunning!
			if ( len == maxLen )
			{
				pTokenOut[len] = '\0';
				Assert(0);
				return pFileBytes;
			}
		}
	}

// parse single characters
	if ( breaks.HasChar( c ) )
	{
		pTokenOut[len] = c;
		len += ( len < maxLen ) ? 1 : 0;
		pTokenOut[len] = '\0';
		return pFileBytes+1;
	}

// parse a regular word
	do
	{
		pTokenOut[len] = c;
		pFileBytes++;
		len++;
		
		// dimhotepus: TF2: Ensure buffer length is not overrunning!
		if ( len == maxLen )
		{
			pTokenOut[len] = '\0';
			Assert(0);
			return pFileBytes;
		}

		c = *pFileBytes;
		if ( breaks.HasChar( c ) )
			break;
	} while (c>32);
	
	pTokenOut[min(maxLen, len)] = '\0';
	return pFileBytes;
}
