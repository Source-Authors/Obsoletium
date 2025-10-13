//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "tier1/utlstring.h"

#define __STDC_LIMIT_MACROS
#include <cstdint>
#include <cctype>

#include "tier1/strtools.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Simple string class. 
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Either allocates or reallocates memory to the length
//
// Allocated space for length characters.  It automatically adds space for the 
// nul and the cached length at the start of the memory block.  Will adjust
// m_pString and explicitly set the nul at the end before returning.
void *CUtlString::AllocMemory( size_t length )
{
	char *pMemoryBlock;
	if ( m_pString )
	{
		pMemoryBlock = static_cast<char*>(realloc( m_pString, length + 1 ));
	}
	else
	{
		pMemoryBlock = static_cast<char*>(malloc( length + 1 ));
	}
	m_pString = pMemoryBlock;
	m_pString[ length ] = '\0';

	return pMemoryBlock;
}

//-----------------------------------------------------------------------------
void CUtlString::SetDirect( const char *pValue, intp nChars )
{
	if ( pValue && nChars > 0 )
	{
		if ( pValue == m_pString )
		{
			AssertMsg( nChars == Q_strlen(m_pString), "CUtlString::SetDirect does not support resizing strings in place." );
			return; // Do nothing. Realloc in AllocMemory might move pValue's location resulting in a bad memcpy.
		}

		Assert( nChars <= Min<intp>( strnlen(pValue, nChars) + 1, nChars ) );
		AllocMemory( nChars );
		Q_memcpy( m_pString, pValue, nChars );
	}
	else
	{
		Purge();
	}
}


void CUtlString::Set( const char *pValue )
{
	intp length = pValue ? V_strlen( pValue ) : 0;
	SetDirect( pValue, length );
}

// Sets the length (used to serialize into the buffer )
void CUtlString::SetLength( intp nLen )
{
	if ( nLen > 0 )
	{
#ifdef _DEBUG
		intp prevLen = m_pString ? Length() : 0;
#endif
		AllocMemory( nLen );
#ifdef _DEBUG
		if ( nLen > prevLen )
		{
			V_memset( m_pString + prevLen, 0xEB, nLen - prevLen );
		}
#endif
	}
	else
	{
		Purge();
	}
}

const char *CUtlString::Get( ) const
{
	if (!m_pString)
	{
		return "";
	}
	return m_pString;
}

char *CUtlString::GetForModify()
{
	if ( !m_pString )
	{
		// In general, we optimise away small mallocs for empty strings
		// but if you ask for the non-const bytes, they must be writable
		// so we can't return "" here, like we do for the const version - jd
		char *pMemoryBlock = static_cast<char*>(malloc( 1 ));
		m_pString = pMemoryBlock;
		*m_pString = '\0';
	}

	return m_pString;
}

char CUtlString::operator[]( intp i ) const
{
	if ( !m_pString )
		return '\0';

	if ( i >= Length() )
	{
		return '\0';
	}

	return m_pString[i];
}

void CUtlString::Clear()
{
	Purge();
}

void CUtlString::Purge()
{
    free( m_pString );
    m_pString = nullptr;
}

bool CUtlString::IsEqual_CaseSensitive( const char *src ) const
{
	if ( !src )
	{
		return (Length() == 0);
	}
	return ( V_strcmp( Get(), src ) == 0 );
}

bool CUtlString::IsEqual_CaseInsensitive( const char *src ) const
{
	if ( !src )
	{
		return (Length() == 0);
	}
	return ( V_stricmp( Get(), src ) == 0 );
}


void CUtlString::ToLower()
{
	if ( !m_pString )
	{
		return;
	}

	V_strlower( m_pString );
}

void CUtlString::ToUpper()
{
	if ( !m_pString )
	{
		return;
	}

	V_strupr( m_pString );
}

CUtlString &CUtlString::operator=( const CUtlString &src )
{
	SetDirect( src.Get(), src.Length() );
	return *this;
}

CUtlString &CUtlString::operator=( const char *src )
{
	Set( src );
	return *this;
}

bool CUtlString::operator==( const CUtlString &src ) const
{
	if ( IsEmpty() )
	{
		if ( src.IsEmpty() )
		{
			return true;
		}

		return false;
	}
	else
	{
		if ( src.IsEmpty() )
		{
			return false;
		}

		// dimhotepus: Speedup equality check by directly comparing pointers.
		return m_pString == src.m_pString || Q_strcmp(m_pString, src.m_pString) == 0;
	}
}

CUtlString &CUtlString::operator+=( const CUtlString &rhs )
{
	const intp lhsLength( Length() );
	const intp rhsLength( rhs.Length() );

	if (!rhsLength)
	{
		return *this;
	}

	const intp requestedLength( lhsLength + rhsLength );

	AllocMemory( requestedLength );
	Q_memcpy( m_pString + lhsLength, rhs.m_pString, rhsLength );

	return *this;
}

CUtlString &CUtlString::operator+=( const char *rhs )
{
	const intp lhsLength( Length() );
	const intp rhsLength( V_strlen( rhs ) );
	const intp requestedLength( lhsLength + rhsLength );

	if (!requestedLength)
	{
		return *this;
	}

	AllocMemory( requestedLength );
	Q_memcpy( m_pString + lhsLength, rhs, rhsLength );

	return *this;
}

CUtlString &CUtlString::operator+=( char c )
{
	const intp lhsLength( Length() );

	AllocMemory( lhsLength + 1 );
	m_pString[ lhsLength ] = c;

	return *this;
}

CUtlString &CUtlString::operator+=( int rhs )
{
	Assert( sizeof( rhs ) == 4 );

	char tmpBuf[ 16 ];	// Sufficient for a signed 32 bit integer [ -2147483648 to +2147483647 ]
	V_to_chars( tmpBuf, rhs );
	tmpBuf[ sizeof( tmpBuf ) - 1 ] = '\0';

	return operator+=( tmpBuf );
}

CUtlString &CUtlString::operator+=( double rhs )
{
	char tmpBuf[ 64 ];	// How big can doubles be???  Dunno.
	V_snprintf( tmpBuf, sizeof( tmpBuf ), "%lg", rhs );
	tmpBuf[ sizeof( tmpBuf ) - 1 ] = '\0';

	return operator+=( tmpBuf );
}

bool CUtlString::MatchesPattern( const CUtlString &Pattern, int nFlags ) const
{
	const char *pszSource = String();
	const char *pszPattern = Pattern.String();
	bool	bExact = true;

	while( true )
	{
		if ( ( *pszPattern ) == '\0' )
		{
			return ( (*pszSource ) == '\0' );
		}

		if ( ( *pszPattern ) == '*' )
		{
			pszPattern++;

			if ( ( *pszPattern ) == '\0' )
			{
				return true;
			}

			bExact = false;
			continue;
		}

		intp nLength = 0;

		while( ( *pszPattern ) != '*' && ( *pszPattern ) != '\0' )
		{
			nLength++;
			pszPattern++;
		}

		while( true )
		{
			const char *pszStartPattern = pszPattern - nLength;
			const char *pszSearch = pszSource;

			for( intp i = 0; i < nLength; i++, pszSearch++, pszStartPattern++ )
			{
				if ( ( *pszSearch ) == 0 )
				{
					return false;
				}

				if ( ( *pszSearch ) != ( *pszStartPattern ) )
				{
					break;
				}
			}

			if ( pszSearch - pszSource == nLength )
			{
				break;
			}

			if ( bExact == true )
			{
				return false;
			}

			if ( ( nFlags & PATTERN_DIRECTORY ) != 0 )
			{
				if ( ( *pszPattern ) != '/' && ( *pszSource ) == '/' )
				{
					return false;
				}
			}

			pszSource++;
		}

		pszSource += nLength;
	}
}


int CUtlString::Format( PRINTF_FORMAT_STRING const char *pFormat, ... ) FMTFUNCTION( 2, 3 )
{
	va_list marker;

	va_start( marker, pFormat );
	int len = FormatV( pFormat, marker );
	va_end( marker );

	return len;
}

//--------------------------------------------------------------------------------------------------
// This can be called from functions that take varargs.
//--------------------------------------------------------------------------------------------------

int CUtlString::FormatV( PRINTF_FORMAT_STRING const char *pFormat, va_list marker )
{
	char tmpBuf[ 4096 ];	//< Nice big 4k buffer, as much memory as my first computer had, a Radio Shack Color Computer

	//va_start( marker, pFormat );
	int len = V_vsprintf_safe( tmpBuf, pFormat, marker );
	//va_end( marker );
	Set( tmpBuf );
	return len;
}

//-----------------------------------------------------------------------------
// Strips the trailing slash
//-----------------------------------------------------------------------------
void CUtlString::StripTrailingSlash()
{
	if ( IsEmpty() )
		return;

	intp nLastChar = Length() - 1;
	char c = m_pString[ nLastChar ];
	if ( c == '\\' || c == '/' )
	{
		SetLength( nLastChar );
	}
}

void CUtlString::FixSlashes( char cSeparator/*=CORRECT_PATH_SEPARATOR*/ )
{
	if ( m_pString )
	{
		V_FixSlashes( m_pString, cSeparator );
	}
}

//-----------------------------------------------------------------------------
// Trim functions
//-----------------------------------------------------------------------------
void CUtlString::TrimLeft( char cTarget )
{
	intp nIndex = 0;

	if ( IsEmpty() )
	{
		return;
	}

	while( m_pString[nIndex] == cTarget )
	{
		++nIndex;
	}

	// We have some whitespace to remove
	if ( nIndex > 0 )
	{
		memcpy( m_pString, &m_pString[nIndex], Length() - nIndex );
		SetLength( Length() - nIndex );
	}
}


void CUtlString::TrimLeft( const char *szTargets )
{
	intp i;

	if ( IsEmpty() )
	{
		return;
	}

	for( i = 0; m_pString[i] != 0; i++ )
	{
		bool bWhitespace = false;

		for( int j = 0; szTargets[j] != 0; j++ )
		{
			if ( m_pString[i] == szTargets[j] )
			{
				bWhitespace = true;
				break;
			}
		}

		if ( !bWhitespace )
		{
			break;
		}
	}

	// We have some whitespace to remove
	if ( i > 0 )
	{
		memcpy( m_pString, &m_pString[i], Length() - i );
		SetLength( Length() - i );
	}
}


void CUtlString::TrimRight( char cTarget )
{
	const intp nLastCharIndex = Length() - 1;
	intp nIndex = nLastCharIndex;

	while ( nIndex >= 0 && m_pString[nIndex] == cTarget )
	{
		--nIndex;
	}

	// We have some whitespace to remove
	if ( nIndex < nLastCharIndex )
	{
		m_pString[nIndex + 1] = 0;
		SetLength( nIndex + 2 );
	}
}


void CUtlString::TrimRight( const char *szTargets )
{
	const intp nLastCharIndex = Length() - 1;
	intp i;

	for( i = nLastCharIndex; i > 0; i-- )
	{
		bool bWhitespace = false;

		for( intp j = 0; szTargets[j] != 0; j++ )
		{
			if ( m_pString[i] == szTargets[j] )
			{
				bWhitespace = true;
				break;
			}
		}

		if ( !bWhitespace )
		{
			break;
		}
	}

	// We have some whitespace to remove
	if ( i < nLastCharIndex )
	{
		m_pString[i + 1] = 0;
		SetLength( i + 2 );
	}
}


void CUtlString::Trim( char cTarget )
{
	TrimLeft( cTarget );
	TrimRight( cTarget );
}


void CUtlString::Trim( const char *szTargets )
{
	TrimLeft( szTargets );
	TrimRight( szTargets );
}


CUtlString CUtlString::Slice( intp nStart, intp nEnd ) const
{
	intp length = Length();
	if ( length == 0 )
	{
		return {};
	}

	if ( nStart < 0 )
		nStart = length - (-nStart % length);
	else if ( nStart >= length )
		nStart = length;

	if ( nEnd == PTRDIFF_MAX )
		nEnd = length;
	else if ( nEnd < 0 )
		nEnd = length - (-nEnd % length);
	else if ( nEnd >= length )
		nEnd = length;
	
	if ( nStart >= nEnd )
		return {};

	const char *pIn = String();

	CUtlString ret;
	ret.SetDirect( pIn + nStart, nEnd - nStart );
	return ret;
}

// Grab a substring starting from the left or the right side.
CUtlString CUtlString::Left( intp nChars ) const
{
	return Slice( 0, nChars );
}

CUtlString CUtlString::Right( intp nChars ) const
{
	return Slice( -nChars );
}

CUtlString CUtlString::Replace( char cFrom, char cTo ) const
{
	if (!m_pString)
	{
		return {};
	}

	CUtlString ret = *this;
	intp len = ret.Length();
	for ( intp i=0; i < len; i++ )
	{
		if ( ret.m_pString[i] == cFrom )
			ret.m_pString[i] = cTo;
	}

	return ret;
}

CUtlString CUtlString::Replace( const char *pszFrom, const char *pszTo ) const
{
	Assert( pszTo ); // Can be 0 length, but not null
	Assert( pszFrom && *pszFrom ); // Must be valid and have one character.
	
	
	const char *pos = V_strstr( String(), pszFrom );
	if ( !pos )
	{
		return *this;
	}

	const char *pFirstFound = pos;

	// count number of search string
	intp nSearchCount = 0;
	intp nSearchLength = V_strlen( pszFrom );
	while ( pos )
	{
		nSearchCount++;
		intp nSrcOffset = ( pos - String() ) + nSearchLength;
		pos = V_strstr( String() + nSrcOffset, pszFrom );
	}

	// allocate the new string
	intp nReplaceLength = V_strlen( pszTo );
	intp nAllocOffset = nSearchCount * ( nReplaceLength - nSearchLength );
	size_t srcLength = Length();
	CUtlString strDest;
	size_t destLength = srcLength + nAllocOffset;
	strDest.SetLength( destLength );

	// find and replace the search string
	pos = pFirstFound;
	intp nDestOffset = 0;
	intp nSrcOffset = 0;
	while ( pos )
	{
		// Found an instance
		intp nCurrentSearchOffset = pos - String();
		intp nCopyLength = nCurrentSearchOffset - nSrcOffset;
		V_strncpy( strDest.GetForModify() + nDestOffset, String() + nSrcOffset, nCopyLength + 1 );
		nDestOffset += nCopyLength;
		V_strncpy( strDest.GetForModify() + nDestOffset, pszTo, nReplaceLength + 1 );
		nDestOffset += nReplaceLength;

		nSrcOffset = nCurrentSearchOffset + nSearchLength;
		pos = V_strstr( String() + nSrcOffset, pszFrom );
	}

	// making sure that the left over string from the source is the same size as the left over dest buffer
	Assert( destLength - nDestOffset == srcLength - nSrcOffset );
	if ( destLength - nDestOffset > 0 )
	{
		V_strncpy( strDest.GetForModify() + nDestOffset, String() + nSrcOffset, destLength - nDestOffset + 1 );
	}

	return strDest;
}

CUtlString CUtlString::AbsPath( const char *pStartingDir ) const
{
	char szNew[MAX_PATH];
	V_MakeAbsolutePath( szNew, this->String(), pStartingDir );
	return { szNew };
}

CUtlString CUtlString::UnqualifiedFilename() const
{
	const char *pFilename = V_UnqualifiedFileName( this->String() );
	return { pFilename };
}

CUtlString CUtlString::DirName() const
{
	CUtlString ret( this->String() );
	V_StripLastDir( ret.GetForModify(), ret.Length() + 1 );
	V_StripTrailingSlash( ret.GetForModify() );
	return ret;
}

CUtlString CUtlString::StripExtension() const
{
	char szTemp[MAX_PATH];
	V_StripExtension( String(), szTemp );
	return { szTemp };
}

CUtlString CUtlString::StripFilename() const
{
	const char *pFilename = V_UnqualifiedFileName( Get() ); // NOTE: returns 'Get()' on failure, never NULL
	intp nCharsToCopy = pFilename - Get();
	CUtlString result;
	result.SetDirect( Get(), nCharsToCopy );
	result.StripTrailingSlash();
	return result;
}

CUtlString CUtlString::GetBaseFilename() const
{
	char szTemp[MAX_PATH];
	V_FileBase( String(), szTemp );
	return { szTemp };
}

CUtlString CUtlString::GetExtension() const
{
	char szTemp[MAX_PATH];
	V_ExtractFileExtension( String(), szTemp );
	return { szTemp };
}


CUtlString CUtlString::PathJoin( const char *pStr1, const char *pStr2 )
{
	char szPath[MAX_PATH];
	V_ComposeFileName( pStr1, pStr2, szPath );
	return { szPath };
}

CUtlString CUtlString::operator+( const char *pOther ) const
{
	CUtlString s = *this;
	s += pOther;
	return s;
}

CUtlString CUtlString::operator+( const CUtlString &other ) const
{
	CUtlString s = *this;
	s += other;
	return s;
}

CUtlString CUtlString::operator+( int rhs ) const
{
	CUtlString ret = *this;
	ret += rhs;
	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: concatenate the provided string to our current content
//-----------------------------------------------------------------------------
void CUtlString::Append( const char *pchAddition )
{
	(*this) += pchAddition;
}

void CUtlString::Append( const char *pchAddition, intp nChars )
{
	nChars = Min( nChars, V_strlen(	pchAddition ) );

	const intp lhsLength( Length() );
	const intp rhsLength( nChars );
	const intp requestedLength( lhsLength + rhsLength );

	AllocMemory( requestedLength );
	const intp allocatedLength( requestedLength );
	const intp copyLength( allocatedLength - lhsLength < rhsLength ? allocatedLength - lhsLength : rhsLength );
	memcpy( GetForModify() + lhsLength, pchAddition, copyLength );
	m_pString[ allocatedLength ] = '\0';
}

// Shared static empty string.
const CUtlString &CUtlString::GetEmptyString()
{
	static const CUtlString s_emptyString;
	return s_emptyString;
}
