//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
// Utilities for globally unique IDs
//=============================================================================//

#ifndef UNIQUEID_H
#define UNIQUEID_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct UniqueId_t;
class CUtlBuffer;


//-----------------------------------------------------------------------------
// Defines a globally unique ID
//-----------------------------------------------------------------------------
// dimhotepus: Add alignment as UUID on Windows has the same.
struct alignas(4) UniqueId_t 
{
	unsigned char m_Value[16];
};


//-----------------------------------------------------------------------------
// Methods related to unique ids
//-----------------------------------------------------------------------------
[[nodiscard]] bool CreateUniqueId(UniqueId_t *pDest);
void InvalidateUniqueId( UniqueId_t *pDest );
[[nodiscard]] bool IsUniqueIdValid( const UniqueId_t &id );
[[nodiscard]] bool IsUniqueIdEqual( const UniqueId_t &id1, const UniqueId_t &id2 );

[[nodiscard]] bool UniqueIdToString( const UniqueId_t &id, OUT_Z_CAP(nMaxLen) char *pBuf, intp nMaxLen );
// dimhotepus: Bounds safe version + ensure 37+ chars to store uuid_t and '\0'.
template<intp outSize>
[[nodiscard]] std::enable_if_t<outSize >= 37, bool> UniqueIdToString( const UniqueId_t &id, OUT_Z_ARRAY char (&pBuf)[outSize] )
{
  return UniqueIdToString( id, pBuf, outSize );
}

[[nodiscard]] bool UniqueIdFromString( UniqueId_t *pDest, IN_CAP(nMaxLen) const char *pBuf, intp nMaxLen = 0 );
void CopyUniqueId( const UniqueId_t &src, UniqueId_t *pDest );
[[nodiscard]] bool Serialize( CUtlBuffer &buf, const UniqueId_t &src );
[[nodiscard]] bool Unserialize( CUtlBuffer &buf, UniqueId_t &dest );

[[nodiscard]] inline bool operator ==( const UniqueId_t& lhs, const UniqueId_t& rhs )
{
	return IsUniqueIdEqual( lhs, rhs );
}


#endif // UNIQUEID_H

