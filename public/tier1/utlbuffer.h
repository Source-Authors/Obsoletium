//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
// Serialization/unserialization buffer
//=============================================================================//

#ifndef UTLBUFFER_H
#define UTLBUFFER_H

#include "utlmemory.h"
#include "byteswap.h"

#include <cstdarg>


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct characterset_t;

	
//-----------------------------------------------------------------------------
// Description of character conversions for string output
// Here's an example of how to use the macros to define a character conversion
// BEGIN_CHAR_CONVERSION( CStringConversion, '\\' )
//	{ '\n', "n" },
//	{ '\t', "t" }
// END_CHAR_CONVERSION( CStringConversion, '\\' )
//-----------------------------------------------------------------------------
class CUtlCharConversion
{
public:
	struct ConversionArray_t
	{
		char m_nActualChar;
		const char *m_pReplacementString;
	};

	CUtlCharConversion( char nEscapeChar, const char *pDelimiter, intp nCount, ConversionArray_t *pArray );
	virtual ~CUtlCharConversion() = default;

	[[nodiscard]] char GetEscapeChar() const;
	[[nodiscard]] const char *GetDelimiter() const;
	[[nodiscard]] intp GetDelimiterLength() const;

	[[nodiscard]] const char *GetConversionString( char c ) const;
	[[nodiscard]] intp GetConversionLength( char c ) const;
	[[nodiscard]] intp MaxConversionLength() const;

	// Finds a conversion for the passed-in string, returns length
	virtual char FindConversion( const char *pString, intp *pLength ) const;

protected:
	struct ConversionInfo_t
	{
		intp m_nLength;
		const char *m_pReplacementString;
	};

	char m_nEscapeChar;
	const char *m_pDelimiter;
	intp m_nDelimiterLength;
	intp m_nCount;
	intp m_nMaxConversionLength;
	char m_pList[256];
	ConversionInfo_t m_pReplacements[256];
};

#define BEGIN_CHAR_CONVERSION( _name, _delimiter, _escapeChar )	\
	static CUtlCharConversion::ConversionArray_t s_pConversionArray ## _name[] = {

#define END_CHAR_CONVERSION( _name, _delimiter, _escapeChar ) \
	}; \
	CUtlCharConversion _name( _escapeChar, _delimiter, sizeof( s_pConversionArray ## _name ) / sizeof( CUtlCharConversion::ConversionArray_t ), s_pConversionArray ## _name );

#define BEGIN_CUSTOM_CHAR_CONVERSION( _className, _name, _delimiter, _escapeChar ) \
	static CUtlCharConversion::ConversionArray_t s_pConversionArray ## _name[] = {

#define END_CUSTOM_CHAR_CONVERSION( _className, _name, _delimiter, _escapeChar ) \
	}; \
	_className _name( _escapeChar, _delimiter, sizeof( s_pConversionArray ## _name ) / sizeof( CUtlCharConversion::ConversionArray_t ), s_pConversionArray ## _name );

//-----------------------------------------------------------------------------
// Character conversions for C strings
//-----------------------------------------------------------------------------
[[nodiscard]] CUtlCharConversion *GetCStringCharConversion();

//-----------------------------------------------------------------------------
// Character conversions for quoted strings, with no escape sequences
//-----------------------------------------------------------------------------
[[nodiscard]] CUtlCharConversion *GetNoEscCharConversion();


//-----------------------------------------------------------------------------
// Macro to set overflow functions easily
//-----------------------------------------------------------------------------
#define SetUtlBufferOverflowFuncs( _get, _put )	\
	SetOverflowFuncs( static_cast <UtlBufferOverflowFunc_t>( _get ), static_cast <UtlBufferOverflowFunc_t>( _put ) )


//-----------------------------------------------------------------------------
// Command parsing..
//-----------------------------------------------------------------------------
class CUtlBuffer
{
public:
	enum SeekType_t
	{
		SEEK_HEAD = 0,
		SEEK_CURRENT,
		SEEK_TAIL
	};

	// flags
	enum BufferFlags_t : unsigned char
	{
		TEXT_BUFFER = 0x1,			// Describes how get + put work (as strings, or binary)
		EXTERNAL_GROWABLE = 0x2,	// This is used w/ external buffers and causes the utlbuf to switch to reallocatable memory if an overflow happens when Putting.
		CONTAINS_CRLF = 0x4,		// For text buffers only, does this contain \n or \n\r?
		READ_ONLY = 0x8,			// For external buffers; prevents null termination from happening.
		AUTO_TABS_DISABLED = 0x10,	// Used to disable/enable push/pop tabs
	};

	// Overflow functions when a get or put overflows
	using UtlBufferOverflowFunc_t = bool (CUtlBuffer::*)( intp nSize );

	// Constructors for growable + external buffers for serialization/unserialization
	CUtlBuffer( intp growSize = 0, intp initSize = 0, int nFlags = 0 );
	CUtlBuffer( const void* pBuffer, intp size, int nFlags = 0 );
	// This one isn't actually defined so that we catch contructors that are trying to pass a bool in as the third param.
	CUtlBuffer( const void *pBuffer, intp size, bool crap ) = delete;

	[[nodiscard]] unsigned char	GetFlags() const;

	// NOTE: This will assert if you attempt to recast it in a way that
	// is not compatible. The only valid conversion is binary-> text w/CRLF
	void			SetBufferType( bool bIsText, bool bContainsCRLF );

	// Makes sure we've got at least this much memory
	void			EnsureCapacity( intp num );

	// Attaches the buffer to external memory....
	void			SetExternalBuffer( void* pMemory, intp nSize, intp nInitialPut, int nFlags = 0 );
	[[nodiscard]] bool			IsExternallyAllocated() const;
	// Takes ownership of the passed memory, including freeing it when this buffer is destroyed.
	void			AssumeMemory( void *pMemory, intp nSize, intp nInitialPut, int nFlags = 0 );

	// copies data from another buffer
	void			CopyBuffer( const CUtlBuffer &buffer );
	void			CopyBuffer( const void *pubData, intp cubData );

	void			Swap( CUtlBuffer &buf );
	void			Swap( CUtlMemory<uint8> &mem );

	FORCEINLINE void ActivateByteSwappingIfBigEndian()
	{
	}


	// Controls endian-ness of binary utlbufs - default matches the current platform
	void			ActivateByteSwapping( bool bActivate );
	void			SetBigEndian( bool bigEndian );
	bool			IsBigEndian();

	// Resets the buffer; but doesn't free memory
	void			Clear();

	// Clears out the buffer; frees memory
	void			Purge();

	// Read stuff out.
	// Binary mode: it'll just read the bits directly in, and characters will be
	//		read for strings until a null character is reached.
	// Text mode: it'll parse the file, turning text #s into real numbers.
	//		GetString will read a string until a space is reached
	[[nodiscard]] char				GetChar( );
	[[nodiscard]] unsigned char		GetUnsignedChar( );
	[[nodiscard]] short				GetShort( );
	[[nodiscard]] unsigned short	GetUnsignedShort( );
	[[nodiscard]] int				GetInt( );
	[[nodiscard]] int64				GetInt64( );
	[[nodiscard]] int				GetIntHex( );
	[[nodiscard]] unsigned int		GetUnsignedInt( );
	[[nodiscard]] uint64			GetUint64( );
	[[nodiscard]] float				GetFloat( );
	[[nodiscard]] double			GetDouble( );
	// dimhotepus: CS:GO backport.
	[[nodiscard]] void *			GetPtr();
	template <size_t maxLenInChars> void GetString( OUT_Z_ARRAY char( &pString )[maxLenInChars] )
	{
		GetStringInternal( pString, maxLenInChars );
	}

	void GetStringManualCharCount( OUT_Z_CAP(maxLenInChars) char *pString, size_t maxLenInChars )
	{
		GetStringInternal( pString, maxLenInChars );
	}

	bool Get( OUT_BYTECAP(size) void* pMem, intp size );
	template<typename T>
	std::enable_if_t<!std::is_pointer_v<T>, bool> Get( T &mem )
	{
		return Get( &mem, static_cast<intp>(sizeof(T)) ); //-V2002
	}
	template<typename T, intp size>
	bool Get( T (&mem)[size] )
	{
		return Get( &mem, static_cast<intp>(sizeof(T)) * size ); //-V2002
	}
	void GetLine( char* pLine, intp nMaxChars = 0 );
	template<intp maxSize>
	void GetLine( char (&pLine)[maxSize] )
	{
		GetLine( pLine, maxSize );
	}

	// Used for getting objects that have a byteswap datadesc defined
	template <typename T> void GetObjects( T *dest, intp count = 1 );

	// This will get at least 1 byte and up to nSize bytes. 
	// It will return the number of bytes actually read.
	intp				GetUpTo( void *pMem, intp nSize );

	// This version of GetString converts \" to \\ and " to \, etc.
	// It also reads a " at the beginning and end of the string
	void			GetDelimitedString( CUtlCharConversion *pConv, char *pString, intp nMaxChars = 0 );
	[[nodiscard]] char			GetDelimitedChar( CUtlCharConversion *pConv );

	// This will return the # of characters of the string about to be read out
	// NOTE: The count will *include* the terminating 0!!
	// In binary mode, it's the number of characters until the next 0
	// In text mode, it's the number of characters until the next space.
	[[nodiscard]] intp				PeekStringLength();

	// This version of PeekStringLength converts \" to \\ and " to \, etc.
	// It also reads a " at the beginning and end of the string
	// NOTE: The count will *include* the terminating 0!!
	// In binary mode, it's the number of characters until the next 0
	// In text mode, it's the number of characters between "s (checking for \")
	// Specifying false for bActualSize will return the pre-translated number of characters
	// including the delimiters and the escape characters. So, \n counts as 2 characters when bActualSize == false
	// and only 1 character when bActualSize == true
	[[nodiscard]] intp				PeekDelimitedStringLength( CUtlCharConversion *pConv, bool bActualSize = true );

	// Just like scanf, but doesn't work in binary mode
	intp				Scanf( SCANF_FORMAT_STRING const char* pFmt, ... );
	intp				VaScanf( const char* pFmt, va_list list );

	// Eats white space, advances Get index
	void			EatWhiteSpace();

	// Eats C++ style comments
	bool			EatCPPComment();

	// (For text buffers only)
	// Parse a token from the buffer:
	// Grab all text that lies between a starting delimiter + ending delimiter
	// (skipping whitespace that leads + trails both delimiters).
	// If successful, the get index is advanced and the function returns true,
	// otherwise the index is not advanced and the function returns false.
	[[nodiscard]] bool			ParseToken( const char *pStartingDelim, const char *pEndingDelim, char* pString, intp nMaxLen );

	// (For text buffers only)
	// Parse a token from the buffer:
	// Grab all text that lies between a starting delimiter + ending delimiter
	// (skipping whitespace that leads + trails both delimiters).
	// If successful, the get index is advanced and the function returns true,
	// otherwise the index is not advanced and the function returns false.
	template<intp size>
	[[nodiscard]] bool ParseToken( const char *pStartingDelim, const char *pEndingDelim, char (&pString)[size] )
	{
		return ParseToken( pStartingDelim, pEndingDelim, pString, size );
	}

	// Advance the get index until after the particular string is found
	// Do not eat whitespace before starting. Return false if it failed
	// String test is case-insensitive.
	[[nodiscard]] bool			GetToken( const char *pToken );

	// Parses the next token, given a set of character breaks to stop at
	// Returns the length of the token parsed in bytes (-1 if none parsed)
	[[nodiscard]] intp			ParseToken( const characterset_t *pBreaks, char *pTokenBuf, intp nMaxLen, bool bParseComments = true );

	// Parses the next token, given a set of character breaks to stop at
	// Returns the length of the token parsed in bytes (-1 if none parsed)
	template<intp size>
	[[nodiscard]] intp ParseToken( const characterset_t *pBreaks, char (&pTokenBuf)[size], bool bParseComments = true )
	{
		return ParseToken( pBreaks, pTokenBuf, size, bParseComments );
	}

	// Write stuff in
	// Binary mode: it'll just write the bits directly in, and strings will be
	//		written with a null terminating character
	// Text mode: it'll convert the numbers to text versions
	//		PutString will not write a terminating character
	void			PutChar( char c );
	void			PutUnsignedChar( unsigned char uc );
	void			PutUint64( uint64 ub );
	void			PutInt16( int16 s16 );
	void			PutShort( short s );
	void			PutUnsignedShort( unsigned short us );
	void			PutInt( int i );
	void			PutInt64( int64 i );
	void			PutUnsignedInt( unsigned int u );
	void			PutFloat( float f );
	void			PutDouble( double d );
	void			PutString( const char* pString );
	// dimhotepus: CS:GO backport.
	void			PutPtr( void * ); // Writes the pointer, not the pointed to
	void			Put( const void* pMem, intp size );
	template<typename T> 
	std::enable_if_t<!std::is_pointer_v<T>> Put( const T &pMem )
	{
		Put( &pMem, static_cast<intp>( sizeof(pMem) ) ); //-V2002
	}

	// Used for putting objects that have a byteswap datadesc defined
	template <typename T> void PutObjects( T *src, intp count = 1 );

	// This version of PutString converts \ to \\ and " to \", etc.
	// It also places " at the beginning and end of the string
	void			PutDelimitedString( CUtlCharConversion *pConv, const char *pString );
	void			PutDelimitedChar( CUtlCharConversion *pConv, char c );

	// Just like printf, writes a terminating zero in binary mode
	void			Printf( PRINTF_FORMAT_STRING const char* pFmt, ... ) FMTFUNCTION( 2, 3 );
	void			VaPrintf( const char* pFmt, va_list list );

	// What am I writing (put)/reading (get)?
	[[nodiscard]] void* PeekPut( intp offset = 0 );
	[[nodiscard]] const void* PeekGet( intp offset = 0 ) const;
	[[nodiscard]] const void* PeekGet( intp nMaxSize, intp nOffset );

	// Where am I writing (put)/reading (get)?
	[[nodiscard]] intp TellPut( ) const;
	[[nodiscard]] intp TellGet( ) const;

	// What's the most I've ever written?
	[[nodiscard]] intp TellMaxPut( ) const;

	// How many bytes remain to be read?
	// NOTE: This is not accurate for streaming text files; it overshoots
	[[nodiscard]] intp GetBytesRemaining() const;

	// Change where I'm writing (put)/reading (get)
	void SeekPut( SeekType_t type, intp offset );
	void SeekGet( SeekType_t type, intp offset );

	// Buffer base
	[[nodiscard]] const void* Base() const;
	[[nodiscard]] void* Base();
	template<typename T>
	[[nodiscard]] const T* Base() const { return static_cast<const T*>(Base()); }
	template<typename T>
	[[nodiscard]] T* Base() { return static_cast<T*>(Base()); }
	// Returns the base as a const char*, only valid in text mode.
	[[nodiscard]] const char *String() const;

	// memory allocation size, does *not* reflect size written or read,
	//	use TellPut or TellGet for that
	[[nodiscard]] intp Size() const;

	// Am I a text buffer?
	[[nodiscard]] bool IsText() const;

	// Can I grow if I'm externally allocated?
	[[nodiscard]] bool IsGrowable() const;

	// Am I valid? (overflow or underflow error), Once invalid it stays invalid
	[[nodiscard]] bool IsValid() const;

	// Do I contain carriage return/linefeeds? 
	[[nodiscard]] bool ContainsCRLF() const;

	// Am I read-only
	[[nodiscard]] bool IsReadOnly() const;

	// Converts a buffer from a CRLF buffer to a CR buffer (and back)
	// Returns false if no conversion was necessary (and outBuf is left untouched)
	// If the conversion occurs, outBuf will be cleared.
	bool ConvertCRLF( CUtlBuffer &outBuf );

	// Push/pop pretty-printing tabs
	void PushTab();
	void PopTab();

	// Temporarily disables pretty print
	void EnableTabs( bool bEnable );

protected:
	// error flags
	enum
	{
		PUT_OVERFLOW = 0x1,
		GET_OVERFLOW = 0x2,
		MAX_ERROR_FLAG = GET_OVERFLOW,
	};

	void SetOverflowFuncs( UtlBufferOverflowFunc_t getFunc, UtlBufferOverflowFunc_t putFunc );

	[[nodiscard]] bool OnPutOverflow( intp nSize );
	[[nodiscard]] bool OnGetOverflow( intp nSize );

protected:
	// Checks if a get/put is ok
	[[nodiscard]] bool CheckPut( intp size );
	[[nodiscard]] bool CheckGet( intp size );

	void AddNullTermination( );

	// Methods to help with pretty-printing
	[[nodiscard]] bool WasLastCharacterCR();
	void PutTabs();

	// Help with delimited stuff
	[[nodiscard]] char GetDelimitedCharInternal( CUtlCharConversion *pConv );
	void PutDelimitedCharInternal( CUtlCharConversion *pConv, char c );

	// Default overflow funcs
	[[nodiscard]] bool PutOverflow( intp nSize );
	[[nodiscard]] bool GetOverflow( intp nSize );

	// Does the next bytes of the buffer match a pattern?
	[[nodiscard]] bool PeekStringMatch( intp nOffset, const char *pString, intp nLen );

	// Peek size of line to come, check memory bound
	[[nodiscard]] intp	PeekLineLength();

	// How much whitespace should I skip?
	[[nodiscard]] intp PeekWhiteSpace( intp nOffset );

	// Checks if a peek get is ok
	[[nodiscard]] bool CheckPeekGet( intp nOffset, intp nSize );

	// Call this to peek arbitrarily long into memory. It doesn't fail unless
	// it can't read *anything* new
	[[nodiscard]] bool CheckArbitraryPeekGet( intp nOffset, intp &nIncrement );
	void GetStringInternal( OUT_Z_CAP(maxLenInChars) char *pString, size_t maxLenInChars );

	template <typename T> void GetType( T& dest, const char *pszFmt );
	template <typename T> void GetTypeBin( T& dest );
	template <typename T> void GetObject( T *src );

	template <typename T> void PutType( T src, const char *pszFmt );
	template <typename T> void PutTypeBin( T src );
	template <typename T> void PutObject( T *src );

	CUtlMemory<unsigned char> m_Memory;
	intp m_Get;
	intp m_Put;

	unsigned char m_Error;
	unsigned char m_Flags;
	unsigned char m_Reserved;  //-V730_NOINIT

	intp m_nTab;
	intp m_nMaxPut;
	intp m_nOffset;

	UtlBufferOverflowFunc_t m_GetOverflowFunc;
	UtlBufferOverflowFunc_t m_PutOverflowFunc;

	CByteswap	m_Byteswap;
};


// Stream style output operators for CUtlBuffer
inline CUtlBuffer &operator<<( CUtlBuffer &b, char v )
{
	b.PutChar( v );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, unsigned char v )
{
	b.PutUnsignedChar( v );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, short v )
{
	b.PutShort( v );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, unsigned short v )
{
	b.PutUnsignedShort( v );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, int v )
{
	b.PutInt( v );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, unsigned int v )
{
	b.PutUnsignedInt( v );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, float v )
{
	b.PutFloat( v );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, double v )
{
	b.PutDouble( v );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, const char *pv )
{
	b.PutString( pv );
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, const Vector &v )
{
	b << v.x << " " << v.y << " " << v.z;
	return b;
}

inline CUtlBuffer &operator<<( CUtlBuffer &b, const Vector2D &v )
{
	b << v.x << " " << v.y;
	return b;
}


class CUtlInplaceBuffer : public CUtlBuffer
{
public:
	CUtlInplaceBuffer( intp growSize = 0, intp initSize = 0, unsigned char nFlags = 0 );

	//
	// Routines returning buffer-inplace-pointers
	//
public:
	//
	// Upon success, determines the line length, fills out the pointer to the
	// beginning of the line and the line length, advances the "get" pointer
	// offset by the line length and returns "true".
	//
	// If end of file is reached or upon error returns "false".
	//
	// Note:	the returned length of the line is at least one character because the
	//			trailing newline characters are also included as part of the line.
	//
	// Note:	the pointer returned points into the local memory of this buffer, in
	//			case the buffer gets relocated or destroyed the pointer becomes invalid.
	//
	// e.g.:	-------------
	//
	//			char *pszLine;
	//			intp nLineLen;
	//			while ( pUtlInplaceBuffer->InplaceGetLinePtr( &pszLine, &nLineLen ) )
	//			{
	//				...
	//			}
	//
	//			-------------
	//
	// @param	ppszInBufferPtr		on return points into this buffer at start of line
	// @param	pnLineLength		on return holds num bytes accessible via (*ppszInBufferPtr)
	//
	// @returns	true				if line was successfully read
	//			false				when EOF is reached or error occurs
	//
	bool InplaceGetLinePtr( /* out */ char **ppszInBufferPtr, /* out */ intp *pnLineLength );

	//
	// Determines the line length, advances the "get" pointer offset by the line length,
	// replaces the newline character with null-terminator and returns the initial pointer
	// to now null-terminated line.
	//
	// If end of file is reached or upon error returns NULL.
	//
	// Note:	the pointer returned points into the local memory of this buffer, in
	//			case the buffer gets relocated or destroyed the pointer becomes invalid.
	//
	// e.g.:	-------------
	//
	//			while ( char *pszLine = pUtlInplaceBuffer->InplaceGetLinePtr() )
	//			{
	//				...
	//			}
	//
	//			-------------
	//
	// @returns	ptr-to-zero-terminated-line		if line was successfully read and buffer modified
	//			NULL							when EOF is reached or error occurs
	//
	[[nodiscard]] char * InplaceGetLinePtr();
};


//-----------------------------------------------------------------------------
// Where am I reading?
//-----------------------------------------------------------------------------
inline intp CUtlBuffer::TellGet() const
{
	return m_Get;
}


//-----------------------------------------------------------------------------
// How many bytes remain to be read?
//-----------------------------------------------------------------------------
inline intp CUtlBuffer::GetBytesRemaining() const
{
	return m_nMaxPut - TellGet();
}


//-----------------------------------------------------------------------------
// What am I reading?
//-----------------------------------------------------------------------------
inline const void* CUtlBuffer::PeekGet( intp offset ) const
{
	return &m_Memory[ m_Get + offset - m_nOffset ];
}


//-----------------------------------------------------------------------------
// Unserialization
//-----------------------------------------------------------------------------

template <typename T> 
inline void CUtlBuffer::GetObject( T *dest )
{
	if ( CheckGet( sizeof(T) ) )
	{
		if ( !m_Byteswap.IsSwappingBytes() || ( sizeof( T ) == 1 ) )
		{
			*dest = *(const T *)PeekGet();
		}
		else
		{
			m_Byteswap.SwapFieldsToTargetEndian<T>( dest, (const T*)PeekGet() );
		}
		m_Get += sizeof(T);	
	}
	else
	{
		Q_memset( dest, 0, sizeof(T) );
	}
}


template <typename T> 
inline void CUtlBuffer::GetObjects( T *dest, intp count )
{
	for ( intp i = 0; i < count; ++i, ++dest )
	{
		GetObject<T>( dest );
	}
}


template <typename T> 
inline void CUtlBuffer::GetTypeBin( T &dest )
{
	if ( CheckGet( sizeof(T) ) )
	{
		if ( !m_Byteswap.IsSwappingBytes() || ( sizeof( T ) == 1 ) )
		{
			dest = *(const T *)PeekGet();
		}
		else
		{
			m_Byteswap.SwapBufferToTargetEndian<T>( &dest, (const T*)PeekGet() );
		}
		m_Get += sizeof(T);
	}
	else
	{
		dest = 0;
	}
}

template <>
inline void CUtlBuffer::GetTypeBin< float >( float &dest )
{
	if ( CheckGet( sizeof( float ) ) )
	{
		auto pData = (uintptr_t)PeekGet();
		// aligned read
		dest = *(float *)pData;

		if ( m_Byteswap.IsSwappingBytes() )
		{
			m_Byteswap.SwapBufferToTargetEndian< float >( &dest, &dest );
		}
		m_Get += sizeof( float );
	}
	else
	{
		dest = 0;
	}
}

template <typename T> 
inline void CUtlBuffer::GetType( T &dest, const char *pszFmt )
{
	if (!IsText())
	{
		GetTypeBin( dest );
	}
	else
	{
		dest = 0;
		Scanf( pszFmt, &dest );
	}
}

inline char CUtlBuffer::GetChar( )
{
	char c;
	GetType( c, "%c" );
	return c;
}

inline unsigned char CUtlBuffer::GetUnsignedChar( )
{
	unsigned char c;
	GetType( c, "%u" );
	return c;
}

inline short CUtlBuffer::GetShort( )
{
	short s;
	GetType( s, "%d" );
	return s;
}

inline unsigned short CUtlBuffer::GetUnsignedShort( )
{
	unsigned short s;
	GetType( s, "%u" );
	return s;
}

inline int CUtlBuffer::GetInt( )
{
	int i;
	GetType( i, "%d" );
	return i;
}

inline int64 CUtlBuffer::GetInt64( )
{
	int64 i;
	GetType( i, "%lld" );
	return i;
}

inline int CUtlBuffer::GetIntHex( )
{
	int i;
	GetType( i, "%x" );
	return i;
}

inline unsigned int CUtlBuffer::GetUnsignedInt( )
{
	unsigned int u;
	GetType( u, "%u" );
	return u;
}

inline uint64 CUtlBuffer::GetUint64( )
{
	uint64 u;
	GetType( u, "%llu" );
	return u;
}

inline float CUtlBuffer::GetFloat( )
{
	float f;
	GetType( f, "%f" );
	return f;
}

inline double CUtlBuffer::GetDouble( )
{
	double d;
	GetType( d, "%f" );
	return d;
}

// dimhotepus: CS:GO backport.
inline void *CUtlBuffer::GetPtr( )
{
	void *p;
	// LEGACY WARNING: in text mode, PutPtr writes 32 bit pointers in hex, while GetPtr reads 32 or 64 bit pointers in decimal
#if !defined(X64BITS) && !defined(PLATFORM_64BITS)
	p = ( void* )(intp)GetUnsignedInt();
#else
	p = ( void* )(intp)GetInt64();
#endif
	return p;
}

//-----------------------------------------------------------------------------
// Where am I writing?
//-----------------------------------------------------------------------------
inline unsigned char CUtlBuffer::GetFlags() const
{ 
	return m_Flags; 
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
inline bool CUtlBuffer::IsExternallyAllocated() const
{ 
	return m_Memory.IsExternallyAllocated();
}

	
//-----------------------------------------------------------------------------
// Where am I writing?
//-----------------------------------------------------------------------------
inline intp CUtlBuffer::TellPut( ) const
{
	return m_Put;
}


//-----------------------------------------------------------------------------
// What's the most I've ever written?
//-----------------------------------------------------------------------------
inline intp CUtlBuffer::TellMaxPut( ) const
{
	return m_nMaxPut;
}


//-----------------------------------------------------------------------------
// What am I reading?
//-----------------------------------------------------------------------------
inline void* CUtlBuffer::PeekPut( intp offset )
{
	return &m_Memory[m_Put + offset - m_nOffset];
}


//-----------------------------------------------------------------------------
// Various put methods
//-----------------------------------------------------------------------------

template <typename T> 
inline void CUtlBuffer::PutObject( T *src )
{
	if ( CheckPut( sizeof(T) ) )
	{
		if ( !m_Byteswap.IsSwappingBytes() || ( sizeof( T ) == 1 ) )
		{
			*(T *)PeekPut() = *src;
		}
		else
		{
			m_Byteswap.SwapFieldsToTargetEndian<T>( (T*)PeekPut(), src );
		}
		m_Put += sizeof(T);
		AddNullTermination();
	}
}


template <typename T> 
inline void CUtlBuffer::PutObjects( T *src, intp count )
{
	for ( intp i = 0; i < count; ++i, ++src )
	{
		PutObject<T>( src );
	}
}


template <typename T> 
inline void CUtlBuffer::PutTypeBin( T src )
{
	if ( CheckPut( sizeof(T) ) )
	{
		if ( !m_Byteswap.IsSwappingBytes() || ( sizeof( T ) == 1 ) )
		{
			*(T *)PeekPut() = src;
		}
		else
		{
			m_Byteswap.SwapBufferToTargetEndian<T>( (T*)PeekPut(), &src );
		}
		m_Put += sizeof(T);
		AddNullTermination();
	}
}

template <typename T> 
inline void CUtlBuffer::PutType( T src, const char *pszFmt )
{
	if (!IsText())
	{
		PutTypeBin( src );
	}
	else
	{
		Printf( pszFmt, src );
	}
}

//-----------------------------------------------------------------------------
// Methods to help with pretty-printing
//-----------------------------------------------------------------------------
inline bool CUtlBuffer::WasLastCharacterCR()
{
	if ( !IsText() || (TellPut() == 0) )
		return false;
	return ( *( const char * )PeekPut( -1 ) == '\n' );
}

inline void CUtlBuffer::PutTabs()
{
	intp nTabCount = ( m_Flags & AUTO_TABS_DISABLED ) ? 0 : m_nTab;
	for (intp i = nTabCount; --i >= 0; )
	{
		PutTypeBin<char>( '\t' );
	}
}


//-----------------------------------------------------------------------------
// Push/pop pretty-printing tabs
//-----------------------------------------------------------------------------
inline void CUtlBuffer::PushTab( )
{
	++m_nTab;
}

inline void CUtlBuffer::PopTab()
{
	if ( --m_nTab < 0 )
	{
		m_nTab = 0;
	}
}


//-----------------------------------------------------------------------------
// Temporarily disables pretty print
//-----------------------------------------------------------------------------
inline void CUtlBuffer::EnableTabs( bool bEnable )
{
	if ( bEnable )
	{
		m_Flags &= ~AUTO_TABS_DISABLED;
	}
	else
	{
		m_Flags |= AUTO_TABS_DISABLED; 
	}
}

inline void CUtlBuffer::PutChar( char c )
{
	if ( WasLastCharacterCR() )
	{
		PutTabs();
	}

	PutTypeBin( c );
}

inline void CUtlBuffer::PutUnsignedChar( unsigned char c )
{
	PutType( c, "%u" );
}

inline void CUtlBuffer::PutUint64( uint64 ub )
{
	PutType( ub, "%llu" );
}

inline void CUtlBuffer::PutInt16( int16 s16 )
{
	PutType( s16, "%d" );
}

inline void  CUtlBuffer::PutShort( short s )
{
	PutType( s, "%d" );
}

inline void CUtlBuffer::PutUnsignedShort( unsigned short s )
{
	PutType( s, "%u" );
}

inline void CUtlBuffer::PutInt( int i )
{
	PutType( i, "%d" );
}

inline void CUtlBuffer::PutInt64( int64 i )
{
	PutType( i, "%llu" );
}

inline void CUtlBuffer::PutUnsignedInt( unsigned int u )
{
	PutType( u, "%u" );
}

inline void CUtlBuffer::PutFloat( float f )
{
	PutType( f, "%f" );
}

inline void CUtlBuffer::PutDouble( double d )
{
	PutType( d, "%f" );
}

// dimhotepus: CS:GO backport.
inline void CUtlBuffer::PutPtr( void *p )
{
	// LEGACY WARNING: in text mode, PutPtr writes 32 bit pointers in hex, while GetPtr reads 32 or 64 bit pointers in decimal
	if (!IsText())
	{
		PutTypeBin( p );
	}
	else
	{
		Printf( "0x%p", p );
	}
}

//-----------------------------------------------------------------------------
// Am I a text buffer?
//-----------------------------------------------------------------------------
inline bool CUtlBuffer::IsText() const 
{ 
	return (m_Flags & TEXT_BUFFER) != 0; 
}


//-----------------------------------------------------------------------------
// Can I grow if I'm externally allocated?
//-----------------------------------------------------------------------------
inline bool CUtlBuffer::IsGrowable() const 
{ 
	return (m_Flags & EXTERNAL_GROWABLE) != 0; 
}


//-----------------------------------------------------------------------------
// Am I valid? (overflow or underflow error), Once invalid it stays invalid
//-----------------------------------------------------------------------------
inline bool CUtlBuffer::IsValid() const 
{ 
	return m_Error == 0; 
}


//-----------------------------------------------------------------------------
// Do I contain carriage return/linefeeds? 
//-----------------------------------------------------------------------------
inline bool CUtlBuffer::ContainsCRLF() const 
{ 
	return IsText() && ((m_Flags & CONTAINS_CRLF) != 0); 
} 


//-----------------------------------------------------------------------------
// Am I read-only
//-----------------------------------------------------------------------------
inline bool CUtlBuffer::IsReadOnly() const
{
	return (m_Flags & READ_ONLY) != 0; 
}


//-----------------------------------------------------------------------------
// Buffer base and size
//-----------------------------------------------------------------------------
inline const void* CUtlBuffer::Base() const	
{ 
	return m_Memory.Base(); 
}

inline void* CUtlBuffer::Base()
{
	return m_Memory.Base(); 
}

// Returns the base as a const char*, only valid in text mode.
inline const char *CUtlBuffer::String() const
{
	Assert( IsText() );
	return reinterpret_cast<const char*>( m_Memory.Base() );
}

inline intp CUtlBuffer::Size() const			
{ 
	return m_Memory.NumAllocated(); 
}


//-----------------------------------------------------------------------------
// Clears out the buffer; frees memory
//-----------------------------------------------------------------------------
inline void CUtlBuffer::Clear()
{
	m_Get = 0;
	m_Put = 0;
	m_Error = 0;
	m_nOffset = 0;
	m_nMaxPut = -1;
	AddNullTermination();
}

inline void CUtlBuffer::Purge()
{
	m_Get = 0;
	m_Put = 0;
	m_nOffset = 0;
	m_nMaxPut = 0;
	m_Error = 0;
	m_Memory.Purge();
}

inline void CUtlBuffer::CopyBuffer( const CUtlBuffer &buffer )
{
	CopyBuffer( buffer.Base(), buffer.TellPut() );
}

inline void	CUtlBuffer::CopyBuffer( const void *pubData, intp cubData )
{
	Clear();
	if ( cubData )
	{
		Put( pubData, cubData ); //-V2002
	}
}

#endif // UTLBUFFER_H

