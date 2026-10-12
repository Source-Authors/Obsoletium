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
	if ( nMaxTokenLen == 0 )
		return nullptr;

	pTokenOut[0] = '\0';

	if ( pWasQuoted )
		*pWasQuoted = false;

	// dimhotepus: TF2 backport.
	if ( nMaxTokenLen == 1 )
		return nullptr;

	if ( !pFileBytes )
		return nullptr;

	// YWB:  Ignore colons as token separators in COM_Parse
	const characterset_t& breaks = pCharSet ? *pCharSet : g_BreakSetIncludingColons;
	
	// dimhotepus: char may be signed.  Bytes above 0x7F can compare as <= ' '
	// and be treated as whitespace, or terminate a word unexpectedly.
	unsigned char c;
	const char *pInitialFileBytes = pFileBytes;
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
			if (c=='\"')
			{
				pTokenOut[len] = '\0';
				return pFileBytes;
			}
			if (!c)
			{
				pTokenOut[len] = '\0';
				// dimhotepus: Reset pointer back to not point past '\0'
				--pFileBytes;
				return pFileBytes;
			}
			pTokenOut[len] = c;
			len++;
			
			// dimhotepus: TF2: Ensure buffer length is not overrunning!
			if ( len == maxLen )
			{
				pTokenOut[len] = '\0';
				AssertMsg(false,
					"Quoted token overflow. %s token overflow at position %zd. Copied only %zu chars to %s",
					pInitialFileBytes,
					pFileBytes - pInitialFileBytes,
					len,
					pTokenOut );
				Warning( "Quoted token overflow. %s token overflow at position %zd. Copied only %zu chars to %s",
					pInitialFileBytes,
					pFileBytes - pInitialFileBytes,
					len,
					pTokenOut );
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
			AssertMsg(false,
				"Token overflow. %s token overflow at position %zd. Copied only %zu chars to %s",
				pInitialFileBytes,
				pFileBytes - pInitialFileBytes,
				len,
				pTokenOut );
			Warning( "Token overflow. %s token overflow at position %zd. Copied only %zu chars to %s",
				pInitialFileBytes,
				pFileBytes - pInitialFileBytes,
				len,
				pTokenOut );
			return pFileBytes;
		}

		c = *pFileBytes;
		if ( breaks.HasChar( c ) )
			break;
	} while (c>32);
	
	pTokenOut[min(maxLen, len)] = '\0';
	return pFileBytes;
}
