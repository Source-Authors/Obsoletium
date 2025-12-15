// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_FILESYSTEM_HELPERS_H_
#define SRC_FILESYSTEM_HELPERS_H_

#include "tier0/annotations.h"

struct characterset_t;

// dimhotepus: Make const characterset_t.
// Don't call this directly.  You should (as long as your destination is an array) be
// able to call ParseFile, which is safer as it infers your array size for you.
const char* ParseFileInternal( const char* pFileBytes, OUT_Z_CAP(nMaxTokenLen) char* pTokenOut, bool* pWasQuoted, const characterset_t *pCharSet, size_t nMaxTokenLen );

// dimhotepus: Make const characterset_t.
// Call until it returns NULL. Each time you call it, it will parse out a token.
template <size_t count>
const char* ParseFile( const char* pFileBytes, OUT_Z_ARRAY char (&pTokenOut)[count], bool* pWasQuoted, const characterset_t *pCharSet = nullptr, [[maybe_unused]] unsigned int nMaxTokenLen = (unsigned int)-1 )
{
	return ParseFileInternal( pFileBytes, pTokenOut, pWasQuoted, pCharSet, count );
}

template <size_t count>
char* ParseFile( char* pFileBytes, OUT_Z_ARRAY char (&pTokenOut)[count], bool* pWasQuoted )	// (same exact thing as the const version)
{
	return const_cast<char*>( ParseFileInternal( pFileBytes, pTokenOut, pWasQuoted, nullptr, count ) );
}

#endif // FILESYSTEM_HELPERS_H
