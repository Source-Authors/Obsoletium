//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
// Unique ID generation
//=============================================================================//

#include "tier1/uniqueid.h"

#include "tier0/platform.h"

#ifdef IS_WINDOWS_PC
#include "winlite.h" // UUIDCreate
#include <rpc.h>
#else
#ifdef LINUX
#include <uuid.h>
#elif defined(OSX)
#include <uuid/uuid.h>
#endif
#endif

#include "tier1/utlbuffer.h"

//-----------------------------------------------------------------------------
// Creates a new unique id
//-----------------------------------------------------------------------------
bool CreateUniqueId( UniqueId_t *pDest )
{
	if ( !pDest ) return false;

#ifdef IS_WINDOWS_PC
	uuid_t out;
	static_assert(sizeof(out) == sizeof(*pDest));

	const bool ok = SUCCEEDED( UuidCreate( &out ) );
	if (ok)
	{
		V_memcpy( pDest, &out, sizeof(*pDest) );
		return true;
	}

	InvalidateUniqueId( pDest );
	return false;
#elif defined(LINUX) || defined(OSX)
	// dimhotepus: Add basic libuuid implementation.

	// See https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/uuid_generate.3.html
	// See https://www.man7.org/linux/man-pages/man3/uuid_generate.3.html
	uuid_t out;
	static_assert(sizeof(out) == sizeof(*pDest));

	uuid_generate(out);
	V_memcpy( pDest, out, sizeof(*pDest) );

	return true;
#else
#error "Please implement CreateUniqueId on your platform."
#endif
}


//-----------------------------------------------------------------------------
// Creates a new unique id from a string representation of one
//-----------------------------------------------------------------------------
bool UniqueIdFromString( UniqueId_t *pDest, IN_CAP(nMaxLen) const char *pBuf, intp nMaxLen )
{
	if ( !pDest ) return false;

	if ( nMaxLen == 0 )
	{
		nMaxLen = V_strlen( pBuf );

		if ( nMaxLen == 0 ) return false;
	}

	char *pTemp = stackallocT( char, nMaxLen + 1 );
	V_strncpy( pTemp, pBuf, nMaxLen + 1 );

	--nMaxLen;
	while( (nMaxLen > 0) && V_isspace( pTemp[nMaxLen] ) )
	{
		--nMaxLen;
	}

	pTemp[ nMaxLen ] = '\0';

	while( *pTemp && V_isspace( *pTemp ) )
	{
		++pTemp;
	}

#ifdef IS_WINDOWS_PC
	uuid_t out;
	static_assert( sizeof( out ) == sizeof( *pDest ) );

	if ( SUCCEEDED( UuidFromString( (unsigned char *)pTemp, &out ) ) )
	{
		V_memcpy( pDest, &out, sizeof(*pDest) );
		return true;
	}
#elif defined(LINUX) || defined(OSX)
	// dimhotepus: Add basic libuuid implementation.
	// See https://www.man7.org/linux/man-pages/man3/uuid_parse.3.html
	// See https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/uuid_parse.3.html#//apple_ref/doc/man/3/uuid_parse
	uuid_t out;
	static_assert( sizeof( out ) == sizeof( *pDest ) );

	if ( uuid_parse( pTemp, out ) == 0 )
	{
		V_memcpy( pDest, out, sizeof(UniqueId_t) );
		return true;
	}
#else
#error "Please implement UniqueIdFromString on your platform."
#endif

	InvalidateUniqueId( pDest );
	return false;
}

//-----------------------------------------------------------------------------
// Sets an object ID to be an invalid state
//-----------------------------------------------------------------------------
void InvalidateUniqueId( UniqueId_t *pDest )
{
	Assert( pDest );
	BitwiseClear( *pDest );
}

bool IsUniqueIdValid( const UniqueId_t &id )
{
	UniqueId_t invalidId;
	InvalidateUniqueId( &invalidId );
	return !IsUniqueIdEqual( invalidId, id );
}

bool IsUniqueIdEqual( const UniqueId_t &id1, const UniqueId_t &id2 )
{
	return memcmp( &id1, &id2, sizeof( UniqueId_t ) ) == 0; 
}

bool UniqueIdToString( const UniqueId_t &id, OUT_Z_CAP(nMaxLen) char *pBuf, intp nMaxLen )
{
	if ( nMaxLen > 0 )
		pBuf[ 0 ] = '\0';

#ifdef IS_WINDOWS_PC
	uuid_t out;
	static_assert( sizeof( out ) == sizeof( id ) );
	V_memcpy( &out, &id, sizeof(out) );

	unsigned char *outstring = nullptr;
	if ( SUCCEEDED( UuidToString( &out, &outstring ) ) && outstring && *outstring )
	{
		V_strncpy( pBuf, reinterpret_cast<const char *>(outstring), nMaxLen );
		RpcStringFree( &outstring );
		return true;
	}

	return false;
#elif defined(LINUX) || defined(OSX)
	// uuid_unparse_lower expects 36-byte string (plus tailing '\0').
	if (nMaxLen < 37)
		return false;

	// See https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/uuid_unparse.3.html#//apple_ref/doc/man/3/uuid_unparse
	// See https://www.man7.org/linux/man-pages/man3/uuid_unparse.3.html
	uuid_t out;
	static_assert( sizeof( out ) == sizeof( id ) );
	V_memcpy( &out, &id, sizeof(out) );

	// Use uuid_unparse_lower to match Windows UuidToString behavior.
	uuid_unparse_lower( out, pBuf );
	return true;
#else
#error "Please implement UniqueIdToString on your platform."
#endif
}

void CopyUniqueId( const UniqueId_t &src, UniqueId_t *pDest )
{
	memcpy( pDest, &src, sizeof( UniqueId_t ) );
}

bool Serialize( CUtlBuffer &buf, const UniqueId_t &src )
{
	if ( buf.IsText() )
	{
		char idstr[37];
		if ( UniqueIdToString( src, idstr ) )
		{
			buf.PutString( idstr );
			return buf.IsValid();
		}

		buf.PutChar( '\0' );
		return false;
	}

	buf.Put( src );
	return buf.IsValid();
}

bool Unserialize( CUtlBuffer &buf, UniqueId_t &dest )
{
	if ( buf.IsText() )
	{
		const intp nTextLen = buf.PeekStringLength();

		char *pBuf = stackallocT( char, nTextLen );
		buf.GetStringManualCharCount( pBuf, nTextLen );

		// dimhotepus: Check buf is valid first.
		return buf.IsValid() && UniqueIdFromString( &dest, pBuf, nTextLen );
	}

	buf.Get( dest );
	return buf.IsValid();
}
