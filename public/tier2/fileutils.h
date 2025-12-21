//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A higher level link library for general use in the game and tools.
//
//===========================================================================//

#ifndef FILEUTILS_H
#define FILEUTILS_H

#include "tier0/platform.h"
#include "tier2/tier2.h"
#include "filesystem.h"

// Builds a directory which is a subdirectory of the current mod
void GetModSubdirectory( const char *pSubDir, OUT_Z_CAP(nBufLen) char *pBuf, intp nBufLen );

// Builds a directory which is a subdirectory of the current mod
template<intp bufferSize>
void GetModSubdirectory(const char *pSubDir, OUT_Z_ARRAY char (&pBuf)[bufferSize] )
{
	GetModSubdirectory( pSubDir, pBuf, bufferSize );
}

// Builds a directory which is a subdirectory of the current mod's *content*
void GetModContentSubdirectory( const char *pSubDir, OUT_Z_CAP(nBufLen) char *pBuf, intp nBufLen );

// Builds a directory which is a subdirectory of the current mod's *content*
template <intp bufferSize>
void GetModContentSubdirectory( const char *pSubDir, OUT_Z_ARRAY char (&pBuf)[bufferSize] )
{
	GetModContentSubdirectory( pSubDir, pBuf, bufferSize );
}

// Generates a filename under the 'game' subdirectory given a subdirectory of 'content'
void ComputeModFilename( const char *pContentFileName, OUT_Z_CAP(nBufLen) char *pBuf, intp nBufLen );

// Generates a filename under the 'game' subdirectory given a subdirectory of 'content'
template<intp bufferSize>
void ComputeModFilename( const char *pContentFileName, OUT_Z_ARRAY char (&pBuf)[bufferSize] )
{
	ComputeModFilename( pContentFileName, pBuf, bufferSize );
}

// Generates a filename under the 'content' subdirectory given a subdirectory of 'game'
void ComputeModContentFilename( const char *pGameFileName, OUT_Z_CAP(nBufLen) char *pBuf, intp nBufLen );

// Generates a filename under the 'content' subdirectory given a subdirectory of 'game'
template<intp bufferSize>
void ComputeModContentFilename( const char *pGameFileName, OUT_Z_ARRAY char (&pBuf)[bufferSize] )
{
	ComputeModContentFilename( pGameFileName, pBuf, bufferSize );
}

// Builds a list of all files under a directory with a particular extension
void AddFilesToList( CUtlVector< CUtlString > &list, const char *pDirectory, const char *pPath, const char *pExtension );

// Returns the search path as a list of paths
void GetSearchPath( CUtlVector< CUtlString > &path, const char *pPathID );

// Given file name generate a full path using the following rules.
// 1. if its full path already return
// 2. if its a relative path try to find it under the path id
// 3. if find fails treat relative path as relative to the current dir
bool GenerateFullPath( const char *pFileName, char const *pPathID, OUT_Z_CAP(nBufLen) char *pBuf, intp nBufLen );

// Given file name generate a full path using the following rules.
// 1. if its full path already return
// 2. if its a relative path try to find it under the path id
// 3. if find fails treat relative path as relative to the current dir
template <intp bufferSize>
bool GenerateFullPath( const char *pFileName, char const *pPathID, OUT_Z_ARRAY char (&pBuf)[bufferSize] )
{
	return GenerateFullPath( pFileName, pPathID, pBuf, bufferSize );
}

// Generates a .360 file if it doesn't exist or is out of sync with the pc source file
#define UOC_FAIL		-1
#define UOC_NOT_CREATED	0
#define UOC_CREATED		1
using CreateCallback_t = bool (*)( const char *pSourceName, const char *pTargetName, const char *pPathID, void *pExtraData );
int UpdateOrCreate( const char *pSourceName, char *pTargetName, int targetLen, const char *pPathID, CreateCallback_t pfnCreate, bool bForce = false, void *pExtraData = nullptr );

char *CreateX360Filename( const char *pSourceName, char *pTargetName, int targetLen );

FORCEINLINE const char *AdjustFileExtensionForPlatform( const char *pSourceName, [[maybe_unused]] char *pTargetName, [[maybe_unused]] int targetLen )
{
	return pSourceName;
}

// simple file classes. File I/O mode (text/binary, read/write) is based upon the subclass chosen.
// classes with the word Required on them abort with a message if the file can't be opened.
// destructores close the file handle, or it can be explicitly closed with the Close() method.

class CBaseFile
{
public:
	FileHandle_t m_FileHandle;

	CBaseFile() : m_FileHandle{FILESYSTEM_INVALID_HANDLE}
	{
	}

	~CBaseFile()
	{
		Close();
	}

	[[nodiscard]] FileHandle_t Handle() const
	{
		return m_FileHandle;
	}

	void Close()
	{
		if ( m_FileHandle != FILESYSTEM_INVALID_HANDLE )
		{
			g_pFullFileSystem->Close( m_FileHandle );
			m_FileHandle = FILESYSTEM_INVALID_HANDLE;
		}
	}

	void Open( char const *fname, char const *modes )
	{
		Close();
		m_FileHandle = g_pFullFileSystem->Open( fname, modes );
	}

	char *ReadLine( OUT_Z_CAP(maxChars) char *pOutput, int maxChars )
	{
		return g_pFullFileSystem->ReadLine( pOutput, maxChars, m_FileHandle );
	}

	template<int maxChars>
	char *ReadLine( OUT_Z_ARRAY char (&pOutput)[maxChars] )
	{
		return ReadLine( pOutput, maxChars );
	}

	// read every line of the file into a vector of strings
	void ReadLines( CUtlStringList &sList, int nMaxLineLength = 2048 );

	int Read( void* pOutput, int size )
	{
		return g_pFullFileSystem->Read( pOutput, size, m_FileHandle );
	}

	void MustRead( void* pOutput, int size )
	{
		int ret = Read( pOutput, size );
		if (ret != size )
			Error("failed to read %d bytes\n", size );
	}
	
	int Write( void const* pInput, int size)
	{
		return g_pFullFileSystem->Write( pInput, size, m_FileHandle );
	}


	// {Get|Put}{Int|Float} read and write ints and floats from a file in x86 order, swapping on
	// input for big-endian systems.
	void PutInt( int n )
	{
		int n1=LittleDWord( n );
		Write(&n1, sizeof( n1 ) );
	}

	[[nodiscard]] int GetInt()
	{
		int ret;
		MustRead( &ret, sizeof( ret ));
		return LittleDWord( ret );
	}

	[[nodiscard]] float GetFloat()
	{
		float ret;
		MustRead( &ret, sizeof( ret ));
		LittleFloat( &ret, &ret );
		return ret;
	}
	void PutFloat( float f )
	{
		LittleFloat( &f, &f );
		Write( &f, sizeof( f ) );
	}

	[[nodiscard]] bool IsOk()
	{
		return ( m_FileHandle != FILESYSTEM_INVALID_HANDLE) &&
			( g_pFullFileSystem->IsOk( m_FileHandle ) );
	}

	void Seek( int pos, FileSystemSeek_t nSeekType = FILESYSTEM_SEEK_HEAD )
	{
		g_pFullFileSystem->Seek( m_FileHandle, pos, nSeekType );
	}

	[[nodiscard]] unsigned int Tell()
	{
		return g_pFullFileSystem->Tell( m_FileHandle );
	}

	[[nodiscard]] unsigned int Size()
	{
		Assert( IsOk() );
		return g_pFullFileSystem->Size( m_FileHandle );
	}

	void ReadFile( CUtlBuffer &dataBuf );
};

class COutputFile : public CBaseFile
{
public:
	void Open( char const *pFname )
	{
		CBaseFile::Open( pFname, "wb" );
	}

	COutputFile( char const *pFname ) : CBaseFile()
	{
		Open( pFname );
	}

	COutputFile() : CBaseFile()
	{
	}
};

class COutputTextFile : public CBaseFile
{
public:
	void Open( char const *pFname )
	{
		CBaseFile::Open( pFname, "w" );
	}

	COutputTextFile( char const *pFname ) : CBaseFile()
	{
		Open( pFname );
	}

	COutputTextFile() : CBaseFile()
	{
	}
};

class CAppendTextFile : public CBaseFile
{
public:
	void Open( char const *pFname )
	{
		CBaseFile::Open( pFname, "a+" );
	}

	CAppendTextFile( char const *pFname ) : CBaseFile()
	{
		Open( pFname );
	}

	CAppendTextFile() : CBaseFile()
	{
	}
};

class CInputFile : public CBaseFile
{
public:
	void Open( char const *pFname )
	{
		CBaseFile::Open( pFname, "rb" );
	}

	CInputFile( char const *pFname ) : CBaseFile()
	{
		Open( pFname );
	}
	CInputFile() : CBaseFile()
	{
	}
};

class CInputTextFile : public CBaseFile
{
public:
	void Open( char const *pFname )
	{
		CBaseFile::Open( pFname, "r" );
	}

	CInputTextFile( char const *pFname ) : CBaseFile()
	{
		Open( pFname );
	}
	CInputTextFile() : CBaseFile()
	{
	}
	

};

class CRequiredInputTextFile : public CBaseFile
{
public:
	void Open( char const *pFname )
	{
		CBaseFile::Open( pFname, "r" );
		if ( ! IsOk() )
		{
			Error("error opening required file %s\n", pFname );
		}
	}

	CRequiredInputTextFile( char const *pFname ) : CBaseFile()
	{
		Open( pFname );
	}
	CRequiredInputTextFile() : CBaseFile()
	{
	}
};

class CRequiredInputFile : public CBaseFile
{
public:
	void Open( char const *pFname )
	{
		CBaseFile::Open( pFname, "rb" );
		if ( ! IsOk() )
		{
			Error("error opening required file %s\n", pFname );
		}
	}

	CRequiredInputFile( char const *pFname ) : CBaseFile()
	{
		Open( pFname );
	}
	CRequiredInputFile() : CBaseFile()
	{
	}
};

#endif // FILEUTILS_H

