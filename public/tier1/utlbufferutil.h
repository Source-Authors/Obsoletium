//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
// Utilities for serialization/unserialization buffer
//=============================================================================//

#ifndef UTLBUFFERUTIL_H
#define UTLBUFFERUTIL_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"
#include "tier1/utlbuffer.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class Vector2D;
class Vector;
class Vector4D;
class QAngle;
class Quaternion;
class VMatrix;
class Color;
class CUtlBinaryBlock;
class CUtlString;
class CUtlCharConversion;

	
//-----------------------------------------------------------------------------
// For string serialization, set the delimiter rules
//-----------------------------------------------------------------------------
void SetSerializationDelimiter( CUtlCharConversion *pConv );
void SetSerializationArrayDelimiter( const char *pDelimiter );


//-----------------------------------------------------------------------------
// Standard serialization methods for basic types
//-----------------------------------------------------------------------------
[[nodiscard]] bool Serialize( CUtlBuffer &buf, const bool &src );
[[nodiscard]] bool Unserialize( CUtlBuffer &buf, bool &dest );

[[nodiscard]] bool Serialize( CUtlBuffer &buf, const int &src );
[[nodiscard]] bool Unserialize( CUtlBuffer &buf, int &dest );

[[nodiscard]] bool Serialize( CUtlBuffer &buf, const float &src );
[[nodiscard]] bool Unserialize( CUtlBuffer &buf, float &dest );

[[nodiscard]] bool Serialize( CUtlBuffer &buf, const Vector2D &src );
[[nodiscard]] bool Unserialize( CUtlBuffer &buf, Vector2D &dest );

[[nodiscard]] bool Serialize( CUtlBuffer &buf, const Vector &src );
[[nodiscard]] bool Unserialize( CUtlBuffer &buf, Vector &dest );

[[nodiscard]] bool Serialize( CUtlBuffer &buf, const Vector4D &src );
[[nodiscard]] bool Unserialize( CUtlBuffer &buf, Vector4D &dest );

[[nodiscard]] bool Serialize( CUtlBuffer &buf, const QAngle &src );
[[nodiscard]] bool Unserialize( CUtlBuffer &buf, QAngle &dest );

[[nodiscard]] bool Serialize( CUtlBuffer &buf, const Quaternion &src );
[[nodiscard]] bool Unserialize( CUtlBuffer &buf, Quaternion &dest );

[[nodiscard]] bool Serialize( CUtlBuffer &buf, const VMatrix &src );
[[nodiscard]] bool Unserialize( CUtlBuffer &buf, VMatrix &dest );

[[nodiscard]] bool Serialize( CUtlBuffer &buf, const Color &src );
[[nodiscard]] bool Unserialize( CUtlBuffer &buf, Color &dest );

[[nodiscard]] bool Serialize( CUtlBuffer &buf, const CUtlBinaryBlock &src );
[[nodiscard]] bool Unserialize( CUtlBuffer &buf, CUtlBinaryBlock &dest );

[[nodiscard]] bool Serialize( CUtlBuffer &buf, const CUtlString &src );
[[nodiscard]] bool Unserialize( CUtlBuffer &buf, CUtlString &dest );


//-----------------------------------------------------------------------------
// You can use this to check if a type serializes on multiple lines
//-----------------------------------------------------------------------------
template< class T >
[[nodiscard]] inline bool SerializesOnMultipleLines()
{
	return false;
}

template< >
[[nodiscard]] inline bool SerializesOnMultipleLines<VMatrix>()
{
	return true;
}

template< >
[[nodiscard]] inline bool SerializesOnMultipleLines<CUtlBinaryBlock>()
{
	return true;
}


//-----------------------------------------------------------------------------
// Vector serialization
//-----------------------------------------------------------------------------
template< class T >
[[nodiscard]] bool Serialize( CUtlBuffer &buf, const CUtlVector<T> &src )
{
	extern const char *s_pUtlBufferUtilArrayDelim;

	intp nCount = src.Count();

	if ( !buf.IsText() )
	{
		bool ok = true;

		buf.PutInt( nCount );
		for ( intp i = 0; i < nCount; ++i )
		{
			if ( !::Serialize( buf, src[i] ) )
			{
				ok = false;
			}
		}

		return ok && buf.IsValid();
	}
	
	bool ok = true;

	if ( !SerializesOnMultipleLines<T>() )
	{
		buf.PutChar('\n');
		for ( intp i = 0; i < nCount; ++i )
		{
			if ( !::Serialize( buf, src[i] ) )
			{
				ok = false;
			}

			if ( s_pUtlBufferUtilArrayDelim && (i != nCount-1) )
			{
				buf.PutString( s_pUtlBufferUtilArrayDelim );
			}
			buf.PutChar('\n');
		}
	}
	else
	{
		for ( intp i = 0; i < nCount; ++i )
		{
			if ( !::Serialize( buf, src[i] ) )
			{
				ok = false;
			}

			if ( s_pUtlBufferUtilArrayDelim && (i != nCount-1) )
			{
				buf.PutString( s_pUtlBufferUtilArrayDelim );
			}
			buf.PutChar(' ');
		}
	}

	return ok && buf.IsValid();
}

template< class T >
[[nodiscard]] bool Unserialize( CUtlBuffer &buf, CUtlVector<T> &dest )
{
	dest.RemoveAll();

	MEM_ALLOC_CREDIT_FUNCTION();

	if ( !buf.IsText() )
	{
		int nCount = buf.GetInt();
		if ( nCount )
		{
			dest.EnsureCapacity( nCount );
			for ( int i = 0; i < nCount; ++i )
			{
				VerifyEquals( dest.AddToTail(), i );
				if ( !::Unserialize( buf, dest[i] ) )
					return false;
			}
		}
		return buf.IsValid();
	}

	while ( true )
	{
		buf.EatWhiteSpace();
		if ( !buf.IsValid() )
			break;

		intp i = dest.AddToTail( );
		if ( ! ::Unserialize( buf, dest[i] ) )
			return false;
	}
	return true;
}


#endif // UTLBUFFERUTIL_H

