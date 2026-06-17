//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Variant Pearson Hash general purpose hashing algorithm described
//			by Cargill in C++ Report 1994.  Generates a 16-bit result.
//
//=============================================================================

#include "tier1/generichash.h"

#include <cctype>
#include <cstdlib>

#include "tier0/basetypes.h"
#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Case-insensitive string 
//-----------------------------------------------------------------------------
unsigned FASTCALL HashStringCaseless( const char *pszKey )
{
	const auto *k = (const uint8 *) pszKey;
	unsigned	even = 0,
				odd  = 0,
				n;

	while ((n = toupper(*k++)) != 0)
	{
		even = g_nRandomValues[odd ^ n];
		if ((n = toupper(*k++)) != 0)
			odd = g_nRandomValues[even ^ n];
		else
			break;
	}

	return (even << 8) | odd;
}

//-----------------------------------------------------------------------------
// 32 bit conventional case-insensitive string 
//-----------------------------------------------------------------------------
unsigned FASTCALL HashStringCaselessConventional( const char *pszKey )
{
	unsigned hash = 0xAAAAAAAA; // Alternating 1's and 0's to maximize the effect of the later multiply and add

	for( ; *pszKey ; pszKey++ )
	{
		// dimhotepus: tolower -> V_tolower.
		hash = ( ( hash << 5 ) + hash ) + (uint8)V_tolower(*pszKey);
	}

	return hash;
}


constexpr inline unsigned char HashToLowerU( char c )
{
	return static_cast<unsigned char>( c >= 'A' && c <= 'Z' ? ( c + 32 ) : c );
}

uint32 MurmurHash2LowerCase( char const *pString, uint32 nSeed )
{
	size_t nLen = strlen( pString );
	char *p = stackallocT( char, nLen + 1 );
	for( size_t i = 0; i < nLen ; i++ )
	{
		p[i] = HashToLowerU( pString[i] );
	}
	return MurmurHash2( p, size_cast<int>( nLen ), nSeed );
}


//-----------------------------------------------------------------------------
// Murmur hash, 64 bit- endian neutral
//-----------------------------------------------------------------------------
uint64 MurmurHash64( const void * key, int len, uint32 seed )
{
	// 'm' and 'r' are mixing constants generated offline.
	// They're not really 'magic', they just happen to work well.

	constexpr uint32 m = 0x5bd1e995;
	constexpr int r = 24;

	// Initialize the hash to a 'random' value

	uint32 h1 = seed ^ len;
	uint32 h2 = 0;

	// Mix 4 bytes at a time into the hash

	const auto * data = (const uint32 *)key;
	while ( len >= 8 )
	{
		uint32 k1 = LittleDWord( *data++ );
		k1 *= m; k1 ^= k1 >> r; k1 *= m;
		h1 *= m; h1 ^= k1;
		len -= 4;

		uint32 k2 = LittleDWord( *data++ );
		k2 *= m; k2 ^= k2 >> r; k2 *= m;
		h2 *= m; h2 ^= k2;
		len -= 4;
	}

	if(len >= 4)
	{
		uint32 k1 = LittleDWord( *data++ );
		k1 *= m; k1 ^= k1 >> r; k1 *= m;
		h1 *= m; h1 ^= k1;
		len -= 4;
	}

	// Handle the last few bytes of the input array
	switch(len)
	{
	case 3: h2 ^= ((const uint8*)data)[2] << 16;
        [[fallthrough]];
	case 2: h2 ^= ((const uint8*)data)[1] << 8;
        [[fallthrough]];
	case 1: h2 ^= ((const uint8*)data)[0];
			h2 *= m;
	}

	h1 ^= h2 >> 18; h1 *= m;
	h2 ^= h1 >> 22; h2 *= m;
	h1 ^= h2 >> 17; h1 *= m;
	h2 ^= h1 >> 19; h2 *= m;

	uint64 h = h1;

	h = (h << 32) | h2;

	return h;
}

