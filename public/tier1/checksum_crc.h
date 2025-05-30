//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Generic CRC functions
//
// $NoKeywords: $
//=============================================================================//
#ifndef CHECKSUM_CRC_H
#define CHECKSUM_CRC_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"

typedef unsigned int CRC32_t;

void CRC32_Init( CRC32_t *pulCRC );
void CRC32_ProcessBuffer( CRC32_t *pulCRC, IN_BYTECAP(nBuffer) const void *p, intp nBuffer );
template<typename T, intp len>
std::enable_if_t<!std::is_pointer_v<T>> CRC32_ProcessBuffer( CRC32_t *pulCRC, const T (&p)[len] )
{
	CRC32_ProcessBuffer( pulCRC, &p, len );
}
template<typename T>
std::enable_if_t<!std::is_pointer_v<T>> CRC32_ProcessBuffer( CRC32_t *pulCRC, const T &p )
{
	CRC32_ProcessBuffer( pulCRC, &p, sizeof(p) );
}

void CRC32_Final( CRC32_t *pulCRC );
[[nodiscard]] CRC32_t CRC32_GetTableEntry(unsigned int slot);

[[nodiscard]] inline CRC32_t CRC32_ProcessSingleBuffer( IN_BYTECAP(len) const void *p, intp len )
{
	CRC32_t crc;

	CRC32_Init( &crc );
	CRC32_ProcessBuffer( &crc, p, len );
	CRC32_Final( &crc );

	return crc;
}

#endif // CHECKSUM_CRC_H
