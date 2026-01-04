//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Generic MD5 hashing algo
//
//=============================================================================//

#ifndef CHECKSUM_MD5_H
#define CHECKSUM_MD5_H

#include "tier0/platform.h"

#include <memory.h>

#ifdef PLATFORM_64BITS
#define SE_MD5_ARG_0 MD5Value_t
#define SE_MD5_ARG_1 MD5Value_t
#else
#define SE_MD5_ARG_0 const MD5Value_t &
#define SE_MD5_ARG_1 const MD5Value_t &
#endif

// 16 bytes == 128 bit digest
constexpr inline int MD5_DIGEST_LENGTH{16};
constexpr inline size_t MD5_BIT_LENGTH{MD5_DIGEST_LENGTH * sizeof(unsigned char)};

struct MD5Value_t
{
	unsigned char bits[MD5_DIGEST_LENGTH];

	void Zero();
	[[nodiscard]] bool IsZero() const;

	[[nodiscard]] bool operator==( SE_MD5_ARG_0 src ) const;
	[[nodiscard]] bool operator!=( SE_MD5_ARG_0 src ) const;
};

// MD5 Hash
struct MD5Context_t
{
	unsigned int	buf[4];
    unsigned int	bits[2];
    unsigned char	in[64];
};

void MD5Init( MD5Context_t *context );
void MD5Update( MD5Context_t *context, IN_BYTECAP(len) const void *buf, unsigned int len );
template<typename T, unsigned size>
std::enable_if_t<!std::is_pointer_v<T>> MD5Update( MD5Context_t *context, const T (&buffer)[size] )
{
	MD5Update( context, buffer, static_cast<unsigned>(sizeof(T)) * size );
}
template<typename T>
std::enable_if_t<!std::is_pointer_v<T>> MD5Update( MD5Context_t *context, const T &value )
{
	MD5Update( context, &value, static_cast<unsigned>(sizeof(value)) );
}

void MD5Final( OUT_BYTECAP_C(MD5_DIGEST_LENGTH) unsigned char digest[ MD5_DIGEST_LENGTH ], MD5Context_t *context );

[[nodiscard]] char *MD5_Print( IN_BYTECAP(hashlen) unsigned char *digest, intp hashlen );

template<intp size>
[[nodiscard]] char *MD5_Print( unsigned char (&digest)[size] )
{
	return MD5_Print( digest, size );
}

/// Convenience wrapper to calculate the MD5 for a buffer, all in one step, without
/// bothering with the context object.
// dimhotepus: int -> unsigned to match MD5Update.
void MD5_ProcessSingleBuffer( IN_BYTECAP(len) const void *p, unsigned int len, MD5Value_t &md5Result );

[[nodiscard]] unsigned int MD5_PseudoRandom( unsigned int nSeed );

/// Returns true if the values match.
//-----------------------------------------------------------------------------
// dimhotepus: Inline for performance.
[[nodiscard]] inline bool MD5_Compare( SE_MD5_ARG_0 data, SE_MD5_ARG_1 compare )
{
	return std::memcmp( data.bits, compare.bits, MD5_DIGEST_LENGTH ) == 0;
}

[[nodiscard]] inline bool MD5Value_t::operator==( SE_MD5_ARG_0 src ) const
{
	return MD5_Compare( *this, src );
}

[[nodiscard]] inline bool MD5Value_t::operator!=( SE_MD5_ARG_0 src ) const
{
	return !(*this == src);
}

#endif // CHECKSUM_MD5_H
