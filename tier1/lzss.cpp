//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	LZSS Codec. Designed for fast cheap gametime encoding/decoding. Compression results
//	are	not aggresive as other alogrithms, but gets 2:1 on most arbitrary uncompressed data.
//
//=====================================================================================//

#include "tier1/lzss.h"

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier0/vprof.h"
#include "tier0/etwprof.h"
#include "tier1/utlbuffer.h"

constexpr inline int LZSS_LOOKSHIFT{4};
constexpr inline int LZSS_LOOKAHEAD{1 << LZSS_LOOKSHIFT};

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Returns true if buffer is compressed.
//-----------------------------------------------------------------------------
bool CLZSS::IsCompressed( const void *pInput )
{
	const auto *pHeader = static_cast<const lzss_header_t *>(pInput);
	return pHeader && pHeader->id == LZSS_ID;
}

//-----------------------------------------------------------------------------
// Returns uncompressed size of compressed input buffer. Used for allocating output
// buffer for decompression. Returns 0 if input buffer is not compressed.
//-----------------------------------------------------------------------------
unsigned int CLZSS::GetActualSize( const void *pInput )
{
	const auto *pHeader = static_cast<const lzss_header_t *>(pInput);
	return pHeader && pHeader->id == LZSS_ID
		? LittleLong( pHeader->actualSize )
		: 0;
}

void CLZSS::BuildHash( const unsigned char *pData )
{
	lzss_list_t *pList;

	intp targetindex = (intp)pData & ( m_nWindowSize - 1 );
	lzss_node_t *pTarget = &m_pHashTarget[targetindex];
	if ( pTarget->pData )
	{
		pList = &m_pHashTable[*pTarget->pData];
		if ( pTarget->pPrev )
		{
			pList->pEnd = pTarget->pPrev;
			pTarget->pPrev->pNext = 0;
		}
		else
		{
			pList->pEnd = 0;
			pList->pStart = 0;
		}
	}

	pList = &m_pHashTable[*pData];
	pTarget->pData = pData;
	pTarget->pPrev = 0;
	pTarget->pNext = pList->pStart;
	if ( pList->pStart )
	{
		pList->pStart->pPrev = pTarget;
	}
	else
	{
		pList->pEnd = pTarget;
	}
	pList->pStart = pTarget;
}

unsigned char *CLZSS::CompressNoAlloc(
	IN_BYTECAP(inputLength) const unsigned char *pInput, int inputLength,
	OUT_BYTECAP(*pOutputSize) unsigned char *pOutputBuf, unsigned int *pOutputSize )
{
	if ( inputLength <= static_cast<int>(sizeof( lzss_header_t )) + 8 )
	{
		return nullptr;
	}

	VPROF( "CLZSS::CompressNoAlloc" );
	ETWMark1I("CompressNoAlloc", inputLength );

	// create the compression work buffers, small enough (~64K) for stack
	constexpr size_t tableSize = 256 * sizeof( lzss_list_t );
	m_pHashTable = stackallocT( lzss_list_t, 256 );
	memset( m_pHashTable, 0, tableSize );

	const intp targetSize = m_nWindowSize * sizeof( lzss_node_t );
	m_pHashTarget = stackallocT( lzss_node_t, m_nWindowSize );
	memset( m_pHashTarget, 0, targetSize );

	// allocate the output buffer, compressed buffer is expected to be less, caller will free
	unsigned char *pStart = pOutputBuf;
	// prevent compression failure (inflation), leave enough to allow dribble eof bytes
	unsigned char *pEnd = pStart + inputLength - sizeof ( lzss_header_t ) - 8;

	// set the header
	lzss_header_t *pHeader = (lzss_header_t *)pStart;
	pHeader->id = LZSS_ID;
	pHeader->actualSize = LittleLong( inputLength );

	unsigned char *pOutput = pStart + sizeof (lzss_header_t);
	const unsigned char *pLookAhead = pInput; 
	const unsigned char *pWindow = pInput;
	const unsigned char *pEncodedPosition = NULL;
	unsigned char *pCmdByte = NULL;
	int putCmdByte = 0;

	while ( inputLength > 0 )
	{
		pWindow = pLookAhead - m_nWindowSize;
		if ( pWindow < pInput )
		{
			pWindow = pInput;
		}

		if ( !putCmdByte )
		{
			pCmdByte = pOutput++;
			*pCmdByte = 0;
		}
		putCmdByte = ( putCmdByte + 1 ) & 0x07;

		int encodedLength = 0;
		int lookAheadLength = inputLength < LZSS_LOOKAHEAD ? inputLength : LZSS_LOOKAHEAD;

		lzss_node_t *pHash = m_pHashTable[pLookAhead[0]].pStart;
		while ( pHash )
		{
			int matchLength = 0;
			int length = lookAheadLength;
			while ( length-- && pHash->pData[matchLength] == pLookAhead[matchLength] )
			{
				matchLength++;
			}
			if ( matchLength > encodedLength )
			{
				encodedLength = matchLength;
				pEncodedPosition = pHash->pData;
			}
			if ( matchLength == lookAheadLength )
			{
				break;
			}
			pHash = pHash->pNext;
		}

		if ( encodedLength >= 3 )
		{
			*pCmdByte = ( *pCmdByte >> 1 ) | 0x80;
			*pOutput++ = ( ( pLookAhead-pEncodedPosition-1 ) >> LZSS_LOOKSHIFT );
			*pOutput++ = ( ( pLookAhead-pEncodedPosition-1 ) << LZSS_LOOKSHIFT ) | ( encodedLength-1 );
		} 
		else 
		{ 
			encodedLength = 1;
			*pCmdByte = ( *pCmdByte >> 1 );
			*pOutput++ = *pLookAhead;
		}

		for ( int i=0; i<encodedLength; i++ )
		{
			BuildHash( pLookAhead++ );
		}

		inputLength -= encodedLength;

		if ( pOutput >= pEnd )
		{
			// compression is worse, abandon
			return NULL;
		}
	}

	if ( inputLength != 0 )
	{
		// unexpected failure
		Assert( 0 );
		return NULL;
	}

	if ( !putCmdByte )
	{
		pCmdByte = pOutput++;
		*pCmdByte = 0x01;
	}
	else
	{
		*pCmdByte = ( ( *pCmdByte >> 1 ) | 0x80 ) >> ( 7 - putCmdByte );
	}

	*pOutput++ = 0;
	*pOutput++ = 0;

	if ( pOutputSize )
	{
		*pOutputSize = pOutput - pStart;
	}

	return pStart;
}

//-----------------------------------------------------------------------------
// Compress an input buffer. Caller must free output compressed buffer.
// Returns NULL if compression failed (i.e. compression yielded worse results)
//-----------------------------------------------------------------------------
unsigned char* CLZSS::Compress(
	IN_BYTECAP(inputLength) const unsigned char *pInput, int inputLength,
	unsigned int *pOutputSize )
{
	unsigned char *pStart = (unsigned char *)malloc( inputLength );
	unsigned char *pFinal = CompressNoAlloc( pInput, inputLength, pStart, pOutputSize );
	if ( !pFinal )
	{
		free( pStart );
		return nullptr;
	}

	return pStart;
}

unsigned int CLZSS::SafeUncompress(
	const unsigned char *pInput,
	OUT_BYTECAP(unBufSize) unsigned char *pOutput, unsigned int unBufSize )
{
	unsigned int totalBytes = 0;
	int cmdByte = 0;
	int getCmdByte = 0;

	unsigned int actualSize = GetActualSize( pInput );
	if ( !actualSize )
	{
		// unrecognized
		return 0;
	}

	if ( actualSize > unBufSize )
	{
		Warning( "LZSS Decompression buffer size %u is lower than needed (%u).\n",
			unBufSize, actualSize );
		return 0;
	}

	pInput += sizeof( lzss_header_t );

	for ( ;; )
	{
		if ( !getCmdByte ) 
		{
			cmdByte = *pInput++;
		}
		getCmdByte = ( getCmdByte + 1 ) & 0x07;

		if ( cmdByte & 0x01 )
		{
			int position = *pInput++ << LZSS_LOOKSHIFT;
			position |= ( *pInput >> LZSS_LOOKSHIFT );
			int count = ( *pInput++ & 0x0F ) + 1;
			if ( count == 1 ) 
			{
				break;
			}
			unsigned char *pSource = pOutput - position - 1;

			if ( totalBytes + count > unBufSize )
			{
				return 0;
			}

			for ( int i=0; i<count; i++ )
			{
				*pOutput++ = *pSource++;
			}
			totalBytes += count;
		} 
		else 
		{
			if ( totalBytes + 1 > unBufSize )
				return 0;

			*pOutput++ = *pInput++;
			totalBytes++;
		}
		cmdByte = cmdByte >> 1;
	}

	if ( totalBytes != actualSize )
	{
		// unexpected failure
		Assert( 0 );
		return 0;
	}

	return totalBytes;
}

//-----------------------------------------------------------------------------
// Uncompress a buffer, Returns the uncompressed size. Caller must provide an
// adequate sized output buffer or memory corruption will occur.
//-----------------------------------------------------------------------------
unsigned int CLZSS::Uncompress( const unsigned char *pInput, unsigned char *pOutput )
{
	unsigned int totalBytes = 0;
	int cmdByte = 0;
	int getCmdByte = 0;

	unsigned int actualSize = GetActualSize( pInput );
	if ( !actualSize )
	{
		// unrecognized
		return 0;
	}

	pInput += sizeof( lzss_header_t );

	for ( ;; )
	{
		if ( !getCmdByte ) 
		{
			cmdByte = *pInput++;
		}
		getCmdByte = ( getCmdByte + 1 ) & 0x07;

		if ( cmdByte & 0x01 )
		{
			int position = *pInput++ << LZSS_LOOKSHIFT;
			position |= ( *pInput >> LZSS_LOOKSHIFT );
			int count = ( *pInput++ & 0x0F ) + 1;
			if ( count == 1 ) 
			{
				break;
			}
			unsigned char *pSource = pOutput - position - 1;
			for ( int i=0; i<count; i++ )
			{
				*pOutput++ = *pSource++;
			}
			totalBytes += count;
		} 
		else 
		{
			*pOutput++ = *pInput++;
			totalBytes++;
		}
		cmdByte = cmdByte >> 1;
	}

	if ( totalBytes != actualSize )
	{
		// unexpected failure
		Assert( 0 );
		return 0;
	}

	return totalBytes;
}
