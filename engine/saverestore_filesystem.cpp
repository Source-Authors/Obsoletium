//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Filesystem abstraction for CSaveRestore - allows for storing temp save files
//			either in memory or on disk.
//
//===========================================================================//

#ifdef _WIN32
#include <winerror.h>
#endif

#include "filesystem_engine.h"
#include "saverestore_filesystem.h"
#include "host_saverestore.h"
#include "host.h"
#include "sys.h"
#include "tier1/utlbuffer.h"
#include "tier1/lzss.h"
#include "tier1/convar.h"
#include "ixboxsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar save_spew;

#define SaveMsg if ( !save_spew.GetBool() ) ; else Msg

void SaveInMemoryCallback( IConVar *var, const char *pOldString, float flOldValue );

ConVar save_in_memory( "save_in_memory", "0", 0, "Set to 1 to save to memory instead of disk (Xbox 360)", SaveInMemoryCallback );

CON_COMMAND( audit_save_in_memory, "Audit the memory usage and files in the save-to-memory system" )
{
	if ( !IsX360() )
		return;

	g_pSaveRestoreFileSystem->AuditFiles();
}

#define FILECOPYBUFSIZE (1024 * 1024)

//-----------------------------------------------------------------------------
// Purpose: Copy one file to another file
//-----------------------------------------------------------------------------
static bool FileCopy( FileHandle_t pOutput, FileHandle_t pInput, int fileSize )
{
	// allocate a reasonably large file copy buffer, since otherwise write performance under steam suffers
	char	*buf = (char *)malloc(FILECOPYBUFSIZE);
	int		size;
	int		readSize;
	bool	success = true;

	while ( fileSize > 0 )
	{
		if ( fileSize > FILECOPYBUFSIZE )
			size = FILECOPYBUFSIZE;
		else
			size = fileSize;
		if ( ( readSize = g_pSaveRestoreFileSystem->Read( buf, size, pInput ) ) < size )
		{
			Warning( "Unexpected end of file expanding save game\n" );
			fileSize = 0;
			success = false;
			break;
		}
		g_pSaveRestoreFileSystem->Write( buf, readSize, pOutput );
		
		fileSize -= size;
	}

	free(buf);
	return success;
}

struct filelistelem_t
{
	char szFileName[MAX_PATH];
};

//-----------------------------------------------------------------------------
// Purpose: Implementation to execute traditional save to disk behavior
//-----------------------------------------------------------------------------
class CSaveRestoreFileSystemPassthrough : public ISaveRestoreFileSystem
{
public:
	CSaveRestoreFileSystemPassthrough() {}
	
	bool FileExists( const char *pFileName, const char *pPathID )
	{
		return g_pFileSystem->FileExists( pFileName, pPathID );
	}

	void RemoveFile( char const* pRelativePath, const char *pathID )
	{
		g_pFileSystem->RemoveFile( pRelativePath, pathID );
	}

	void RenameFile( char const *pOldPath, char const *pNewPath, const char *pathID )
	{
		g_pFileSystem->RenameFile( pOldPath, pNewPath, pathID );
	}

	void AsyncFinishAllWrites( void )
	{
		g_pFileSystem->AsyncFinishAllWrites();
	}

	FileHandle_t Open( const char *pFullName, const char *pOptions, const char *pathID )
	{
		return g_pFileSystem->OpenEx( pFullName, pOptions, FSOPEN_NEVERINPACK, pathID );
	}

	void Close( FileHandle_t hSaveFile )
	{
		g_pFileSystem->Close( hSaveFile );
	}

	int Read( void *pOutput, int size, FileHandle_t hFile )
	{
		return g_pFileSystem->Read( pOutput, size, hFile );
	}

	int Write( void const* pInput, int size, FileHandle_t hFile )
	{
		return g_pFileSystem->Write( pInput, size, hFile );
	}

	FSAsyncStatus_t AsyncWrite( const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, bool bAppend, FSAsyncControl_t *pControl )
	{
		SaveMsg( "AsyncWrite (%s/%d)...\n", pFileName, nSrcBytes );
		return g_pFileSystem->AsyncWrite( pFileName, pSrc, nSrcBytes, bFreeMemory, bAppend, pControl );
	}

	void Seek( FileHandle_t hFile, int pos, FileSystemSeek_t method )
	{
		g_pFileSystem->Seek( hFile, pos, method );
	}

	unsigned int Tell( FileHandle_t hFile )
	{
		return g_pFileSystem->Tell( hFile );
	}

	unsigned int Size( FileHandle_t hFile )
	{
		return g_pFileSystem->Size( hFile );
	}

	unsigned int Size( const char *pFileName, const char *pPathID )
	{
		return g_pFileSystem->Size( pFileName, pPathID );
	}

	FSAsyncStatus_t AsyncFinish( FSAsyncControl_t hControl, bool wait )
	{
		return g_pFileSystem->AsyncFinish( hControl, wait );
	}

	void AsyncRelease( FSAsyncControl_t hControl )
	{
		g_pFileSystem->AsyncRelease( hControl );
	}

	FSAsyncStatus_t AsyncAppend(const char *pFileName, const void *pSrc, int nSrcBytes, bool bFreeMemory, FSAsyncControl_t *pControl )
	{
		return g_pFileSystem->AsyncAppend( pFileName, pSrc, nSrcBytes, bFreeMemory, pControl );
	}

	FSAsyncStatus_t AsyncAppendFile(const char *pDestFileName, const char *pSrcFileName, FSAsyncControl_t *pControl )
	{
		return g_pFileSystem->AsyncAppendFile( pDestFileName, pSrcFileName, pControl );
	}

	//-----------------------------------------------------------------------------
	// Purpose: Copies the contents of the save directory into a single file
	//-----------------------------------------------------------------------------
	void DirectoryCopy( const char *pPath, const char *pDestFileName )
	{
		SaveMsg( "DirectoryCopy....\n");

		CUtlVector<filelistelem_t> list;

		// force the writes to finish before trying to get the size/existence of a file
		// @TODO: don't need this if retain sizes for files written earlier in process
		SaveMsg( "DirectoryCopy: AsyncFinishAllWrites\n");
		g_pFileSystem->AsyncFinishAllWrites();

		// build the directory list
		char basefindfn[ MAX_PATH ];
		const char *findfn = Sys_FindFirstEx(pPath, "DEFAULT_WRITE_PATH", basefindfn, sizeof( basefindfn ) );
		while ( findfn )
		{
			intp index = list.AddToTail();
			memset( list[index].szFileName, 0, sizeof(list[index].szFileName) );
			Q_strncpy( list[index].szFileName, findfn, sizeof(list[index].szFileName) );

			findfn = Sys_FindNext( basefindfn, sizeof( basefindfn ) );
		}
		Sys_FindClose();

		// write the list of files to the save file
		char szName[MAX_PATH];
		for ( intp i = 0; i < list.Count(); i++ )
		{
			Q_snprintf( szName, sizeof( szName ), "%s/%s", saverestore->GetSaveDir(), list[i].szFileName );
			Q_FixSlashes( szName );

			unsigned fileSize = g_pFileSystem->Size( szName );
			if ( fileSize )
			{
				Assert( sizeof(list[i].szFileName) == MAX_PATH );

				SaveMsg( "DirectoryCopy: AsyncAppend %s, %s\n", szName, pDestFileName );
				g_pFileSystem->AsyncAppend( pDestFileName, memcpy( new char[MAX_PATH], list[i].szFileName, MAX_PATH), MAX_PATH, true );		// Filename can only be as long as a map name + extension
				// dimhotepus: ASAN catch, AsyncWrite will delete[] it. Also expect char[].
				char *sizeAsString = new char[sizeof(fileSize)];
				memcpy( sizeAsString, &fileSize, sizeof(fileSize) );
				g_pFileSystem->AsyncAppend( pDestFileName, sizeAsString, sizeof(fileSize), true );
				g_pFileSystem->AsyncAppendFile( pDestFileName, szName );
			}
		}
	}

	//-----------------------------------------------------------------------------
	// Purpose: Extracts all the files contained within pFile
	//-----------------------------------------------------------------------------
	bool DirectoryExtract( FileHandle_t pFile, int fileCount, bool bIsXSave )
	{
		int				fileSize;
		FileHandle_t	pCopy;
		char			szName[ MAX_PATH ], fileName[ MAX_PATH ];
		bool			success = true;

		for ( int i = 0; i < fileCount && success; i++ )
		{
			// Filename can only be as long as a map name + extension
			if ( g_pSaveRestoreFileSystem->Read( fileName, MAX_PATH, pFile ) != MAX_PATH )
				return false;

			if ( g_pSaveRestoreFileSystem->Read( &fileSize, sizeof(int), pFile ) != sizeof(int) )
				return false;

			if ( !fileSize )
				return false;

			if ( !bIsXSave )
			{
				Q_snprintf( szName, sizeof( szName ), "%s/%s", saverestore->GetSaveDir(), fileName );
			}
			else
			{
				Q_snprintf( szName, sizeof( szName ), "%s:\\%s", GetCurrentMod(), fileName );
			}

			Q_FixSlashes( szName );
			pCopy = g_pSaveRestoreFileSystem->Open( szName, "wb", "MOD" );
			if ( !pCopy )
				return false;
			success = FileCopy( pCopy, pFile, fileSize );
			g_pSaveRestoreFileSystem->Close( pCopy );
		}

		return success;
	}

	//-----------------------------------------------------------------------------
	// Purpose: returns the number of files in the specified filter
	//-----------------------------------------------------------------------------
	int DirectoryCount( const char *pPath )
	{
		int count = 0;
		const char *findfn = Sys_FindFirstEx( pPath, "DEFAULT_WRITE_PATH", NULL, 0 );

		while ( findfn != NULL )
		{
			count++;
			findfn = Sys_FindNext(NULL, 0 );
		}
		Sys_FindClose();

		return count;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Clears the save directory of all temporary files (*.hl)
	//-----------------------------------------------------------------------------
	void DirectoryClear( const char *pPath, bool bIsXSave )
	{
		char const	*findfn;
		char		szPath[ MAX_PATH ];
		
		findfn = Sys_FindFirstEx( pPath, "DEFAULT_WRITE_PATH", NULL, 0 );
		while ( findfn != NULL )
		{
			if ( !bIsXSave )
			{
				Q_snprintf( szPath, sizeof( szPath ), "%s/%s", saverestore->GetSaveDir(), findfn );
			}
			else
			{
				Q_snprintf( szPath, sizeof( szPath ), "%s:\\%s", GetCurrentMod(), findfn );
			}

			// Delete the temporary save file
			g_pFileSystem->RemoveFile( szPath, "MOD" );

			// Any more save files
			findfn = Sys_FindNext( NULL, 0 );
		}
		Sys_FindClose();
	}

	void AuditFiles( void )
	{
		Msg("Not using save-in-memory path!\n" );
	}

	bool LoadFileFromDisk( const char *pFilename )
	{
		Msg("Not using save-in-memory path!\n" );
		return true;
	}
};

static CSaveRestoreFileSystemPassthrough	s_SaveRestoreFileSystemPassthrough;

ISaveRestoreFileSystem *g_pSaveRestoreFileSystem = &s_SaveRestoreFileSystemPassthrough;

//-----------------------------------------------------------------------------
// Purpose: Called when switching between saving in memory and saving to disk.
//-----------------------------------------------------------------------------
void SaveInMemoryCallback( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	Warning( "save_in_memory is compatible with only the Xbox 360!\n" );
}
