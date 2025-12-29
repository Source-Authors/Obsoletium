//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef UNICODEFILEHELPERS_H
#define UNICODEFILEHELPERS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/annotations.h"
#include "tier0/basetypes.h"
#include "tier0/platform.h"

// helper functions for parsing unicode file buffers
[[nodiscard]] ucs2 *AdvanceOverWhitespace(ucs2 *start);
ucs2 *ReadUnicodeToken(ucs2 *start, OUT_CAP(tokenBufferSize) ucs2 *token, intp tokenBufferSize, bool &quoted);
template<intp tokenSize>
ucs2 *ReadUnicodeToken(ucs2 *start, OUT_Z_ARRAY ucs2 (&token)[tokenSize], bool &quoted)
{
	return ReadUnicodeToken( start, token, tokenSize, quoted );
}
ucs2 *ReadUnicodeTokenNoSpecial(ucs2 *start, OUT_CAP(tokenBufferSize) ucs2 *token, intp tokenBufferSize, bool &quoted);
template<intp tokenSize>
ucs2 *ReadUnicodeTokenNoSpecial(ucs2 *start, OUT_Z_ARRAY ucs2 (&token)[tokenSize], bool &quoted)
{
	return ReadUnicodeTokenNoSpecial( start, token, tokenSize, quoted );
}
[[nodiscard]] ucs2 *ReadToEndOfLine(ucs2 *start);

// writing to unicode files via CUtlBuffer
class CUtlBuffer;
void WriteUnicodeString(CUtlBuffer &buffer, const wchar_t *string, bool addQuotes = false);
void WriteAsciiStringAsUnicode(CUtlBuffer &buffer, const char *string, bool addQuotes = false);



#endif // UNICODEFILEHELPERS_H
