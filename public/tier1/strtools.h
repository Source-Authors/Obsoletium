//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef TIER1_STRTOOLS_H
#define TIER1_STRTOOLS_H

#include "tier0/dbg.h"
#include "tier0/platform.h"

#include <cctype>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <charconv>
#include <memory>
#include <system_error>
#include <type_traits>

#ifdef POSIX
#include <wchar.h>
#include <cmath>
#include <wctype.h>
#endif


class CUtlBuffer;
class CUtlString;

#ifdef _WIN64
#define str_size unsigned int
#else
#define str_size size_t
#endif

template< class T, class I > class CUtlMemory;
template< class T, class A > class CUtlVector;


//-----------------------------------------------------------------------------
// Portable versions of standard string functions
//-----------------------------------------------------------------------------
void	_V_memset	( const char* file, int line, void *dest, int fill, intp count );
void	_V_memcpy	( const char* file, int line, void *dest, const void *src, intp count );
void	_V_memmove	( const char* file, int line, void *dest, const void *src, intp count );
int		_V_memcmp	( const char* file, int line, const void *m1, const void *m2, intp count );
[[nodiscard]] intp	_V_strlen	( const char *file, int line, IN_Z const char *str );
void	_V_strcpy	( const char* file, int line, IN_Z char *dest, IN_Z const char *src );
[[nodiscard]] RET_MAY_BE_NULL char *_V_strrchr( const char *file, int line, IN_Z const char *s, char c );
[[nodiscard]] int _V_strcmp	( const char* file, int line, IN_Z const char *s1, IN_Z const char *s2 );
[[nodiscard]] int _V_wcscmp ( const char *file, int line, IN_Z const wchar_t *s1, IN_Z const wchar_t *s2 );
[[nodiscard]] RET_MAY_BE_NULL char*	_V_strstr ( const char* file, int line, IN_Z const char *s1, IN_Z const char *search );
[[nodiscard]] intp _V_wcslen ( const char *file, int line, IN_Z const wchar_t *pwch );
wchar_t*	_V_wcslower	( const char* file, int line, INOUT_Z wchar_t *start );
wchar_t*	_V_wcsupr	( const char* file, int line, INOUT_Z wchar_t *start );

// ASCII-optimized functions which fall back to CRT only when necessary
char *V_strupr( INOUT_Z char *start );
char *V_strlower( INOUT_Z char *start );
[[nodiscard]] int V_stricmp( IN_Z const char *s1, IN_Z const char *s2 );
[[nodiscard]] int V_strncmp( IN_Z const char *s1, IN_Z const char *s2, intp count );
[[nodiscard]] int V_strnicmp( IN_Z const char *s1, IN_Z const char *s2, intp n );

//-----------------------------------------------------------------------------
// Purpose: Slightly modified strtok. Does not modify the input string. Does
//			not skip over more than one separator at a time. This allows parsing
//			strings where tokens between separators may or may not be present:
//
//			Door01,,,0 would be parsed as "Door01"  ""  ""  "0"
//			Door01,Open,,0 would be parsed as "Door01"  "Open"  ""  "0"
//
// Input  : token - Returns with a token, or zero length if the token was missing.
//			str - String to parse.
//			sep - Character to use as separator. UNDONE: allow multiple separator chars
// Output : Returns a pointer to the next token to be parsed.
//-----------------------------------------------------------------------------
RET_MAY_BE_NULL const char *nexttoken(OUT_Z_CAP(nMaxTokenLen) char *token,
	size_t nMaxTokenLen,
	IN_OPT_Z const char *str,
	char sep);
template <size_t maxLenInChars>
inline RET_MAY_BE_NULL const char *nexttoken( OUT_Z_ARRAY char (&pToken)[maxLenInChars],
	IN_OPT_Z const char *str,
	char sep)
{
	return nexttoken( pToken, maxLenInChars, str, sep );
}


#ifdef POSIX

inline char *strupr( char *start )
{
	return V_strupr( start );
}

inline char *strlwr( char *start )
{
	return V_strlower( start );
}

inline wchar_t *_wcslwr( wchar_t *start )
{
	wchar_t *str = start;
	while( str && *str )
	{
		*str = (wchar_t)towlower(static_cast<wint_t>(*str));
		str++;
	}
	return start;
};

inline wchar_t *_wcsupr( wchar_t *start )
{
	wchar_t *str = start;
	while( str && *str )
	{
		*str = (wchar_t)towupper(static_cast<wint_t>(*str));
		str++;
	}
	return start;
};

#endif // POSIX


#ifdef _DEBUG

#define V_memset(dest, fill, count)		_V_memset   (__FILE__, __LINE__, (dest), (fill), (count))
#define V_memcpy(dest, src, count)		_V_memcpy	(__FILE__, __LINE__, (dest), (src), (count))
#define V_memmove(dest, src, count)		_V_memmove	(__FILE__, __LINE__, (dest), (src), (count))
#define V_memcmp(m1, m2, count)			_V_memcmp	(__FILE__, __LINE__, (m1), (m2), (count))
#define V_strlen(str)					_V_strlen	(__FILE__, __LINE__, (str))
#define V_strcpy(dest, src)				_V_strcpy	(__FILE__, __LINE__, (dest), (src))
#define V_strrchr(s, c)					_V_strrchr	(__FILE__, __LINE__, (s), (c))
#define V_strcmp(s1, s2)				_V_strcmp	(__FILE__, __LINE__, (s1), (s2))
#define V_wcscmp(s1, s2)				_V_wcscmp	(__FILE__, __LINE__, (s1), (s2))
#define V_strstr(s1, search )			_V_strstr	(__FILE__, __LINE__, (s1), (search) )
#define V_wcslen(pwch)					_V_wcslen	(__FILE__, __LINE__, (pwch))
#define V_wcslower(start)				_V_wcslower (__FILE__, __LINE__, (start))
#define V_wcsupr(start)					_V_wcsupr	(__FILE__, __LINE__, (start))

#else


inline void		V_memset (void *dest, int fill, intp count)			{ memset( dest, fill, count ); }
inline void		V_memcpy (void *dest, const void *src, intp count)	{ memcpy( dest, src, count ); }
inline void		V_memmove (void *dest, const void *src, intp count)	{ memmove( dest, src, count ); }
inline int		V_memcmp (const void *m1, const void *m2, intp count){ return memcmp( m1, m2, count ); }
inline intp		V_strlen (const char *str)							{ return (intp)strlen ( str ); }
inline void		V_strcpy (char *dest, const char *src)				{ strcpy( dest, src ); }
inline intp		V_wcslen(const wchar_t *pwch)						{ return (intp)wcslen(pwch); }
inline char*	V_strrchr (const char *s, char c)					{ return (char*)strrchr( s, c ); }
inline int		V_strcmp (const char *s1, const char *s2)			{ return strcmp( s1, s2 ); }
inline int		V_wcscmp (const wchar_t *s1, const wchar_t *s2)		{ return wcscmp( s1, s2 ); }
inline char*	V_strstr( const char *s1, const char *search )		{ return (char*)strstr( s1, search ); }
inline wchar_t*	V_wcslower (wchar_t *start)							{ return _wcslwr( start ); }
inline wchar_t*	V_wcsupr (wchar_t *start)							{ return _wcsupr( start ); }

#endif

[[nodiscard]] int		V_atoi(IN_Z const char *str);
[[nodiscard]] int64 	V_atoi64(IN_Z const char *str);
[[nodiscard]] uint64 	V_atoui64(IN_Z const char *str);
[[nodiscard]] int64		V_strtoi64(IN_Z const char *nptr, char **endptr, int base );
[[nodiscard]] uint64	V_strtoui64(IN_Z const char *nptr, char **endptr, int base );
[[nodiscard]] float		V_atof(IN_Z const char *str);
[[nodiscard]] RET_MAY_BE_NULL char*		V_stristr( IN_Z char* pStr, IN_Z const char* pSearch );
[[nodiscard]] RET_MAY_BE_NULL const char*	V_stristr( IN_Z const char* pStr, IN_Z const char* pSearch );
[[nodiscard]] RET_MAY_BE_NULL const char*	V_strnistr( IN_Z const char* pStr, IN_Z const char* pSearch, intp n );
[[nodiscard]] RET_MAY_BE_NULL const char*	V_strnchr( IN_Z const char* pStr, char c, intp n );
[[nodiscard]] inline int V_strcasecmp (IN_Z const char *s1, IN_Z const char *s2) { return V_stricmp(s1, s2); }
[[nodiscard]] inline int V_strncasecmp (IN_Z const char *s1, IN_Z const char *s2, intp n) { return V_strnicmp(s1, s2, n); }
void		V_qsort_s( INOUT_BYTECAP(num) void *base, size_t num, size_t width, int ( __cdecl *compare )(void *, const void *, const void *), void *context );


// returns string immediately following prefix, (ie str+strlen(prefix)) or nullptr if prefix not found
[[nodiscard]] RET_MAY_BE_NULL const char *StringAfterPrefix             ( IN_Z const char *str, IN_Z const char *prefix );
[[nodiscard]] RET_MAY_BE_NULL const char *StringAfterPrefixCaseSensitive( IN_Z const char *str, IN_Z const char *prefix );
[[nodiscard]] inline bool	StringHasPrefix             ( IN_Z const char *str, IN_Z const char *prefix ) { return StringAfterPrefix             ( str, prefix ) != nullptr; }
[[nodiscard]] inline bool	StringHasPrefixCaseSensitive( IN_Z const char *str, IN_Z const char *prefix ) { return StringAfterPrefixCaseSensitive( str, prefix ) != nullptr; }


template< bool CASE_SENSITIVE >
[[nodiscard]] inline bool _V_strEndsWithInner( IN_Z const char *pStr, IN_Z const char *pSuffix )
{
	intp nSuffixLen = V_strlen( pSuffix );
	intp nStringLen = V_strlen( pStr );
	if ( nSuffixLen == 0 )
		return true; // All strings end with the empty string (matches Java & .NET behaviour)
	if ( nStringLen < nSuffixLen )
		return false;
	pStr += nStringLen - nSuffixLen;
	if ( CASE_SENSITIVE )
		return !V_strcmp(  pStr, pSuffix );
	else
		return !V_stricmp( pStr, pSuffix );
}

// Does 'pStr' end with 'pSuffix'? (case sensitive/insensitive variants)
[[nodiscard]] inline bool V_strEndsWith(  IN_Z const char *pStr, IN_Z const char *pSuffix ) { return _V_strEndsWithInner<TRUE>(  pStr, pSuffix ); }
[[nodiscard]] inline bool V_striEndsWith( IN_Z const char *pStr, IN_Z const char *pSuffix ) { return _V_strEndsWithInner<FALSE>( pStr, pSuffix ); }


// Normalizes a float string in place.  
// (removes leading zeros, trailing zeros after the decimal point, and the decimal point itself where possible)
void V_normalizeFloatString( INOUT_Z char* pFloat );

// this is locale-unaware and therefore faster version of standard isdigit()
// It also avoids sign-extension errors.
[[nodiscard]] inline bool V_isdigit( char c )
{
	return c >= '0' && c <= '9';
}

[[nodiscard]] inline bool V_iswdigit( int c )
{ 
	return static_cast<uint>( c - '0' ) < 10;
}

[[nodiscard]] inline bool V_isempty( IN_OPT_Z const char* pszString ) { return !pszString || !pszString[ 0 ]; }

// The islower/isdigit/etc. functions all expect a parameter that is either
// 0-0xFF or EOF. It is easy to violate this constraint simply by passing
// 'char' to these functions instead of unsigned char.
// The V_ functions handle the char/unsigned char mismatch by taking a
// char parameter and casting it to unsigned char so that chars with the
// sign bit set will be zero extended instead of sign extended.
// Not that EOF cannot be passed to these functions.
//
// These functions could also be used for optimizations if locale
// considerations make some of the CRT functions slow.
//#undef isdigit // In case this is implemented as a macro
//#define isdigit use_V_isdigit_instead_of_isdigit
[[nodiscard]] inline bool V_isalpha(char c) { return isalpha( static_cast<unsigned char>(c) ) != 0; }
//#undef isalpha
//#define isalpha use_V_isalpha_instead_of_isalpha
[[nodiscard]] inline bool V_isalnum(char c) { return isalnum( static_cast<unsigned char>(c) ) != 0; }
//#undef isalnum
//#define isalnum use_V_isalnum_instead_of_isalnum
[[nodiscard]] inline bool V_isprint(char c) { return isprint( static_cast<unsigned char>(c) ) != 0; }
//#undef isprint
//#define isprint use_V_isprint_instead_of_isprint
[[nodiscard]] inline bool V_isxdigit(char c) { return isxdigit( static_cast<unsigned char>(c) ) != 0; }
//#undef isxdigit
//#define isxdigit use_V_isxdigit_instead_of_isxdigit
[[nodiscard]] inline bool V_ispunct(char c) { return ispunct( static_cast<unsigned char>(c) ) != 0; }
//#undef ispunct
//#define ispunct use_V_ispunct_instead_of_ispunct
[[nodiscard]] inline bool V_isgraph(char c) { return isgraph( static_cast<unsigned char>(c) ) != 0; }
//#undef isgraph
//#define isgraph use_V_isgraph_instead_of_isgraph
[[nodiscard]] inline bool V_isupper(char c) { return isupper( static_cast<unsigned char>(c) ) != 0; }
//#undef isupper
//#define isupper use_V_isupper_instead_of_isupper
[[nodiscard]] inline bool V_islower(char c) { return islower( static_cast<unsigned char>(c) ) != 0; }
//#undef islower
//#define islower use_V_islower_instead_of_islower
[[nodiscard]] inline bool V_iscntrl(char c) { return iscntrl( static_cast<unsigned char>(c) ) != 0; }
//#undef iscntrl
//#define iscntrl use_V_iscntrl_instead_of_iscntrl
[[nodiscard]] inline bool V_isspace(int c) {
	// dimhotepus: CSGO backport.
	//
	// The standard white-space characters are the following: space, tab,
	// carriage-return, newline, vertical tab, and form-feed. In the C locale,
	// V_isspace() returns true only for the standard white-space characters.
	// return c == ' ' || c == 9 /*horizontal tab*/ || c == '\r' || c == '\n' || c
	// == 11 /*vertical tab*/ || c == '\f';
	// codes of whitespace symbols: 9 HT, 10 \n, 11 VT, 12 form feed, 13 \r, 32
	// space

	// easy to understand version, validated:
	// return ((1 << (c-1)) & 0x80001F00) != 0 && ((c-1)&0xE0) == 0;

	// 5% faster on Core i7, 35% faster on Xbox360, no branches, validated:
#ifdef _X360
	return ((1 << (c - 1)) & 0x80001F00 & ~(-int((c - 1) & 0xE0))) != 0;
#else

	// this is 11% faster on Core i7 than the previous, VC2005 compiler
	// generates a seemingly unbalanced search tree that's faster
	switch (c) {
		case ' ':
		case '\t':
		case '\r':
		case '\n':
		case '\v':
		case '\f':
			return true;
		default:
			return false;
	}
#endif
}
//#undef isspace
//#define isspace use_V_isspace_instead_of_isspace


//-----------------------------------------------------------------------------
// Purpose: returns true if it's a valid hex string
//-----------------------------------------------------------------------------
[[nodiscard]] inline bool V_isvalidhex( IN_Z_CAP(inputchars) char const *in, intp inputchars )
{
	if ( inputchars < 2 )
		return false;
	if ( inputchars % 2 == 1 )
		return false;

	for ( intp i = 0; i < inputchars; i++ )
	{
		char c = in[i];
		if ( !(
			(c >= '0' && c <= '9') ||
			(c >= 'a' && c <= 'f') ||
			(c >= 'A' && c <= 'F')
			) )
		{
			return false;
		}

	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Checks if the string is lower case
// NOTE:	Only works with ASCII strings
//-----------------------------------------------------------------------------
[[nodiscard]] inline bool V_isstrlower( IN_Z const char *pch )
{
	const char *pCurrent = pch;
	while ( *pCurrent != '\0' )
	{
		if ( *pCurrent >= 'A' && *pCurrent <= 'Z' )
			return false;

		pCurrent++;
	}

	return true;
}



// These are versions of functions that guarantee nullptr termination.
//
// maxLen is the maximum number of bytes in the destination string.
// pDest[maxLen-1] is always nullptr terminated if pSrc's length is >= maxLen.
//
// This means the last parameter can usually be a sizeof() of a string.
void V_strncpy( OUT_Z_CAP(maxLenInChars) char *pDest, IN_Z const char *pSrc, intp maxLenInChars );

// Ultimate safe strcpy function, for arrays only -- buffer size is inferred by the compiler
template <intp maxLenInChars>
void V_strcpy_safe( OUT_Z_ARRAY char (&pDest)[maxLenInChars], IN_Z const char *pSrc ) 
{ 
	Assert(V_strlen(pSrc) < maxLenInChars);
	V_strncpy( pDest, pSrc, maxLenInChars ); 
}

// A function which duplicates a string using new[] to allocate the new string.
[[nodiscard]] RET_MAY_BE_NULL inline char *V_strdup( IN_Z const char *pSrc )
{
	const intp nLen = V_strlen( pSrc );
	char *pResult = new char [ nLen+1 ];
	if (pResult) //-V668
		V_memcpy( pResult, pSrc, nLen+1 );
	return pResult;
}

// A function which duplicates a string using new[] to allocate the new string.
[[nodiscard]] RET_MAY_BE_NULL inline wchar_t *V_wcsdup( IN_Z const wchar_t *pSrc )
{
	const intp nLen = V_wcslen( pSrc );
	auto *pResult = new wchar_t [ nLen+1 ];
	if (pResult) //-V668
		V_memcpy( pResult, pSrc, (nLen+1) * static_cast<intp>(sizeof(wchar_t)) );
	return pResult;
}

// dimhotepus: V_strdup on stack.
#define V_strdup_stack( src, name ) \
  const intp V_strdup_stack_##name##_len_{V_strlen(src) + 1};  \
  char *name{stackallocT(char, V_strdup_stack_##name##_len_)}; \
  V_strncpy(name, src, V_strdup_stack_##name##_len_)

// dimhotepus: V_wcsdup on stack.
#define V_wcsdup_stack( src, name ) \
  const intp V_wcsdup_stack_len_{V_wcslen(src) + 1};     \
  char *name{stackallocT(wchar_t, V_wcsdup_stack_len_)}; \
  V_wcsncpy( name, src, V_wcsdup_stack_len_ )

void V_wcsncpy( OUT_Z_BYTECAP(maxLenInBytes) wchar_t *pDest, IN_Z wchar_t const *pSrc, intp maxLenInBytes );
template <intp maxLenInChars>
void V_wcscpy_safe( OUT_Z_ARRAY wchar_t (&pDest)[maxLenInChars], IN_Z wchar_t const *pSrc ) 
{
	Assert(V_wcslen(pSrc) < maxLenInChars);
	V_wcsncpy( pDest, pSrc, maxLenInChars * static_cast<intp>(sizeof(*pDest)) ); 
}

#define COPY_ALL_CHARACTERS -1
char *V_strncat( INOUT_Z_CAP(destBufferSize) char *pDest, IN_Z const char *pSrc, size_t destBufferSize, intp max_chars_to_copy=COPY_ALL_CHARACTERS );
template <intp cchDest>
char *V_strcat_safe( INOUT_Z_ARRAY char (&pDest)[cchDest], IN_Z const char *pSrc, intp nMaxCharsToCopy=COPY_ALL_CHARACTERS )
{
	return V_strncat( pDest, pSrc, cchDest, nMaxCharsToCopy ); 
}

wchar_t *V_wcsncat( INOUT_Z_CAP(cchDest) wchar_t *pDest, IN_Z const wchar_t *pSrc, size_t cchDest, intp nMaxCharsToCopy=COPY_ALL_CHARACTERS );
template <intp cchDest> wchar_t *V_wcscat_safe( INOUT_Z_ARRAY wchar_t (&pDest)[cchDest], IN_Z const wchar_t *pSrc, intp nMaxCharsToCopy=COPY_ALL_CHARACTERS )
{ 
	return V_wcsncat( pDest, pSrc, cchDest, nMaxCharsToCopy ); 
}

char *V_strnlwr( INOUT_Z_CAP(count) char *pBuf, size_t count);
template <intp cchDest> char *V_strlwr_safe( INOUT_Z_ARRAY char (&pBuf)[cchDest] )
{ 
	return V_strnlwr( pBuf, cchDest ); 
}

// Unicode string conversion policies - what to do if an illegal sequence is encountered
enum EStringConvertErrorPolicy
{
	_STRINGCONVERTFLAG_SKIP =		1,
	_STRINGCONVERTFLAG_FAIL =		2,
	_STRINGCONVERTFLAG_ASSERT =		4,

	STRINGCONVERT_REPLACE =			0,
	STRINGCONVERT_SKIP =			_STRINGCONVERTFLAG_SKIP,
	STRINGCONVERT_FAIL =			_STRINGCONVERTFLAG_FAIL,

	STRINGCONVERT_ASSERT_REPLACE =	_STRINGCONVERTFLAG_ASSERT + STRINGCONVERT_REPLACE,
	STRINGCONVERT_ASSERT_SKIP =		_STRINGCONVERTFLAG_ASSERT + STRINGCONVERT_SKIP,
	STRINGCONVERT_ASSERT_FAIL =		_STRINGCONVERTFLAG_ASSERT + STRINGCONVERT_FAIL,
}; 

// Unicode (UTF-8, UTF-16, UTF-32) fundamental conversion functions.
[[nodiscard]] bool Q_IsValidUChar32( uchar32 uValue );
[[nodiscard]] int Q_UChar32ToUTF8Len( uchar32 uValue );
[[nodiscard]] int Q_UChar32ToUTF8( uchar32 uValue, OUT_Z_CAP(4) char *pOut );

[[nodiscard]] int Q_UChar32ToUTF16Len( uchar32 uValue );
[[nodiscard]] int Q_UChar32ToUTF16( uchar32 uValue, OUT_Z_CAP(2) uchar16 *pOut );

// Validate that a Unicode string is well-formed and contains only valid code points
[[nodiscard]] bool Q_UnicodeValidate( IN_Z const char *pUTF8 );
[[nodiscard]] bool Q_UnicodeValidate( IN_Z const uchar16 *pUTF16 );
[[nodiscard]] bool Q_UnicodeValidate( const uchar32 *pUTF32 );

// Returns length of string in Unicode code points (printed glyphs or non-printing characters)
[[nodiscard]] intp Q_UnicodeLength( IN_Z const char *pUTF8 );
[[nodiscard]] intp Q_UnicodeLength( IN_Z const uchar16 *pUTF16 );
[[nodiscard]] intp Q_UnicodeLength( const uchar32 *pUTF32 );

// Returns length of string in elements, not characters! These are analogous to Q_strlen and Q_wcslen
[[nodiscard]] inline intp Q_strlen16( IN_Z const uchar16 *puc16 ) { intp nElems = 0; while ( puc16[nElems] ) ++nElems; return nElems; }
[[nodiscard]] inline intp Q_strlen32( const uchar32 *puc32 ) { intp nElems = 0; while ( puc32[nElems] ) ++nElems; return nElems; }


// Repair invalid Unicode strings by dropping truncated characters and fixing improperly-double-encoded UTF-16 sequences.
// Unlike conversion functions which replace with '?' by default, a repair operation assumes that you know that something
// is wrong with the string (eg, mid-sequence truncation) and you just want to do the best possible job of cleaning it up.
// You can pass a REPLACE or FAIL policy if you would prefer to replace characters with '?' or clear the entire string.
// Returns nonzero on success, or 0 if the policy is FAIL and an invalid sequence was found.
intp Q_UnicodeRepair( INOUT_Z char *pUTF8, EStringConvertErrorPolicy ePolicy = STRINGCONVERT_SKIP );
intp Q_UnicodeRepair( INOUT_Z uchar16 *pUTF16, EStringConvertErrorPolicy ePolicy = STRINGCONVERT_SKIP );
intp Q_UnicodeRepair( uchar32 *pUTF32, EStringConvertErrorPolicy ePolicy = STRINGCONVERT_SKIP );

// Advance pointer forward by N Unicode code points (printed glyphs or non-printing characters), stopping at terminating null if encountered.
[[nodiscard]] char *Q_UnicodeAdvance( IN_Z char *pUTF8, intp nCharacters);
[[nodiscard]] uchar16 *Q_UnicodeAdvance( IN_Z uchar16 *pUTF16, intp nCharactersnCharacters );
[[nodiscard]] uchar32 *Q_UnicodeAdvance( uchar32 *pUTF32, intp nChars );
[[nodiscard]] const char *Q_UnicodeAdvance( IN_Z const char *pUTF8, intp nCharacters );
[[nodiscard]] const uchar16 *Q_UnicodeAdvance( IN_Z const uchar16 *pUTF16, intp nCharacters );
[[nodiscard]] const uchar32 *Q_UnicodeAdvance( const uchar32 *pUTF32, intp nCharacters );

// Truncate to maximum of N Unicode code points (printed glyphs or non-printing characters)
inline void Q_UnicodeTruncate( INOUT_Z char *pUTF8, intp nCharacters ) { *Q_UnicodeAdvance( pUTF8, nCharacters ) = 0; }
inline void Q_UnicodeTruncate( INOUT_Z uchar16 *pUTF16, intp nCharacters ) { *Q_UnicodeAdvance( pUTF16, nCharacters ) = 0; }
inline void Q_UnicodeTruncate( uchar32 *pUTF32, intp nCharacters ) { *Q_UnicodeAdvance( pUTF32, nCharacters ) = 0; }


// Conversion between Unicode string types (UTF-8, UTF-16, UTF-32). Deals with bytes, not element counts,
// to minimize harm from the programmer mistakes which continue to plague our wide-character string code.
// Returns the number of bytes written to the output, or if output is nullptr, the number of bytes required.
intp Q_UTF8ToUTF16( IN_Z const char *pUTF8, OUT_Z_BYTECAP(cubDestSizeInBytes) uchar16 *pUTF16, intp cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy = STRINGCONVERT_ASSERT_REPLACE );
intp Q_UTF8ToUTF32( IN_Z const char *pUTF8, OUT_Z_BYTECAP(cubDestSizeInBytes) uchar32 *pUTF32, intp cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy = STRINGCONVERT_ASSERT_REPLACE );
intp Q_UTF16ToUTF8( IN_Z const uchar16 *pUTF16, OUT_Z_BYTECAP(cubDestSizeInBytes) char *pUTF8, intp cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy = STRINGCONVERT_ASSERT_REPLACE );
intp Q_UTF16ToUTF32( IN_Z const uchar16 *pUTF16, OUT_Z_BYTECAP(cubDestSizeInBytes) uchar32 *pUTF32, intp cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy = STRINGCONVERT_ASSERT_REPLACE );
intp Q_UTF32ToUTF8( const uchar32 *pUTF32, OUT_Z_BYTECAP(cubDestSizeInBytes) char *pUTF8, intp cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy = STRINGCONVERT_ASSERT_REPLACE );
intp Q_UTF32ToUTF16( const uchar32 *pUTF32, OUT_Z_BYTECAP(cubDestSizeInBytes) uchar16 *pUTF16, intp cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy = STRINGCONVERT_ASSERT_REPLACE );

// This is disgusting and exist only easily to facilitate having 16-bit and 32-bit wchar_t's on different platforms
intp Q_UTF32ToUTF32( const uchar32 *pUTF32Source, OUT_Z_BYTECAP(cubDestSizeInBytes) uchar32 *pUTF32Dest, intp cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy = STRINGCONVERT_ASSERT_REPLACE );

// Conversion between count-limited UTF-n character arrays, including any potential nullptr characters.
// Output has a terminating nullptr for safety; strip the last character if you want an unterminated string.
// Returns the number of bytes written to the output, or if output is nullptr, the number of bytes required.
intp Q_UTF8CharsToUTF16( IN_Z const char *pUTF8, intp nElements, OUT_Z_BYTECAP(cubDestSizeInBytes) uchar16 *pUTF16, intp cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy = STRINGCONVERT_ASSERT_REPLACE );
intp Q_UTF8CharsToUTF32( IN_Z const char *pUTF8, intp nElements, OUT_Z_BYTECAP(cubDestSizeInBytes) uchar32 *pUTF32, intp cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy = STRINGCONVERT_ASSERT_REPLACE );
intp Q_UTF16CharsToUTF8( IN_Z const uchar16 *pUTF16, intp nElements, OUT_Z_BYTECAP(cubDestSizeInBytes) char *pUTF8, intp cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy = STRINGCONVERT_ASSERT_REPLACE );
intp Q_UTF16CharsToUTF32( IN_Z const uchar16 *pUTF16, intp nElements, OUT_Z_BYTECAP(cubDestSizeInBytes) uchar32 *pUTF32, intp cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy = STRINGCONVERT_ASSERT_REPLACE );
intp Q_UTF32CharsToUTF8( const uchar32 *pUTF32, intp nElements, OUT_Z_BYTECAP(cubDestSizeInBytes) char *pUTF8, intp cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy = STRINGCONVERT_ASSERT_REPLACE );
intp Q_UTF32CharsToUTF16( const uchar32 *pUTF32, intp nElements, OUT_Z_BYTECAP(cubDestSizeInBytes) uchar16 *pUTF16, intp cubDestSizeInBytes, EStringConvertErrorPolicy ePolicy = STRINGCONVERT_ASSERT_REPLACE );

// Decode a single UTF-8 character to a uchar32, returns number of UTF-8 bytes parsed
int Q_UTF8ToUChar32( IN_Z const char *pUTF8_, uchar32 &uValueOut, bool &bErrorOut );

// Decode a single UTF-16 character to a uchar32, returns number of UTF-16 characters (NOT BYTES) consumed
int Q_UTF16ToUChar32( IN_Z const uchar16 *pUTF16, uchar32 &uValueOut, bool &bErrorOut );


// NOTE: WString means either UTF32 or UTF16 depending on the platform and compiler settings.
#if defined( _MSC_VER ) || defined( _WIN32 )
#define Q_UTF8ToWString Q_UTF8ToUTF16
#define Q_UTF8CharsToWString Q_UTF8CharsToUTF16
#define Q_UTF32ToWString Q_UTF32ToUTF16
#define Q_WStringToUTF8 Q_UTF16ToUTF8
#define Q_WStringCharsToUTF8 Q_UTF16CharsToUTF8
#define Q_WStringToUTF32 Q_UTF16ToUTF32
#else
#define Q_UTF8ToWString Q_UTF8ToUTF32
#define Q_UTF8CharsToWString Q_UTF8CharsToUTF32
#define Q_UTF32ToWString Q_UTF32ToUTF32
#define Q_WStringToUTF8 Q_UTF32ToUTF8
#define Q_WStringCharsToUTF8 Q_UTF32CharsToUTF8
#define Q_WStringToUTF32 Q_UTF32ToUTF32
#endif

// These are legacy names which don't make a lot of sense but are used everywhere. Prefer the WString convention wherever possible
#define V_UTF8ToUnicode Q_UTF8ToWString
#define V_UnicodeToUTF8 Q_WStringToUTF8

#ifdef POSIX
#include <cstdarg>
#endif

#ifdef _WIN32
constexpr inline char CORRECT_PATH_SEPARATOR{'\\'};
#define CORRECT_PATH_SEPARATOR_S "\\"
constexpr inline char INCORRECT_PATH_SEPARATOR{'/'};
#define INCORRECT_PATH_SEPARATOR_S "/";
#elif POSIX
constexpr inline char CORRECT_PATH_SEPARATOR{'/'};
#define CORRECT_PATH_SEPARATOR_S "/"
constexpr inline char INCORRECT_PATH_SEPARATOR{'\\'};
#define INCORRECT_PATH_SEPARATOR_S "\\"
#endif

int V_vsnprintf( OUT_Z_CAP(maxLenInChars) char *pDest,
	intp maxLenInChars,
	PRINTF_FORMAT_STRING const char *pFormat,
	va_list params );
template <size_t maxLenInCharacters>
int V_vsprintf_safe( OUT_Z_ARRAY char (&pDest)[maxLenInCharacters],
	PRINTF_FORMAT_STRING const char *pFormat,
	va_list params )
{
	return V_vsnprintf( pDest, maxLenInCharacters, pFormat, params );
}

int V_snprintf( OUT_Z_CAP(maxLenInChars) char *pDest,
	intp maxLenInChars,
	PRINTF_FORMAT_STRING const char *pFormat,
	... ) FMTFUNCTION( 3, 4 );
// gcc insists on only having format annotations on declarations, not definitions, which is why I have both.
template <size_t maxLenInChars>
int V_sprintf_safe( OUT_Z_ARRAY char (&pDest)[maxLenInChars],
	PRINTF_FORMAT_STRING const char *pFormat,
	... ) FMTFUNCTION( 2, 3 );
template <size_t maxLenInChars>
int V_sprintf_safe( OUT_Z_ARRAY char (&pDest)[maxLenInChars],
	PRINTF_FORMAT_STRING const char *pFormat,
	... )
{
	va_list params;
	va_start( params, pFormat ); //-V2019 //-V2018
	int result = V_vsnprintf( pDest, maxLenInChars, pFormat, params );
	va_end( params );
	return result;
}

// gcc insists on only having format annotations on declarations, not definitions, which is why I have both.
// Append formatted text to an array in a safe manner -- always null-terminated, truncation rather than buffer overrun.
template <size_t maxLenInChars>
int V_sprintfcat_safe( INOUT_Z_ARRAY char (&pDest)[maxLenInChars],
	PRINTF_FORMAT_STRING const char *pFormat,
	... ) FMTFUNCTION( 2, 3 );
template <size_t maxLenInChars>
int V_sprintfcat_safe( INOUT_Z_ARRAY char (&pDest)[maxLenInChars],
	PRINTF_FORMAT_STRING const char *pFormat,
	... )
{
	va_list params;
	va_start( params, pFormat ); //-V2019 //-V2018
	size_t usedLength = V_strlen(pDest);
	// This code is here to check against buffer overruns when uninitialized arrays are passed in.
	// It should never be executed. Unfortunately we can't assert in this header file.
	if ( usedLength >= maxLenInChars )
		usedLength = 0;
	int result = V_vsnprintf( pDest + usedLength, maxLenInChars - usedLength, pFormat, params );
	va_end( params );
	return result;
}

int V_vsnwprintf( OUT_Z_CAP(maxLenInChars) wchar_t *pDest,
	intp maxLenInChars,
	PRINTF_FORMAT_STRING const wchar_t *pFormat,
	va_list params );
template <size_t maxLenInCharacters>
int V_vswprintf_safe( OUT_Z_ARRAY wchar_t (&pDest)[maxLenInCharacters],
	PRINTF_FORMAT_STRING const wchar_t *pFormat,
	va_list params )
{
	return V_vsnwprintf( pDest, maxLenInCharacters, pFormat, params );
}

int V_vsnprintfRet( OUT_Z_CAP(maxLenInChars) char *pDest,
	intp maxLenInChars,
	PRINTF_FORMAT_STRING const char *pFormat,
	va_list params,
	bool *pbTruncated );
template <size_t maxLenInCharacters>
int V_vsprintfRet_safe( OUT_Z_ARRAY char (&pDest)[maxLenInCharacters],
	PRINTF_FORMAT_STRING const char *pFormat,
	va_list params,
	bool *pbTruncated )
{
	return V_vsnprintfRet( pDest, maxLenInCharacters, pFormat, params, pbTruncated );
}

// FMTFUNCTION can only be used on ASCII functions, not wide-char functions.
int V_snwprintf( OUT_Z_CAP(maxLenInChars) wchar_t *pDest,
	intp maxLenInChars,
	PRINTF_FORMAT_STRING const wchar_t *pFormat,
	... );
template <size_t maxLenInChars>
int V_swprintf_safe( OUT_Z_ARRAY wchar_t (&pDest)[maxLenInChars],
	PRINTF_FORMAT_STRING const wchar_t *pFormat,
	... )
{
	va_list params;
	va_start( params, pFormat ); //-V2019 //-V2018
	int result = V_vsnwprintf( pDest, maxLenInChars, pFormat, params );
	va_end( params );
	return result;
}

// Prints out a pretified memory counter string value ( e.g., 7,233.27 MiB, 1,298.003 KiB, 127 bytes )
// dimhotepus: Add double version as some inputs ({u}int64) are too large and truncated on float.
// Prints out a pretified memory counter string value ( e.g., 7,233.27 MiB, 1,298.003 KiB, 127 bytes )
[[nodiscard]] char *V_pretifymem( double value, int digitsafterdecimal = 2, bool usebinaryonek = false );

// Prints out a pretified integer with comma separators (eg, 7,233,270,000)
[[nodiscard]] char *V_pretifynum( int64 value );

intp _V_UCS2ToUnicode( const ucs2 *pUCS2, OUT_Z_BYTECAP(cubDestSizeInBytes) wchar_t *pUnicode, intp cubDestSizeInBytes );
template< typename T >
inline intp V_UCS2ToUnicode( const ucs2 *pUCS2, OUT_Z_BYTECAP(cubDestSizeInBytes) wchar_t *pUnicode, T cubDestSizeInBytes )
{
	return _V_UCS2ToUnicode( pUCS2, pUnicode, static_cast<intp>(cubDestSizeInBytes) );
}

int _V_UCS2ToUTF8( const ucs2 *pUCS2, OUT_Z_BYTECAP(cubDestSizeInBytes) char *pUTF8, int cubDestSizeInBytes );
template< typename T >
inline int V_UCS2ToUTF8( const ucs2 *pUCS2, OUT_Z_BYTECAP(cubDestSizeInBytes) char *pUTF8, T cubDestSizeInBytes )
{
	return _V_UCS2ToUTF8( pUCS2, pUTF8, static_cast<int>(cubDestSizeInBytes) );
}

intp _V_UnicodeToUCS2( const wchar_t *pUnicode, intp cubSrcInBytes, OUT_Z_BYTECAP(cubDestSizeInBytes) char *pUCS2, intp cubDestSizeInBytes );
template< typename T, typename U >
inline intp V_UnicodeToUCS2( const wchar_t *pUnicode, T cubSrcInBytes, OUT_Z_BYTECAP(cubDestSizeInBytes) char *pUCS2, U cubDestSizeInBytes )
{
	return _V_UnicodeToUCS2( pUnicode, static_cast<intp>(cubSrcInBytes), pUCS2, static_cast<intp>(cubDestSizeInBytes) );
}

int _V_UTF8ToUCS2( const char *pUTF8, int cubSrcInBytes, OUT_Z_BYTECAP(cubDestSizeInBytes) ucs2 *pUCS2, int cubDestSizeInBytes );
template< typename T, typename U >
inline int V_UTF8ToUCS2( const char *pUTF8, T cubSrcInBytes, OUT_Z_BYTECAP(cubDestSizeInBytes) ucs2 *pUCS2, U cubDestSizeInBytes )
{
	return _V_UTF8ToUCS2( pUTF8, static_cast<int>(cubSrcInBytes), pUCS2, static_cast<int>(cubDestSizeInBytes) );
}

// strips leading and trailing whitespace; returns true if any characters were removed. UTF-8 and UTF-16 versions.
bool Q_StripPrecedingAndTrailingWhitespace( INOUT_Z char *pch );
bool Q_StripPrecedingAndTrailingWhitespaceW( INOUT_Z wchar_t *pwch );

// strips leading and trailing whitespace, also taking "aggressive" characters 
// like punctuation spaces, non-breaking spaces, composing characters, and so on
bool Q_AggressiveStripPrecedingAndTrailingWhitespace( INOUT_Z char *pch );
bool Q_AggressiveStripPrecedingAndTrailingWhitespaceW( INOUT_Z wchar_t *pwch );
bool Q_RemoveAllEvilCharacters( INOUT_Z char *pch );

// Functions for converting hexidecimal character strings back into binary data etc.
//
// e.g., 
// int output;
// V_hextobinary( "ffffffff", 8, &output, sizeof( output ) );
// would make output == 0xfffffff or -1
// Similarly,
// char buffer[ 9 ];
// V_binarytohex( &output, sizeof( output ), buffer, sizeof( buffer ) );
// would put "ffffffff" into buffer (note null terminator!!!)
unsigned char V_nibble( char c );
// dimhotepus: Check data and report failures.
[[nodiscard]] bool V_nibble( char c, unsigned char &nibble );
bool V_hextobinary( IN_Z_CAP(numchars) char const *in, intp numchars, OUT_BYTECAP(maxoutputbytes) byte *out, intp maxoutputbytes );
// int output;
// V_hextobinary( "ffffffff", 8, &output );
// would make output == 0xfffffff or -1
template<intp outSize>
bool V_hextobinary( IN_Z_CAP(numchars) char const *in, intp numchars, byte (&out)[outSize] )
{
	return V_hextobinary( in, numchars, out, outSize );
}
// int output;
// V_hextobinary( "ffffffff", 8, output );
// would make output == 0xfffffff or -1
template<typename T>
std::enable_if_t<!std::is_pointer_v<T>, bool> V_hextobinary( IN_Z_CAP(numchars) char const *in, intp numchars, T &out )
{
	return V_hextobinary( in, numchars, reinterpret_cast<byte*>(&out), static_cast<intp>(sizeof(out)) );
}
// char buffer[ 9 ];
// V_binarytohex( &output, sizeof( output ), buffer, sizeof( buffer ) );
// would put "ffffffff" into buffer (note null terminator!!!)
void V_binarytohex( IN_BYTECAP(inputbytes) const byte *in, intp inputbytes, OUT_Z_CAP(outsize) char *out, intp outsize );
// char buffer[ 9 ];
// V_binarytohex( &output, sizeof( output ), buffer );
// would put "ffffffff" into buffer (note null terminator!!!)
template<intp outSize>
void V_binarytohex( IN_BYTECAP(inputbytes) const byte *in, intp inputbytes, OUT_Z_ARRAY char (&out)[outSize] )
{
	V_binarytohex( in, inputbytes, out, outSize );
}
// char buffer[ 9 ];
// V_binarytohex( output, buffer );
// would put "ffffffff" into buffer (note null terminator!!!)
template<intp inSize, intp outSize>
void V_binarytohex( const byte (&in)[inSize], OUT_Z_ARRAY char (&out)[outSize] )
{
	V_binarytohex( in, inSize, out, outSize );
}
// char buffer[ 9 ];
// V_binarytohex( output, buffer );
// would put "ffffffff" into buffer (note null terminator!!!)
template<typename T, intp outSize>
std::enable_if_t<!std::is_pointer_v<T>>	V_binarytohex( const T &in, OUT_Z_ARRAY char (&out)[outSize] )
{
	V_binarytohex( reinterpret_cast<const byte*>(&in), static_cast<intp>(sizeof(in)), out, outSize );
}

// Tools for working with filenames
// Extracts the base name of a file (no path, no extension, assumes '/' or '\' as path separator)
void V_FileBase( IN_Z const char *in, OUT_Z_CAP(maxlen) char *out, intp maxlen );
// Extracts the base name of a file (no path, no extension, assumes '/' or '\' as path separator)
template<intp outSize>
void V_FileBase( IN_Z const char *in, OUT_Z_ARRAY char (&out)[outSize] )
{
	V_FileBase( in, out, outSize );
}

// Remove the final characters of ppath if it's '\' or '/'.
void V_StripTrailingSlash( INOUT_Z char *ppath );

// Remove the final characters of ppline if they are whitespace (uses V_isspace)
void V_StripTrailingWhitespace( INOUT_Z char *ppline );

// Remove the initial characters of ppline if they are whitespace (uses V_isspace)
void V_StripLeadingWhitespace( INOUT_Z char *ppline );

// Remove the initial/final characters of ppline if they are " quotes
void V_StripSurroundingQuotes( INOUT_Z char *ppline );

// Remove any extension from in and return resulting string in out
void V_StripExtension( IN_Z const char *in, OUT_Z_CAP(outSize) char *out, intp outSize );
// Remove any extension from in and return resulting string in out
template<intp outSize>
void V_StripExtension( IN_Z const char *in, OUT_Z_ARRAY char (&out)[outSize] )
{
	return V_StripExtension( in, out, outSize );
}

// Make path end with extension if it doesn't already have an extension
void V_DefaultExtension( INOUT_Z_CAP(pathStringLength) char *path, IN_Z const char *extension, intp pathStringLength );
// Make path end with extension if it doesn't already have an extension
template<intp pathSize>
void V_DefaultExtension( INOUT_Z_ARRAY char (&path)[pathSize], IN_Z const char* extension )
{
	V_DefaultExtension( path, extension, pathSize );
}
// Strips any current extension from path and ensures that extension is the new extension
void V_SetExtension( INOUT_Z_CAP(pathStringLength) char *path, IN_Z const char *extension, intp pathStringLength );
// Strips any current extension from path and ensures that extension is the new extension
template<intp pathSize>
void V_SetExtension( INOUT_Z_ARRAY char (&path)[pathSize], IN_Z const char *extension )
{
	V_SetExtension( path, extension, pathSize );
}

// Removes any filename from path ( strips back to previous / or \ character )
void V_StripFilename( INOUT_Z char *path );
// Remove the final directory from the path
bool V_StripLastDir( INOUT_Z_CAP(maxlen) char *dirName, intp maxlen );
// Remove the final directory from the path
template<intp dirNameSize>
bool V_StripLastDir( INOUT_Z_ARRAY char (&dirName)[dirNameSize] )
{
	return V_StripLastDir( dirName, dirNameSize );
}
// Returns a pointer to the unqualified file name (no path) of a file name
const char * V_UnqualifiedFileName( IN_Z const char * in );
// Given a path and a filename, composes "path\filename", inserting the (OS correct) separator if necessary
void V_ComposeFileName( IN_Z const char *path, IN_Z const char *filename, OUT_Z_CAP(destSize) char *dest, intp destSize );
// Given a path and a filename, composes "path\filename", inserting the (OS correct) separator if necessary
template<intp destSize>
void V_ComposeFileName( IN_Z const char *path, IN_Z const char *filename, OUT_Z_ARRAY char (&dest)[destSize] )
{
	V_ComposeFileName( path, filename, dest, destSize );
}

// Copy out the path except for the stuff after the final pathseparator
bool V_ExtractFilePath( IN_Z const char *path,  OUT_Z_CAP(destSize) char *dest, intp destSize );
// Copy out the path except for the stuff after the final pathseparator
template<intp destSize>
bool V_ExtractFilePath( IN_Z const char *path, OUT_Z_ARRAY char (&dest)[destSize] )
{
	return V_ExtractFilePath( path, dest, destSize );
}

// Copy out the file extension into dest
void V_ExtractFileExtension( IN_Z const char *path, OUT_Z_CAP(destSize) char *dest, intp destSize );
// Copy out the file extension into dest
template<intp destSize>
void V_ExtractFileExtension( IN_Z const char *path, OUT_Z_ARRAY char (&dest)[destSize] )
{
	V_ExtractFileExtension( path, dest, destSize );
}

[[nodiscard]] RET_MAY_BE_NULL const char *V_GetFileExtension(IN_Z const char *path);

// returns a pointer to just the filename part of the path
// (everything after the last path seperator)
[[nodiscard]] const char *V_GetFileName( IN_Z const char *path );

// This removes "./" and "../" from the pathname. pFilename should be a full pathname.
// Also incorporates the behavior of V_FixSlashes and optionally V_FixDoubleSlashes.
// Returns false if it tries to ".." past the root directory in the drive (in which case 
// it is an invalid path).
bool V_RemoveDotSlashes( INOUT_Z char *pFilename, char separator = CORRECT_PATH_SEPARATOR, bool bRemoveDoubleSlashes = true );

// If pPath is a relative path, this function makes it into an absolute path
// using the current working directory as the base, or pStartingDir if it's non-nullptr.
// Returns false if it runs out of room in the string, or if pPath tries to ".." past the root directory.
void V_MakeAbsolutePath( OUT_Z_CAP(outLen) char *pOut, int outLen, IN_Z const char *pPath, IN_OPT_Z const char *pStartingDir = nullptr );

// If pPath is a relative path, this function makes it into an absolute path
// using the current working directory as the base, or pStartingDir if it's non-nullptr.
// Returns false if it runs out of room in the string, or if pPath tries to ".." past the root directory.
template<int outSize>
void V_MakeAbsolutePath( OUT_Z_ARRAY char (&pOut)[outSize], IN_Z const char *pPath, IN_OPT_Z const char *pStartingDir = nullptr )
{
	V_MakeAbsolutePath( pOut, outSize, pPath, pStartingDir );
}

inline void V_MakeAbsolutePath( OUT_Z_CAP(outLen) char *pOut, int outLen, IN_Z const char *pPath, IN_OPT_Z const char *pStartingDir, bool bLowercaseName )
{
	V_MakeAbsolutePath( pOut, outLen, pPath, pStartingDir );
	if ( bLowercaseName )
	{
		V_strlower( pOut );
	}
}

template<int outSize>
inline void V_MakeAbsolutePath( OUT_Z_ARRAY char (&pOut)[outSize], IN_Z const char *pPath, IN_OPT_Z const char *pStartingDir, bool bLowercaseName )
{
	V_MakeAbsolutePath( pOut, outSize, pPath, pStartingDir, bLowercaseName );
}


// Creates a relative path given two full paths
// The first is the full path of the file to make a relative path for.
// The second is the full path of the directory to make the first file relative to
// Returns false if they can't be made relative (on separate drives, for example)
bool V_MakeRelativePath( IN_Z const char *pFullPath, IN_Z const char *pDirectory, OUT_Z_CAP(nBufLen) char *pRelativePath, intp nBufLen );

// Creates a relative path given two full paths
// The first is the full path of the file to make a relative path for.
// The second is the full path of the directory to make the first file relative to
// Returns false if they can't be made relative (on separate drives, for example)
template<intp relativePathSize>
bool V_MakeRelativePath( IN_Z const char *pFullPath, IN_Z const char *pDirectory, OUT_Z_ARRAY char (&pRelativePath)[relativePathSize] )
{
	return V_MakeRelativePath( pFullPath, pDirectory, pRelativePath, relativePathSize );
}

// Fixes up a file name, removing dot slashes, fixing slashes, converting to lowercase, etc.
void V_FixupPathName( OUT_Z_CAP(nOutLen) char *pOut, intp nOutLen, IN_Z const char *pPath );

// Fixes up a file name, removing dot slashes, fixing slashes, converting to lowercase, etc.
template<intp outSize>
void V_FixupPathName( OUT_Z_ARRAY char (&pOut)[outSize], IN_Z const char *pPath )
{
	V_FixupPathName( pOut, outSize, pPath );
}

// Adds a path separator to the end of the string if there isn't one already.
// Returns false if it would run out of space.
void V_AppendSlash( INOUT_Z_CAP(strSize) char *pStr, intp strSize );

// Adds a path separator to the end of the string if there isn't one already.
// Returns false if it would run out of space.
template<intp strSize>
void V_AppendSlash( INOUT_Z_ARRAY char(&pStr)[strSize] )
{
	V_AppendSlash( pStr, strSize );
}

// Returns true if the path is an absolute path.
[[nodiscard]] bool V_IsAbsolutePath( IN_Z const char *pPath );

// Scans pIn and replaces all occurences of pMatch with pReplaceWith.
// Writes the result to pOut.
// Returns true if it completed successfully.
// If it would overflow pOut, it fills as much as it can and returns false.
bool V_StrSubst( IN_Z const char *pIn, IN_Z const char *pMatch, IN_Z const char *pReplaceWith,
	OUT_Z_CAP(outLen) char *pOut, intp outLen, bool bCaseSensitive=false );

// Scans pIn and replaces all occurences of pMatch with pReplaceWith.
// Writes the result to pOut.
// Returns true if it completed successfully.
// If it would overflow pOut, it fills as much as it can and returns false.
template<intp outSize>
bool V_StrSubst( IN_Z const char *pIn, IN_Z const char *pMatch, IN_Z const char *pReplaceWith,
	OUT_Z_ARRAY char (&pOut)[outSize], bool bCaseSensitive=false )
{
	return V_StrSubst( pIn, pMatch, pReplaceWith, pOut, outSize, bCaseSensitive );
}

// Split the specified string on the specified separator.
// Returns a list of strings separated by pSeparator.
// You are responsible for freeing the contents of outStrings (call outStrings.PurgeAndDeleteElements).
void V_SplitString( IN_Z const char *pString, IN_Z const char *pSeparator, CUtlVector<char*, CUtlMemory<char*, intp> > &outStrings );

void V_SplitString( IN_Z const char *pString, IN_Z const char *pSeparator, CUtlVector< CUtlString, CUtlMemory<CUtlString, intp> > &outStrings, bool bIncludeEmptyStrings = false );

// Just like V_SplitString, but it can use multiple possible separators.
void V_SplitString2( IN_Z const char *pString,
	const char **pSeparators,
	intp nSeparators,
	CUtlVector<char*, CUtlMemory<char*, intp> > &outStrings );
// Just like V_SplitString, but it can use multiple possible separators.
template<intp separatorsSize>
void V_SplitString2( IN_Z const char *pString,
	const char * (&pSeparators)[separatorsSize],
	CUtlVector<char*, CUtlMemory<char*, intp> > &outStrings )
{
	V_SplitString2( pString, pSeparators, separatorsSize, outStrings );
}

// Returns false if the buffer is not large enough to hold the working directory name.
[[nodiscard]] bool V_GetCurrentDirectory( OUT_Z_CAP(maxLen) char *pOut, int maxLen );

// Returns false if the buffer is not large enough to hold the working directory name.
template<int outSize>
[[nodiscard]] bool V_GetCurrentDirectory( OUT_Z_ARRAY char (&pOut)[outSize] )
{
	return V_GetCurrentDirectory( pOut, outSize );
}

// Set the working directory thus.
[[nodiscard]] bool V_SetCurrentDirectory( IN_Z const char *pDirName );


// This function takes a slice out of pStr and stores it in pOut.
// It follows the Python slice convention:
// Negative numbers wrap around the string (-1 references the last character).
// Large numbers are clamped to the end of the string.
void V_StrSlice( IN_Z const char *pStr,
	intp firstChar, intp lastCharNonInclusive,
	OUT_Z_CAP(outSize) char *pOut, intp outSize );

// This function takes a slice out of pStr and stores it in pOut.
// It follows the Python slice convention:
// Negative numbers wrap around the string (-1 references the last character).
// Large numbers are clamped to the end of the string.
template<intp outSize>
void V_StrSlice( IN_Z const char *pStr,
	intp firstChar, intp lastCharNonInclusive,
	OUT_Z_ARRAY char (&pOut)[outSize] )
{
	return V_StrSlice( pStr, firstChar, lastCharNonInclusive, pOut, outSize );
}

// Chop off the left nChars of a string.
void V_StrLeft( IN_Z const char *pStr, intp nChars, OUT_Z_CAP(outSize) char *pOut, intp outSize );

// Chop off the left nChars of a string.
template<intp outSize>
void V_StrLeft( IN_Z const char *pStr, intp nChars, OUT_Z_ARRAY char (&pOut)[outSize] )
{
	V_StrLeft( pStr, nChars, pOut, outSize );
}

// Chop off the right nChars of a string.
void V_StrRight( IN_Z const char *pStr, intp nChars, OUT_Z_CAP(outSize) char *pOut, intp outSize );

// Chop off the right nChars of a string.
template<intp outSize>
void V_StrRight( IN_Z const char *pStr, intp nChars, OUT_Z_ARRAY char (&pOut)[outSize] )
{
	V_StrRight( pStr, nChars, pOut, outSize );
}

// change "special" characters to have their c-style backslash sequence. like \n, \r, \t, ", etc.
// returns a pointer to a newly allocated string, which you must delete[] when finished with.
[[nodiscard]] char *V_AddBackSlashesToSpecialChars( IN_Z const char *pSrc );

// Force slashes of either type to be = separator character
void V_FixSlashes( INOUT_Z char *pname, char separator = CORRECT_PATH_SEPARATOR );

// This function fixes cases of filenames like materials\\blah.vmt or somepath\otherpath\\ and removes the extra double slash.
void V_FixDoubleSlashes( INOUT_Z char *pStr );

// Convert multibyte to wchar + back
// Specify -1 for nInSize for null-terminated string
void V_strtowcs( IN_Z_CAP(nInSize) const char *pString, int nInSize, OUT_Z_BYTECAP(nOutSizeInBytes) wchar_t *pWString, int nOutSizeInBytes );

// Convert multibyte to wchar + back
// Specify -1 for nInSize for null-terminated string
template<int destSize>
void V_strtowcs( IN_Z_CAP(nInSize) const char *pString, int nInSize, OUT_Z_ARRAY wchar_t (&pWString)[destSize] )
{
	V_strtowcs( pString, nInSize, pWString, destSize * static_cast<int>(sizeof(wchar_t)) );
}

void V_wcstostr( IN_Z_CAP(nInSize) const wchar_t *pWString, int nInSize, OUT_Z_CAP(nOutSizeInBytes) char *pString, int nOutSizeInBytes );

template <int destSize>
void V_wcstostr( IN_Z_CAP(nInSize) const wchar_t *pWString, int nInSize, OUT_Z_ARRAY char (&pString)[destSize] )
{
	V_wcstostr( pWString, nInSize, pString, destSize );
}

// buffer-safe strcat
inline void V_strcat( INOUT_Z_CAP(cchDest) char *dest, IN_Z const char *src, intp cchDest )
{
	V_strncat( dest, src, cchDest, COPY_ALL_CHARACTERS );
}

// Buffer safe wcscat
inline void V_wcscat( INOUT_Z_CAP(cchDest) wchar_t *dest, IN_Z const wchar_t *src, intp cchDest )
{
	V_wcsncat( dest, src, cchDest, COPY_ALL_CHARACTERS );
}

// Encode a string for display as HTML -- this only encodes ' " & < >, which are the important ones to encode for 
// security and ensuring HTML display doesn't break.  Other special chars like the ? sign and so forth will not
// be encoded
//
// Returns false if there was not enough room in pDest to encode the entire source string, otherwise true
[[nodiscard]] bool V_BasicHtmlEntityEncode( OUT_Z_CAP( nDestSize ) char *pDest, const intp nDestSize, IN_Z_CAP(nInSize) char const *pIn, const intp nInSize, bool bPreserveWhitespace = false );

// Decode a string with htmlentities HTML -- this should handle all special chars, not just the ones Q_BasicHtmlEntityEncode uses.
//
// Returns false if there was not enough room in pDest to decode the entire source string, otherwise true
[[nodiscard]] bool V_HtmlEntityDecodeToUTF8( OUT_Z_CAP( nDestSize ) char *pDest, const intp nDestSize, IN_Z_CAP(nInSize) char const *pIn, const intp nInSize );

// strips HTML from a string.  Should call Q_HTMLEntityDecodeToUTF8 afterward.
void V_StripAndPreserveHTML( CUtlBuffer *pbuffer, IN_Z const char *pchHTML, const char **rgszPreserveTags, size_t cPreserveTags, size_t cMaxResultSize );
void V_StripAndPreserveHTMLCore( CUtlBuffer *pbuffer, IN_Z const char *pchHTML, const char **rgszPreserveTags, size_t cPreserveTags, const char **rgszNoCloseTags, size_t cNoCloseTags, size_t cMaxResultSize );

// Extracts the domain from a URL
[[nodiscard]] bool V_ExtractDomainFromURL( IN_Z const char *pchURL, OUT_Z_CAP( cchDomain ) char *pchDomain, intp cchDomain );

// returns true if the url passed in is on the specified domain
[[nodiscard]] bool V_URLContainsDomain( IN_Z const char *pchURL, IN_Z const char *pchDomain );

//-----------------------------------------------------------------------------
// returns true if the character is allowed in a URL, false otherwise
//-----------------------------------------------------------------------------
[[nodiscard]] bool V_IsValidURLCharacter( IN_Z const char *pch, IN_OPT intp *pAdvanceBytes );

//-----------------------------------------------------------------------------
// returns true if the character is allowed in a DNS doman name, false otherwise
//-----------------------------------------------------------------------------
[[nodiscard]] bool V_IsValidDomainNameCharacter( IN_Z const char *pch, IN_OPT intp *pAdvanceBytes );

 // Converts BBCode tags to HTML tags
bool V_BBCodeToHTML( OUT_Z_CAP( nDestSize ) char *pDest, const intp nDestSize, IN_Z_CAP(nInSize) char const *pIn, const intp nInSize );


// helper to identify "mean" spaces, which we don't like in visible identifiers
// such as player Name
[[nodiscard]] bool V_IsMeanSpaceW( wchar_t wch );

// helper to identify characters which are deprecated in Unicode,
// and we simply don't accept
[[nodiscard]] bool V_IsDeprecatedW( wchar_t wch );

//-----------------------------------------------------------------------------
// generic unique name helper functions
//-----------------------------------------------------------------------------

// returns startindex if none found, 2 if "prefix" found, and n+1 if "prefixn" found
template <typename NameArray>
[[nodiscard]] intp V_GenerateUniqueNameIndex( IN_Z const char *prefix, const NameArray &nameArray, intp startindex = 0 )
{
	Assert(prefix);

	if ( prefix == nullptr )
		return 0;

	intp freeindex = startindex;

	intp nNames = nameArray.Count();
	for ( intp i = 0; i < nNames; ++i )
	{
		const char *pName = nameArray[ i ];
		if ( !pName )
			continue;

		const char *pIndexStr = StringAfterPrefix( pName, prefix );
		if ( pIndexStr )
		{
#ifdef PLATFORM_64BITS
			// dimhotepus: Handle 64 bit counters.
			intp index = *pIndexStr ? strtoll( pIndexStr, nullptr, 10 ) : 1;
#else
			intp index = *pIndexStr ? atoi( pIndexStr ) : 1;
#endif
			if ( index >= freeindex )
			{
				// TODO - check that there isn't more junk after the index in pElementName
				freeindex = index + 1;
			}
		}
	}

	return freeindex;
}

template <typename NameArray>
[[nodiscard]] bool V_GenerateUniqueName( OUT_Z_CAP(memsize) char *name, intp memsize, IN_Z const char *prefix, const NameArray &nameArray )
{
	Assert(name);
	Assert(prefix);

	if ( name == nullptr || memsize == 0 )
		return false;

	if ( prefix == nullptr )
	{
		name[ 0 ] = '\0';
		return false;
	}

	intp prefixLength = V_strlen(prefix);
	if ( prefixLength + 1 > memsize )
	{
		name[ 0 ] = '\0';
		return false;
	}

	intp i = V_GenerateUniqueNameIndex(prefix, nameArray);
	if ( i <= 0 )
	{
		V_strncpy( name, prefix, memsize );
		return true;
	}

	intp newlen = prefixLength + static_cast<intp>( log10( i ) ) + 1; //-V2003
	if ( newlen + 1 > memsize )
	{
		V_strncpy( name, prefix, memsize );
		return false;
	}

	V_snprintf( name, memsize, "%s%zd", prefix, i );
	return true;
}

template <typename NameArray, intp memsize>
[[nodiscard]] bool V_GenerateUniqueName( OUT_Z_ARRAY char (&name)[memsize], IN_Z const char *prefix, const NameArray &nameArray )
{
	return V_GenerateUniqueName( name, memsize, prefix, nameArray );
}

//
// This utility class is for performing UTF-8 <-> UTF-16 conversion.
// It is intended for use with function/method parameters.
//
// For example, you can call
//     FunctionTakingUTF16( CStrAutoEncode( utf8_string ).ToWString() )
// or
//     FunctionTakingUTF8( CStrAutoEncode( utf16_string ).ToString() )
//
// The converted string is allocated off the heap, and destroyed when
// the object goes out of scope.
//
// if the string cannot be converted, nullptr is returned.
//
// This class doesn't have any conversion operators; the intention is
// to encourage the developer to get used to having to think about which
// encoding is desired.
//
class CStrAutoEncode
{
public:

	// ctor
	explicit CStrAutoEncode( IN_Z const char *pch )
	{
		m_pch = pch;
		m_pwch = nullptr;
#if !defined( WIN32 ) && !defined(_WIN32)
		m_pucs2 = nullptr;
		m_bCreatedUCS2 = false;
#endif
		m_bCreatedUTF16 = false;
	}

	// ctor
	explicit CStrAutoEncode( IN_Z const wchar_t *pwch )
	{
		m_pch = nullptr;
		m_pwch = pwch;
#if !defined(_WIN32)
		m_pucs2 = nullptr;
		m_bCreatedUCS2 = false;
#endif
		m_bCreatedUTF16 = true;
	}

#if !defined(_WIN32)
	explicit CStrAutoEncode( const ucs2 *pwch )
	{
		m_pch = nullptr;
		m_pwch = nullptr;
		m_pucs2 = pwch;
		m_bCreatedUCS2 = true;
		m_bCreatedUTF16 = false;
	}
#endif

	// returns the UTF-8 string, converting on the fly.
	[[nodiscard]] const char* ToString()
	{
		PopulateUTF8();
		return m_pch;
	}

	// returns the UTF-8 string - a writable pointer.
	// only use this if you don't want to call const_cast
	// yourself. We need this for cases like CreateProcess.
	[[nodiscard]] char* ToStringWritable()
	{
		PopulateUTF8();
		return const_cast< char* >( m_pch );
	}

	// returns the UTF-16 string, converting on the fly.
	[[nodiscard]] const wchar_t* ToWString()
	{
		PopulateUTF16();
		return m_pwch;
	}

#if !defined( WIN32 ) && !defined(_WIN32)
	// returns the UTF-16 string, converting on the fly.
	const ucs2* ToUCS2String()
	{
		PopulateUCS2();
		return m_pucs2;
	}
#endif

	// returns the UTF-16 string - a writable pointer.
	// only use this if you don't want to call const_cast
	// yourself. We need this for cases like CreateProcess.
	[[nodiscard]] wchar_t* ToWStringWritable()
	{
		PopulateUTF16();
		return const_cast< wchar_t* >( m_pwch );
	}

	// dtor
	~CStrAutoEncode()
	{
		// if we're "native unicode" then the UTF-8 string is something we allocated,
		// and vice versa.
		if ( m_bCreatedUTF16 )
		{
			delete [] m_pch;
		}
		else
		{
			delete [] m_pwch;
		}
#if !defined( WIN32 ) && !defined(_WIN32)
		if ( !m_bCreatedUCS2 && m_pucs2 )
			delete [] m_pucs2;
#endif
	}

private:
	// ensure we have done any conversion work required to farm out a
	// UTF-8 encoded string.
	//
	// We perform two heap allocs here; the first one is the worst-case
	// (four bytes per Unicode code point). This is usually quite pessimistic,
	// so we perform a second allocation that's just the size we need.
	void PopulateUTF8()
	{
		if ( !m_bCreatedUTF16 )
			return;					// no work to do
		if ( m_pwch == nullptr )
			return;					// don't have a UTF-16 string to convert
		if ( m_pch != nullptr )
			return;					// already been converted to UTF-8; no work to do

		// each Unicode code point can expand to as many as four bytes in UTF-8; we
		// also need to leave room for the terminating NUL.
		const intp cbMax = 4 * V_wcslen( m_pwch ) + 1; //-V112
		std::unique_ptr<char[]> pchTemp = std::make_unique<char[]>(cbMax);
		if ( Q_WStringToUTF8( m_pwch, pchTemp.get(), cbMax ) )
		{
			m_pch = V_strdup( pchTemp.get() );
		}
	}

	// ensure we have done any conversion work required to farm out a
	// UTF-16 encoded string.
	//
	// We perform two heap allocs here; the first one is the worst-case
	// (one code point per UTF-8 byte). This is sometimes pessimistic,
	// so we perform a second allocation that's just the size we need.
	void PopulateUTF16()
	{
		if ( m_bCreatedUTF16 )
			return;					// no work to do
		if ( m_pch == nullptr )
			return;					// no UTF-8 string to convert
		if ( m_pwch != nullptr )
			return;					// already been converted to UTF-16; no work to do

		const intp cchMax = V_strlen( m_pch ) + 1;
		std::unique_ptr<wchar_t[]> pwchTemp = std::make_unique<wchar_t[]>(cchMax);
		if ( V_UTF8ToUnicode( m_pch, pwchTemp.get(), cchMax * sizeof( wchar_t ) ) )
		{
			m_pwch = V_wcsdup( pwchTemp.get() );
		}
	}

#if !defined(_WIN32)
	// ensure we have done any conversion work required to farm out a
	// UTF-16 encoded string.
	//
	// We perform two heap allocs here; the first one is the worst-case
	// (one code point per UTF-8 byte). This is sometimes pessimistic,
	// so we perform a second allocation that's just the size we need.
	void PopulateUCS2()
	{
		if ( m_bCreatedUCS2 )
			return;
		if ( m_pch == nullptr )
			return;					// no UTF-8 string to convert
		if ( m_pucs2 != nullptr )
			return;					// already been converted to UTF-16; no work to do

		intp cchMax = V_strlen( m_pch ) + 1;
		std::unique_ptr<ucs2[]> pwchTemp = std::make_unique<ucs2[]>( cchMax );
		if ( V_UTF8ToUCS2( m_pch, cchMax, pwchTemp.get(), cchMax * sizeof( ucs2 ) ) )
		{
			intp cchAlloc = cchMax;
			ucs2 *pwchHeap = new ucs2[ cchAlloc ];
			memcpy( pwchHeap, pwchTemp.get(), cchAlloc * sizeof( ucs2 ) );
			m_pucs2 = pwchHeap;
		}
	}
#endif

	// one of these pointers is an owned pointer; whichever
	// one is the encoding OTHER than the one we were initialized
	// with is the pointer we've allocated and must free.
	const char *m_pch;
	const wchar_t *m_pwch;
#if !defined(_WIN32)
	const ucs2 *m_pucs2;
	bool m_bCreatedUCS2;
#endif
	// "created as UTF-16", means our owned string is the UTF-8 string not the UTF-16 one.
	bool m_bCreatedUTF16;

};

// Encodes a string (or binary data) in URL encoding format, see rfc1738 section 2.2.
// Dest buffer should be 3 times the size of source buffer to guarantee it has room to encode.
void Q_URLEncodeRaw( OUT_Z_CAP(nDestLen) char *pchDest, intp nDestLen, IN_Z_CAP(nSourceLen) const char *pchSource, intp nSourceLen );

// Decodes a string (or binary data) from URL encoding format, see rfc1738 section 2.2.
// Dest buffer should be at least as large as source buffer to gurantee room for decode.
// Dest buffer being the same as the source buffer (decode in-place) is explicitly allowed.
//
// Returns the amount of space actually used in the output buffer.  
size_t Q_URLDecodeRaw( OUT_Z_CAP(nDecodeDestLen) char *pchDecodeDest, intp nDecodeDestLen, IN_Z_CAP(nEncodedSourceLen)  const char *pchEncodedSource, intp nEncodedSourceLen );

// trim right whitespace
[[nodiscard]] inline char* TrimRight( IN_Z char *pString )
{
	Assert(pString);

	char *pEnd = pString + V_strlen( pString );
	// trim
	while ( pString < ( pEnd-- ) )
	{
		if ( static_cast<uint>( *pEnd ) <= static_cast<uint>( ' ' ) )
		{
			*pEnd = '\0';
		}
		else
			break;
	}
	return pString;
}

[[nodiscard]] inline const char * SkipBlanks( IN_Z const char *pString )
{
	Assert(pString);

	const char *p = pString;
	while ( *p && static_cast<uint>( *p ) <= static_cast<uint>( ' ' ) )
	{
		p++;
	}
	return p;
}

[[nodiscard]] inline intp V_strcspn( IN_Z const char *s1, IN_Z const char *search )		{ return static_cast<intp>( strcspn( s1, search ) ); }
// Encodes a string (or binary data) in URL encoding format, this isn't the strict rfc1738 format, but instead uses + for spaces.  
// This is for historical reasons and HTML spec foolishness that lead to + becoming a de facto standard for spaces when encoding form data.
// Dest buffer should be 3 times the size of source buffer to guarantee it has room to encode.
void Q_URLEncode( OUT_Z_CAP(nDestLen) char *pchDest, intp nDestLen, IN_Z_CAP(nSourceLen) const char *pchSource, intp nSourceLen );

// Decodes a string (or binary data) in URL encoding format, this isn't the strict rfc1738 format, but instead uses + for spaces.  
// This is for historical reasons and HTML spec foolishness that lead to + becoming a de facto standard for spaces when encoding form data.
// Dest buffer should be at least as large as source buffer to gurantee room for decode.
// Dest buffer being the same as the source buffer (decode in-place) is explicitly allowed.
//
// Returns the amount of space actually used in the output buffer.  
size_t Q_URLDecode( OUT_CAP(nDecodeDestLen) char *pchDecodeDest, intp nDecodeDestLen, IN_Z_CAP(nEncodedSourceLen) const char *pchEncodedSource, intp nEncodedSourceLen );


// NOTE: This is for backward compatability!
// We need to DLL-export the Q methods in vstdlib but not link to them in other projects
#if !defined( VSTDLIB_BACKWARD_COMPAT )

#define Q_memset				V_memset
#define Q_memcpy				V_memcpy
#define Q_memmove				V_memmove
#define Q_memcmp				V_memcmp
#define Q_strlen				V_strlen
#define Q_strcpy				V_strcpy
#define Q_strrchr				V_strrchr
#define Q_strcmp				V_strcmp
#define Q_wcscmp				V_wcscmp
#define Q_stricmp				V_stricmp
#define Q_strstr				V_strstr
#define Q_strupr				V_strupr
#define Q_strlower				V_strlower
#define Q_wcslen				V_wcslen
#define	Q_strncmp				V_strncmp 
#define	Q_strcasecmp			V_strcasecmp
#define	Q_strncasecmp			V_strncasecmp
#define	Q_strnicmp				V_strnicmp
#define	Q_atoi					V_atoi
#define	Q_atoi64				V_atoi64
#define Q_atoui64				V_atoui64
#define	Q_atof					V_atof
#define	Q_stristr				V_stristr
#define	Q_strnistr				V_strnistr
#define	Q_strnchr				V_strnchr
#define Q_normalizeFloatString	V_normalizeFloatString
#define Q_strncpy				V_strncpy
#define Q_snprintf				V_snprintf
#define Q_wcsncpy				V_wcsncpy
#define Q_strncat				V_strncat
#define Q_strnlwr				V_strnlwr
#define Q_vsnprintf				V_vsnprintf
#define Q_vsnprintfRet			V_vsnprintfRet
#define Q_pretifymem			V_pretifymem
#define Q_pretifynum			V_pretifynum
#define Q_UTF8ToUnicode			V_UTF8ToUnicode
#define Q_UnicodeToUTF8			V_UnicodeToUTF8
#define Q_hextobinary			V_hextobinary
#define Q_binarytohex			V_binarytohex
#define Q_FileBase				V_FileBase
#define Q_StripTrailingSlash	V_StripTrailingSlash
#define Q_StripExtension		V_StripExtension
#define	Q_DefaultExtension		V_DefaultExtension
#define Q_SetExtension			V_SetExtension
#define Q_StripFilename			V_StripFilename
#define Q_StripLastDir			V_StripLastDir
#define Q_UnqualifiedFileName	V_UnqualifiedFileName
#define Q_ComposeFileName		V_ComposeFileName
#define Q_ExtractFilePath		V_ExtractFilePath
#define Q_ExtractFileExtension	V_ExtractFileExtension
#define Q_GetFileExtension		V_GetFileExtension
#define Q_RemoveDotSlashes		V_RemoveDotSlashes
#define Q_MakeAbsolutePath		V_MakeAbsolutePath
#define Q_AppendSlash			V_AppendSlash
#define Q_IsAbsolutePath		V_IsAbsolutePath
#define Q_StrSubst				V_StrSubst
#define Q_SplitString			V_SplitString
#define Q_SplitString2			V_SplitString2
#define Q_StrSlice				V_StrSlice
#define Q_StrLeft				V_StrLeft
#define Q_StrRight				V_StrRight
#define Q_FixSlashes			V_FixSlashes
#define Q_strtowcs				V_strtowcs
#define Q_wcstostr				V_wcstostr
#define Q_strcat				V_strcat
#define Q_GenerateUniqueNameIndex	V_GenerateUniqueNameIndex
#define Q_GenerateUniqueName		V_GenerateUniqueName
#define Q_MakeRelativePath		V_MakeRelativePath
#define Q_qsort_s				V_qsort_s

[[nodiscard]] inline bool Q_isempty(IN_Z const char *v) { return V_isempty(v); }

[[nodiscard]] inline bool Q_isempty(IN_Z const wchar_t *v) { return v[0] == L'\0'; }

template<size_t size>
[[nodiscard]] constexpr inline bool Q_isempty(IN_Z_ARRAY char (&v)[size]) { return v[0] == '\0'; }

template<size_t size>
[[nodiscard]] constexpr inline bool Q_isempty(IN_Z_ARRAY wchar_t (&v)[size]) { return v[0] == L'\0'; }

// dimhotepus: Fast integral -> char conversion.
template<size_t size, typename TIntegral>
inline std::enable_if_t<std::is_integral_v<TIntegral>, std::errc>
V_to_chars(OUT_Z_ARRAY char (&buffer)[size], TIntegral value, int base = 10)
{
	// Do not NULL terminate, so be careful.
	const std::to_chars_result result{std::to_chars(buffer, buffer + size, value, base)};
	if (result.ec == std::errc{})
	{
		// If has space for NULL terminate.
		if (result.ptr != buffer + size)
		{
			*result.ptr = '\0';
			return std::errc{};
		}
	}
#ifdef _DEBUG
	const std::string message = std::make_error_code
	(
		result.ec != std::errc{} ? result.ec : std::errc::value_too_large
	).message();
	AssertMsg(false, "Unable to convert integral to chars: %s.\n", message.c_str());
#endif
	// Overflow.
	*buffer = '\0';
	return std::errc::value_too_large;
}

// dimhotepus: Fast floating point -> char conversion.
template<size_t size, typename TFloat>
inline std::enable_if_t<std::is_floating_point_v<TFloat>, std::errc>
V_to_chars(OUT_Z_ARRAY char (&buffer)[size], TFloat value)
{
	// Do not NULL terminate, so be careful.
	const std::to_chars_result result{std::to_chars(buffer, buffer + size, value)};
	if (result.ec == std::errc{})
	{
		// If has space for NULL terminate.
		if (result.ptr != buffer + size)
		{
			*result.ptr = '\0';
			return std::errc{};
		}
	}
#ifdef _DEBUG
	const std::string message = std::make_error_code
	(
		result.ec != std::errc{} ? result.ec : std::errc::value_too_large
	).message();
	AssertMsg(false, "Unable to convert floating point to chars: %s.\n", message.c_str());
#endif
	// Overflow.
	*buffer = '\0';
	return std::errc::value_too_large;
}

// Removes any possible formatting codes and double quote characters from the input string
template<intp maxlen>
void V_StripInvalidCharacters( INOUT_Z_ARRAY char (&pszInput)[maxlen] )
{
	char szOutput[maxlen];
	
	char *pIn = pszInput;
	char *pOut = szOutput;

	*pOut = '\0';

	while ( *pIn )
	{
		if ( ( *pIn != '"' ) && ( *pIn != '%' ) )
		{
			*pOut++ = *pIn;
		}
		pIn++;
	}

	*pOut = '\0';

	// Copy back over, in place
	V_strcpy_safe( pszInput, szOutput );
}

#endif // !defined( VSTDLIB_DLL_EXPORT )


#ifdef POSIX
#define FMT_WS L"%ls"
#else
#define FMT_WS L"%s"
#endif



// Strip white space at the beginning and end of a string
intp V_StrTrim( IN_Z char *pStr );


#endif	// TIER1_STRTOOLS_H
