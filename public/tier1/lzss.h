//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	LZSS Codec. Designed for fast cheap gametime encoding/decoding. Compression results
//	are	not aggresive as other alogrithms, but gets 2:1 on most arbitrary uncompressed data.
//
//=====================================================================================//

#ifndef _LZSS_H
#define _LZSS_H
#pragma once

#include "tier0/platform.h"

constexpr inline uint32 LZSS_ID{MAKEUID('L', 'Z', 'S', 'S')};
constexpr inline uint32 SNAPPY_ID{MAKEUID('S', 'N', 'A', 'P')};

// bind the buffer for correct identification
struct lzss_header_t
{
	unsigned int	id;
	unsigned int	actualSize;	// always little endian
};

class CUtlBuffer;

#define DEFAULT_LZSS_WINDOW_SIZE 4096

class CLZSS
{
public:
	unsigned char*	Compress( IN_BYTECAP(inSize) const unsigned char *in, int inSize, unsigned int *outSize );
	unsigned char*	CompressNoAlloc( IN_BYTECAP(inSize) const unsigned char *in, int inSize, OUT_BYTECAP(*outSize) unsigned char *out, unsigned int *outSize );
	[[deprecated("Unsafe as no buffer size")]] unsigned int	Uncompress( const unsigned char *in, unsigned char *out );
	unsigned int	SafeUncompress( const unsigned char *in, OUT_BYTECAP(outSize) unsigned char *out, unsigned int outSize );

	[[nodiscard]] static bool			IsCompressed( const void *in );
	[[nodiscard]] static unsigned int	GetActualSize( const void *in );

	// windowsize must be a power of two.
	FORCEINLINE CLZSS( int nWindowSize = DEFAULT_LZSS_WINDOW_SIZE );

private:
	// expected to be sixteen bytes
	struct lzss_node_t
	{
		const unsigned char	*pData;
		lzss_node_t		*pPrev;
		lzss_node_t		*pNext;
		char			empty[4];
	};

	struct lzss_list_t
	{
		lzss_node_t *pStart;
		lzss_node_t *pEnd;
	};

	void			BuildHash( const unsigned char *pData );
	lzss_list_t		*m_pHashTable;	
	lzss_node_t		*m_pHashTarget;
	int             m_nWindowSize;

};

FORCEINLINE CLZSS::CLZSS( int nWindowSize )
{
	m_pHashTable = nullptr;
	m_pHashTarget = nullptr;
	m_nWindowSize = nWindowSize;
}
#endif

