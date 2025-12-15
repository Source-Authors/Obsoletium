//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef COMMON_H
#define COMMON_H
#pragma once

#ifndef WORLDSIZE_H
#include "worldsize.h"
#endif

#include "basetypes.h"
#include "filesystem.h"
#include "mathlib/vector.h" // @Note (toml 05-01-02): solely for definition of QAngle
#include "qlimits.h"
#define INCLUDED_STEAM2_USERID_STRUCTS	
#include "steamcommon.h"
#include "steam/steamclientpublic.h"


class Vector;
struct cache_user_t;

//============================================================================

// dimhotepus: Increase 1024 -> 65535 to speedup copying.
constexpr inline int COM_COPY_CHUNK_SIZE{65535};  // For copying operations

// dimhotepus: Expose to the public.
constexpr inline int COM_TOKEN_MAX_LENGTH{1024};

#include "tier1/strtools.h"

//============================================================================
char *COM_StringCopy(IN_Z const char *text);	// allocates memory and copys text
void COM_StringFree(IN_Z const char *text);	// frees memory allocated by COM_StringCopy
void COM_AddNoise( INOUT_BYTECAP(number) unsigned char *data, int length, int number ); // Changes n random bits in a data block

//============================================================================
extern void COM_WriteFile (IN_Z const char *filename, IN_BYTECAP(len) void *data, int len);
extern int COM_OpenFile (IN_Z const char *filename, FileHandle_t* file );
extern void COM_CloseFile ( FileHandle_t hFile );
extern void COM_CreatePath (IN_Z const char *path);
extern int COM_FileSize (IN_Z const char *filename);
extern int COM_ExpandFilename ( INOUT_Z_CAP(maxlength) char *filename, int maxlength);
extern byte *COM_LoadFile (IN_Z const char *path, int usehunk, int *pLength);
extern bool COM_IsValidPath (IN_Z const char *pszFilename );
extern bool COM_IsValidLogFilename (IN_Z const char *pszFilename );

[[deprecated("Not thread-safe. Use COM_Parse with token as arg.")]] const char *COM_Parse(IN_Z const char *data);
[[deprecated("Not thread-safe. Use COM_ParseLine with token as arg.")]] const char *COM_ParseLine ( IN_Z const char *data);
[[nodiscard]] int COM_TokenWaiting( IN_Z const char *buffer );

const char *COM_Parse ( IN_Z const char *data, OUT_Z_CAP(tokenSize) char* token, size_t tokenSize );
const char *COM_ParseLine( IN_Z const char *data, OUT_Z_CAP(tokenSize) char* token, size_t tokenSize );

template<size_t tokenSize>
const char *COM_Parse ( IN_Z const char *data, OUT_Z_ARRAY char (&token)[tokenSize] )
{
	return COM_Parse( data, token, tokenSize );
}

template<size_t tokenSize>
const char *COM_ParseLine ( IN_Z const char *data, OUT_Z_ARRAY char (&token)[tokenSize] )
{
	return COM_ParseLine( data, token, tokenSize );
}

extern bool com_ignorecolons;
extern char com_token[COM_TOKEN_MAX_LENGTH];

void COM_Init();
void COM_Shutdown();
[[nodiscard]] bool COM_CheckGameDirectory( IN_Z const char *gamedir );
void COM_ParseDirectoryFromCmd( const char *pCmdName, char *pDirName, int maxlen, const char *pDefault );

template<typename B>
[[nodiscard]] constexpr inline auto Bits2Bytes(B b)
{
	return (b + 7) >> 3;
}

// returns a temp buffer of at least 512 bytes 
[[nodiscard]] char	*tmpstr512();
// does a varargs printf into a temp buffer.
// Returns char* because of bad historical reasons.
[[nodiscard]] char	*va(PRINTF_FORMAT_STRING const char *format, ...) FMTFUNCTION( 1, 2 );
// prints a vector into a temp buffer.
[[nodiscard]] const char    *vstr(Vector& v);

//============================================================================
extern	char	com_basedir[MAX_OSPATH];
extern	char	com_gamedir[MAX_OSPATH];

byte *COM_LoadStackFile (IN_Z const char *path, OUT_BYTECAP(bufsize) void *buffer, int bufsize, int& filesize );
void COM_LoadCacheFile (IN_Z const char *path, cache_user_t *cu);
byte* COM_LoadFile (IN_Z const char *path, int usehunk, int *pLength);

void COM_CopyFileChunk( FileHandle_t dst, FileHandle_t src, int nSize );
bool COM_CopyFile( IN_Z const char *pSourcePath, IN_Z const char *pDestPath );

void COM_SetupLogDir( IN_Z const char *mapname );
void COM_GetGameDir( OUT_Z_CAP(maxlen) char *szGameDir, int maxlen );
template<int maxlen>
void COM_GetGameDir( OUT_Z_ARRAY char (&szGameDir)[maxlen] )
{
	COM_GetGameDir( szGameDir, maxlen );
}

[[nodiscard]] int COM_CompareFileTime( IN_Z const char *filename1, IN_Z const char *filename2, int *iCompare );
[[nodiscard]] int COM_GetFileTime(IN_Z const char *pFileName);
const char *COM_ParseFile( IN_Z const char *data, OUT_Z_CAP(maxtoken) char *token, intp maxtoken );
template<intp tokenSize>
const char *COM_ParseFile( IN_Z const char *data, OUT_Z_ARRAY char (&token)[tokenSize] )
{
	return COM_ParseFile( data, token, tokenSize );
}

extern char gszDisconnectReason[256];
extern char gszExtendedDisconnectReason[256];
extern bool gfExtendedError;
extern uint8 g_eSteamLoginFailure;
void COM_ExplainDisconnection( bool bPrint, PRINTF_FORMAT_STRING const char *fmt, ... );

[[nodiscard]] const char *COM_DXLevelToString( int dxlevel );  // convert DX level to string

void COM_Log( const char *pszFile, PRINTF_FORMAT_STRING const char *fmt, ...) FMTFUNCTION( 2, 3 ); // Log a debug message to specified file ( if pszFile == NULL uses c:\\hllog.txt )
void COM_LogString( char const *pchFile, char const *pchString );

[[nodiscard]] const char *COM_FormatSeconds( int seconds ); // returns seconds as hh:mm:ss string

[[nodiscard]] const char *COM_GetModDirectory(); // return the mod dir (rather than the complete -game param, which can be a path)

void *COM_CompressBuffer_LZSS( IN_BYTECAP(sourceLen) const void *source, unsigned int sourceLen, OUT_BYTECAP(maxCompressedLen) unsigned int *compressedLen, unsigned int maxCompressedLen = 0 );
[[nodiscard]] bool COM_BufferToBufferCompress_LZSS( OUT_BYTECAP(*destLen) void *dest, unsigned int *destLen, IN_BYTECAP(sourceLen) const void *source, unsigned int sourceLen );
[[nodiscard]] unsigned int COM_GetIdealDestinationCompressionBufferSize_LZSS( unsigned int uncompressedSize );

[[nodiscard]] void *COM_CompressBuffer_Snappy( IN_BYTECAP(sourceLen) const void *source, unsigned int sourceLen, unsigned int *compressedLen, unsigned int maxCompressedLen = 0 );
[[nodiscard]] bool COM_BufferToBufferCompress_Snappy( OUT_BYTECAP(*destLen) void *dest, unsigned int *destLen, IN_BYTECAP(sourceLen) const void *source, unsigned int sourceLen );
[[nodiscard]] unsigned int COM_GetIdealDestinationCompressionBufferSize_Snappy( unsigned int uncompressedSize );

/// Fetch ideal working buffer size.  You should allocate the buffer you wish to compress into
/// at least this big, in order to get the best performance when using COM_BufferToBufferCompress
[[nodiscard]] inline unsigned int COM_GetIdealDestinationCompressionBufferSize( unsigned int uncompressedSize )
{
	return COM_GetIdealDestinationCompressionBufferSize_LZSS( uncompressedSize );
}

/// Compress the source data into a newly allocated buffer.  Returns the buffer and its
/// size.  Note that the buffer may have been allocated to a larger size than necessary,
/// and the compressed size may be larger than the size of the input!
///
/// If maxCompressedLen is nonzero, then we will fail compression if the compressed data
/// exceeds this size.  Depending on the compressor used, we might be able to terminate
/// early in this case
inline void *COM_CompressBuffer( IN_BYTECAP(sourceLen) const void *source, unsigned int sourceLen, OUT_BYTECAP(maxCompressedLen) unsigned int *compressedLen, unsigned int maxCompressedLen = 0 )
{
	return COM_CompressBuffer_LZSS( source, sourceLen, compressedLen, maxCompressedLen );
}

/// Compress data to the specified buffer.  Returns false if compression fails or the data cannot fit into
/// the specified buffer.  If false is returned, the destination buffer and size field are not modified.
/// (Note that this differs from previous behaviour.)
inline bool COM_BufferToBufferCompress( OUT_BYTECAP(*destLen) void *dest, unsigned int *destLen, IN_BYTECAP(sourceLen) const void *source, unsigned int sourceLen )
{
	return COM_BufferToBufferCompress_LZSS( dest, destLen, source, sourceLen );
}

/// Returns true if compression succeeded, false otherwise
bool COM_BufferToBufferDecompress( OUT_BYTECAP(*destLen) void *dest, unsigned int *destLen, IN_BYTECAP(sourceLen) const void *source, unsigned int sourceLen );

/// Fetch size of the decompressed data in a buffer that was created using COM_BufferToBufferCompress.
/// Returns -1 if buffer does not appear to be compressed.
[[nodiscard]] int COM_GetUncompressedSize( IN_BYTECAP(compressedLen) const void *compressed, unsigned int compressedLen );

#endif // COMMON_H
