//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================

#include "filesystem_init.h"

#undef PROTECTED_THINGS_ENABLE
#undef PROTECT_FILEIO_FUNCTIONS
#ifndef POSIX
#undef fopen
#endif

#if defined( _WIN32 )
#include "winlite.h"

#include <direct.h>
#include <io.h>
#include <process.h>
#elif defined( POSIX )
#include <unistd.h>
#include <sys/stat.h>
#define _chdir chdir
#define _access access
#endif

#include "tier1/strtools.h"
#include "tier1/utlbuffer.h"
#include "tier1/KeyValues.h"
#include "tier0/icommandline.h"
#include "appframework/IAppSystemGroup.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

constexpr char GAMEINFO_FILENAME[]{"gameinfo.txt"};

static char g_FileSystemError[256];
static bool s_bUseVProjectBinDir = false;
static FSErrorMode_t g_FileSystemErrorMode = FS_ERRORMODE_VCONFIG;

// Call this to use a bin directory relative to VPROJECT
void FileSystem_UseVProjectBinDir( bool bEnable )
{
	s_bUseVProjectBinDir = bEnable;
}

namespace {

// This class lets you modify environment variables, and it restores the original value
// when it goes out of scope.
class CTempEnvVar
{
public:
	CTempEnvVar( const char *pVarName )
	{
		m_bRestoreOriginalValue = true;
		m_pVarName = pVarName;

		const char *pValue = nullptr;

#ifdef _WIN32
		// Use GetEnvironmentVariable instead of getenv because getenv doesn't pick up changes
		// to the process environment after the DLL was loaded.
		char szBuf[ 4096 ];
		if ( GetEnvironmentVariable( m_pVarName, szBuf, sizeof( szBuf ) ) != 0)
		{
			pValue = szBuf;
		}
#else
		// LINUX BUG: see above
		pValue = getenv( pVarName );
#endif 

		if ( pValue )
		{
			m_bExisted = true;
			m_OriginalValue.SetSize( Q_strlen( pValue ) + 1 );
			memcpy( m_OriginalValue.Base(), pValue, m_OriginalValue.Count() );
		}
		else
		{
			m_bExisted = false;
		}
	}

	~CTempEnvVar()
	{
		if ( m_bRestoreOriginalValue )
		{
			// Restore the original value.
			if ( m_bExisted )
			{
				SetValue( "%s", m_OriginalValue.Base() );
			}
			else
			{
				ClearValue();
			}
		}
	}

	void SetRestoreOriginalValue( bool bRestore )
	{
		m_bRestoreOriginalValue = bRestore;
	}

	size_t GetValue [[maybe_unused]] (char *pszBuf, unsigned nBufSize)
	{
		if ( !pszBuf || ( nBufSize <= 0 ) )
			return 0;
	
#ifdef _WIN32
		// Use GetEnvironmentVariable instead of getenv because getenv doesn't pick up changes
		// to the process environment after the DLL was loaded.
		return GetEnvironmentVariable( m_pVarName, pszBuf, nBufSize );
#else
		// LINUX BUG: see above
		const char *pszOut = getenv( m_pVarName );
		if ( !pszOut )
		{
			*pszBuf = '\0';
			return 0;
		}

		Q_strncpy( pszBuf, pszOut, nBufSize );
		return Q_strlen( pszBuf );
#endif
	}

	bool SetValue( const char *pValue, ... )
	{
		char valueString[4096];
		va_list marker;
		va_start( marker, pValue );
		Q_vsnprintf( valueString, sizeof( valueString ), pValue, marker );
		va_end( marker );

#ifdef WIN32
		char str[4096];
		Q_snprintf( str, sizeof( str ), "%s=%s", m_pVarName, valueString );
		bool ok = !_putenv( str );
#else
		bool ok = !setenv( m_pVarName, valueString, 1 );
#endif

		if (!ok)
		{
			Warning( "Unable to set env var %s to %s: %s", m_pVarName, valueString, strerror(errno) );
		}

		return ok;
	}

	bool ClearValue()
	{
#ifdef WIN32
		char str[512];
		Q_snprintf( str, sizeof( str ), "%s=", m_pVarName );
		bool ok = !_putenv( str );
#else
		bool ok = !setenv( m_pVarName, "", 1 );
#endif
		
		if (!ok)
		{
			Warning( "Unable to clear env var %s: %s", m_pVarName, strerror(errno) );
		}

		return ok;
	}

private:
	const char *m_pVarName;
	CUtlVector<char> m_OriginalValue;
	bool m_bRestoreOriginalValue;
	bool m_bExisted;
};


class CSteamEnvVars
{
public:
	CSteamEnvVars() :
		m_SteamAppId( "SteamAppId" ),
		m_SteamUserPassphrase( "SteamUserPassphrase" ),
		m_SteamAppUser( "SteamAppUser" ),
		m_Path( "path" )
	{
	}
	
	void SetRestoreOriginalValue_ALL( bool bRestore )
	{
		m_SteamAppId.SetRestoreOriginalValue( bRestore );
		m_SteamUserPassphrase.SetRestoreOriginalValue( bRestore );
		m_SteamAppUser.SetRestoreOriginalValue( bRestore );
		m_Path.SetRestoreOriginalValue( bRestore );
	}

	CTempEnvVar m_SteamAppId;
	CTempEnvVar m_SteamUserPassphrase;
	CTempEnvVar m_SteamAppUser;
	CTempEnvVar m_Path;
};


// ---------------------------------------------------------------------------------------------------- //
// Helpers.
// ---------------------------------------------------------------------------------------------------- //
template<size_t outSize>
[[nodiscard]] bool Q_getwd( char (&out)[outSize] ) {
#if defined( _WIN32 ) || defined( WIN32 )
	bool ok = !!_getcwd( out, outSize );
	if (ok)
	{
		V_strcat_safe( out, "\\" );
		Q_FixSlashes( out );
	}
	return ok;
#else
	bool ok = !!getcwd( out, outSize );
	if (ok)
	{
		strcat( out, "/" );
		Q_FixSlashes( out );
	}
	return ok;
#endif
}

}  // namespace

// ---------------------------------------------------------------------------------------------------- //
// Module interface.
// ---------------------------------------------------------------------------------------------------- //

CFSSearchPathsInit::CFSSearchPathsInit()
{
	m_pDirectoryName = nullptr;
	m_pLanguage = nullptr;
	m_pFileSystem = nullptr;
	m_ModPath[0] = 0;
	m_bMountHDContent = m_bLowViolence = false;
}


CFSSteamSetupInfo::CFSSteamSetupInfo()
{
	m_pDirectoryName = nullptr;
	m_bOnlyUseDirectoryName = false;
	m_bSetSteamDLLPath = false;
	m_bSteam = false;
	m_bToolsMode = true;
	m_bNoGameInfo = false;
	m_GameInfoPath[0] = '\0';
}


CFSLoadModuleInfo::CFSLoadModuleInfo()
{
	m_pFileSystemDLLName = nullptr;
	m_ConnectFactory = nullptr;
	m_pFileSystem = nullptr;
	m_pModule = nullptr;
}


CFSMountContentInfo::CFSMountContentInfo()
{
	m_bToolsMode = true;
	m_pDirectoryName = nullptr;
	m_pFileSystem = nullptr;
}


const char *FileSystem_GetLastErrorString()
{
	return g_FileSystemError;
}


KeyValues* ReadKeyValuesFile( const char *pFilename )
{
	// Read in the gameinfo.txt file and null-terminate it.
	FILE *fp = fopen( pFilename, "rb" );
	if ( !fp )
		return nullptr;
	
	fseek( fp, 0, SEEK_END );
	CUtlVector<char> buf;
	buf.SetSize( ftell( fp ) + 1 );
	fseek( fp, 0, SEEK_SET );
	fread( buf.Base(), 1, buf.Count()-1, fp );
	fclose( fp );
	buf[buf.Count()-1] = 0;

	KeyValues *kv = new KeyValues( "" );
	if ( !kv->LoadFromBuffer( pFilename, buf.Base() ) )
	{
		kv->deleteThis();
		return nullptr;
	}
	
	return kv;
}

static bool Sys_GetExecutableName( char *out, unsigned len )
{
#if defined( _WIN32 )
    if ( !::GetModuleFileName( ( HINSTANCE )GetModuleHandle( nullptr ), out, len ) )
    {
		return false;
    }
#else
	if ( CommandLine()->GetParm(0) )
	{
		Q_MakeAbsolutePath( out, len, CommandLine()->GetParm(0) );
	}
	else
	{
		return false;
	}
#endif

	return true;
}

bool FileSystem_GetExecutableDir( char *exedir, unsigned exeDirLen )
{
	exedir[0] = '\0';

	if ( s_bUseVProjectBinDir )
	{
		const char *pProject = GetVProjectCmdLineValue();
		if ( !pProject )
		{
			// Check their registry.
			pProject = getenv( GAMEDIR_TOKEN );
		}
		if ( pProject )
		{
			Q_snprintf( exedir, exeDirLen, "%s%c..%cbin", pProject, CORRECT_PATH_SEPARATOR, CORRECT_PATH_SEPARATOR );
			return true;
		}
		return false;
	}

	if ( !Sys_GetExecutableName( exedir, exeDirLen ) )
		return false;
	Q_StripFilename( exedir );

	Q_FixSlashes( exedir );

	// Return the bin directory as the executable dir if it's not in there
	// because that's really where we're running from...
	char ext[MAX_PATH];
	Q_StrRight( exedir, 4, ext, sizeof( ext ) );
	if ( ext[0] != CORRECT_PATH_SEPARATOR ||
#ifdef PLATFORM_64BITS
		Q_stricmp( ext+1, "x64" ) != 0
#else
		Q_stricmp( ext+1, "bin" ) != 0
#endif
		)
	{
		Q_strncat( exedir, CORRECT_PATH_SEPARATOR_S, exeDirLen, COPY_ALL_CHARACTERS );
		Q_strncat( exedir, "bin", exeDirLen, COPY_ALL_CHARACTERS );

#ifdef PLATFORM_64BITS
		Q_strncat( exedir, CORRECT_PATH_SEPARATOR_S "x64", exeDirLen, COPY_ALL_CHARACTERS );
#endif

		Q_FixSlashes( exedir );
	}
	
	return true;
}

template<size_t max_size>
static bool FileSystem_GetBaseDir( char (&baseDir)[max_size] )
{
	if ( FileSystem_GetExecutableDir( baseDir ) )
	{
#ifdef PLATFORM_64BITS
		// dimhteopus: Need to strip x64, too.
		Q_StripFilename( baseDir );
#endif
		Q_StripFilename( baseDir );
		return true;
	}
	
	return false;
}

static bool LaunchVConfig()
{
	char vconfigExe[MAX_PATH];
	FileSystem_GetExecutableDir( vconfigExe );
	Q_AppendSlash( vconfigExe, sizeof( vconfigExe ) );
	V_strcat_safe( vconfigExe, "vconfig.exe" );

	const char *argv[] =
	{
		vconfigExe,
		"-allowdebug",
		nullptr
	};

	return _spawnv( _P_NOWAIT, vconfigExe, argv ) != -1;
}

const char* GetVProjectCmdLineValue()
{
	return CommandLine()->ParmValue( "-vproject", CommandLine()->ParmValue( "-game" ) );
}

FSReturnCode_t SetupFileSystemError( bool bRunVConfig, FSReturnCode_t retVal, const char *pMsg, ... )
{
	va_list marker;
	va_start( marker, pMsg );
	Q_vsnprintf( g_FileSystemError, sizeof( g_FileSystemError ), pMsg, marker );
	va_end( marker );

	Warning( "%s\n", g_FileSystemError );

	// Run vconfig?
	// Don't do it if they specifically asked for it not to, or if they manually specified a vconfig with -game or -vproject.
	if ( bRunVConfig && g_FileSystemErrorMode == FS_ERRORMODE_VCONFIG && !CommandLine()->FindParm( CMDLINEOPTION_NOVCONFIG ) && !GetVProjectCmdLineValue() )
	{
		if ( !LaunchVConfig() )
		{
			Warning( "Unable to launch VConfig.\n" );
			return FS_UNABLE_TO_INIT;
		}
	}

	if ( g_FileSystemErrorMode == FS_ERRORMODE_AUTO || g_FileSystemErrorMode == FS_ERRORMODE_VCONFIG )
	{
		Error( "%s\n", g_FileSystemError );
	}
	
	return retVal;
}

FSReturnCode_t LoadGameInfoFile( 
	const char *pDirectoryName, 
	KeyValues *&pMainFile, 
	KeyValues *&pFileSystemInfo, 
	KeyValues *&pSearchPaths )
{
	// If GameInfo.txt exists under pBaseDir, then this is their game directory.
	// All the filesystem mappings will be in this file.
	char gameinfoFilename[MAX_PATH];
	V_strcpy_safe( gameinfoFilename, pDirectoryName );
	Q_AppendSlash( gameinfoFilename, sizeof( gameinfoFilename ) );
	V_strcat_safe( gameinfoFilename, GAMEINFO_FILENAME );
	Q_FixSlashes( gameinfoFilename );
	pMainFile = ReadKeyValuesFile( gameinfoFilename );
	if ( !pMainFile )
	{
		return SetupFileSystemError( true, FS_MISSING_GAMEINFO_FILE, "%s is missing.", gameinfoFilename );
	}

	pFileSystemInfo = pMainFile->FindKey( "FileSystem" );
	if ( !pFileSystemInfo )
	{
		pMainFile->deleteThis();
		return SetupFileSystemError( true, FS_INVALID_GAMEINFO_FILE, "%s is not a valid format.", gameinfoFilename );
	}

	// Now read in all the search paths.
	pSearchPaths = pFileSystemInfo->FindKey( "SearchPaths" );
	if ( !pSearchPaths )
	{
		pMainFile->deleteThis();
		return SetupFileSystemError( true, FS_INVALID_GAMEINFO_FILE, "%s is not a valid format.", gameinfoFilename );
	}
	return FS_OK;
}


static void FileSystem_AddLoadedSearchPath( 
	CFSSearchPathsInit &initInfo, 
	const char *pPathID, 
	const char *fullLocationPath, 
	bool bLowViolence )
{

	// Check for mounting LV game content in LV builds only
	if ( V_stricmp( pPathID, "game_lv" ) == 0 )
	{

		// Not in LV build, don't mount
		if ( !bLowViolence )
			return;

		// Mount, as a game path
		pPathID = "game";
	}

	// Check for mounting HD game content if enabled
	if ( V_stricmp( pPathID, "game_hd" ) == 0 )
	{

		// Not in LV build, don't mount
		if ( !initInfo.m_bMountHDContent )
			return;

		// Mount, as a game path
		pPathID = "game";
	}


	// Special processing for ordinary game folders
	if ( V_stristr( fullLocationPath, ".vpk" ) == nullptr && Q_stricmp( pPathID, "game" ) == 0 )
	{
		if ( CommandLine()->FindParm( "-tempcontent" ) != 0 )
		{
			char szPath[MAX_PATH];
			Q_snprintf( szPath, sizeof(szPath), "%s_tempcontent", fullLocationPath );
			initInfo.m_pFileSystem->AddSearchPath( szPath, pPathID, PATH_ADD_TO_TAIL );
		}
	}

	
	if ( initInfo.m_pLanguage &&
	     Q_stricmp( initInfo.m_pLanguage, "english" ) &&
	     V_strstr( fullLocationPath, "_english" ) != nullptr )
	{
		char szPath[MAX_PATH];
		char szLangString[MAX_PATH];
		
		// Need to add a language version of this path first

		Q_snprintf( szLangString, sizeof(szLangString), "_%s", initInfo.m_pLanguage);
		V_StrSubst( fullLocationPath, "_english", szLangString, szPath, sizeof( szPath ), true );
		initInfo.m_pFileSystem->AddSearchPath( szPath, pPathID, PATH_ADD_TO_TAIL );
	}

	initInfo.m_pFileSystem->AddSearchPath( fullLocationPath, pPathID, PATH_ADD_TO_TAIL );
}

static int SortStricmp( char * const * sz1, char * const * sz2 )
{
	return V_stricmp( *sz1, *sz2 );
}

FSReturnCode_t FileSystem_LoadSearchPaths( CFSSearchPathsInit &initInfo )
{
	if ( !initInfo.m_pFileSystem || !initInfo.m_pDirectoryName )
		return SetupFileSystemError( false, FS_INVALID_PARAMETERS, "FileSystem_LoadSearchPaths: Invalid parameters specified." );

	KeyValues *pMainFile, *pFileSystemInfo, *pSearchPaths;
	FSReturnCode_t retVal = LoadGameInfoFile( initInfo.m_pDirectoryName, pMainFile, pFileSystemInfo, pSearchPaths );
	if ( retVal != FS_OK )
		return retVal;
	
	// All paths except those marked with |gameinfo_path| are relative to the base dir.
	char baseDir[MAX_PATH];
	if ( !FileSystem_GetBaseDir( baseDir ) )
		return SetupFileSystemError( false, FS_INVALID_PARAMETERS, "FileSystem_GetBaseDir failed." );

	// The MOD directory is always the one that contains gameinfo.txt
	V_strcpy_safe( initInfo.m_ModPath, initInfo.m_pDirectoryName );

	constexpr char GAMEINFOPATH_TOKEN[]{"|gameinfo_path|"};
	constexpr char BASESOURCEPATHS_TOKEN[]{"|all_source_engine_paths|"};

	const char *pszExtraSearchPath = CommandLine()->ParmValue( "-insert_search_path" );
	if ( pszExtraSearchPath )
	{
		CUtlStringList vecPaths;
		V_SplitString( pszExtraSearchPath, ",", vecPaths );
		FOR_EACH_VEC( vecPaths, idxExtraPath )
		{
			char szAbsSearchPath[MAX_PATH];
			Q_StripPrecedingAndTrailingWhitespace( vecPaths[ idxExtraPath ] );
			V_MakeAbsolutePath( szAbsSearchPath, sizeof( szAbsSearchPath ), vecPaths[ idxExtraPath ], baseDir );
			V_FixSlashes( szAbsSearchPath );
			if ( !V_RemoveDotSlashes( szAbsSearchPath ) )
				Error( "Bad -insert_search_path - Can't resolve pathname for '%s'", szAbsSearchPath );
			V_StripTrailingSlash( szAbsSearchPath );
			FileSystem_AddLoadedSearchPath( initInfo, "GAME", szAbsSearchPath, false );
			FileSystem_AddLoadedSearchPath( initInfo, "MOD", szAbsSearchPath, false );
		}
	}

	bool bLowViolence = initInfo.m_bLowViolence;
	bool bFirstGamePath = true;
	for ( KeyValues *pCur=pSearchPaths->GetFirstValue(); pCur; pCur=pCur->GetNextValue() )
	{
		const char *pLocation = pCur->GetString();
		const char *pszBaseDir = baseDir;

		if ( Q_stristr( pLocation, GAMEINFOPATH_TOKEN ) == pLocation )
		{
			pLocation += std::size( GAMEINFOPATH_TOKEN ) - 1;
			pszBaseDir = initInfo.m_pDirectoryName;
		}
		else if ( Q_stristr( pLocation, BASESOURCEPATHS_TOKEN ) == pLocation )
		{
			// This is a special identifier that tells it to add the specified path for all source engine versions equal to or prior to this version.
			// So in Orange Box, if they specified:
			//		|all_source_engine_paths|hl2
			// it would add the ep2\hl2 folder and the base (ep1-era) hl2 folder.
			//
			// We need a special identifier in the gameinfo.txt here because the base hl2 folder exists in different places.
			// In the case of a game or a Steam-launched dedicated server, all the necessary prior engine content is mapped in with the Steam depots,
			// so we can just use the path as-is.
			pLocation += std::size( BASESOURCEPATHS_TOKEN ) - 1;
		}

		CUtlStringList vecFullLocationPaths;
		char szAbsSearchPath[MAX_PATH];
		V_MakeAbsolutePath( szAbsSearchPath, sizeof( szAbsSearchPath ), pLocation, pszBaseDir );

		// Now resolve any ./'s.
		V_FixSlashes( szAbsSearchPath );
		if ( !V_RemoveDotSlashes( szAbsSearchPath ) )
			Error( "FileSystem_AddLoadedSearchPath - Can't resolve pathname for '%s'", szAbsSearchPath );
		V_StripTrailingSlash( szAbsSearchPath );

		// Don't bother doing any wildcard expansion unless it has wildcards.  This avoids the weird
		// thing with xxx_dir.vpk files being referred to simply as xxx.vpk.
		if ( V_stristr( pLocation, "?") == nullptr && V_stristr( pLocation, "*") == nullptr )
		{
			vecFullLocationPaths.CopyAndAddToTail( szAbsSearchPath );
		}
		else
		{
			FileFindHandle_t findHandle = 0;
			const char *pszFoundShortName = initInfo.m_pFileSystem->FindFirst( szAbsSearchPath, &findHandle );
			if ( pszFoundShortName )
			{
				do 
				{

					// We only know how to mount VPK's and directories
					if ( pszFoundShortName[0] != '.' && ( initInfo.m_pFileSystem->FindIsDirectory( findHandle ) || V_stristr( pszFoundShortName, ".vpk" ) ) )
					{
						char szAbsName[MAX_PATH];
						V_ExtractFilePath( szAbsSearchPath, szAbsName, sizeof( szAbsName ) );
						V_AppendSlash( szAbsName, sizeof(szAbsName) );
						V_strcat_safe( szAbsName, pszFoundShortName );

						vecFullLocationPaths.CopyAndAddToTail( szAbsName );

						// Check for a common mistake
						if (
							!V_stricmp( pszFoundShortName, "materials" )
							|| !V_stricmp( pszFoundShortName, "maps" )
							|| !V_stricmp( pszFoundShortName, "resource" )
							|| !V_stricmp( pszFoundShortName, "scripts" )
							|| !V_stricmp( pszFoundShortName, "sound" )
							|| !V_stricmp( pszFoundShortName, "models" ) )
						{

							char szReadme[MAX_PATH];
							V_ExtractFilePath( szAbsSearchPath, szReadme, sizeof( szReadme ) );
							V_AppendSlash( szReadme, sizeof(szReadme) );
							V_strcat_safe( szReadme, "readme.txt" );

							Error(
								"Tried to add %s as a search path.\n"
								"\nThis is probably not what you intended.\n"
								"\nCheck %s for more info\n",
								szAbsName, szReadme );
						}

					}
					pszFoundShortName = initInfo.m_pFileSystem->FindNext( findHandle );
				} while ( pszFoundShortName );
				initInfo.m_pFileSystem->FindClose( findHandle );
			}

			// Sort alphabetically.  Also note that this will put
			// all the xxx_000.vpk packs just before the corresponding
			// xxx_dir.vpk
			vecFullLocationPaths.Sort( SortStricmp );

			// Now for any _dir.vpk files, remove the _nnn.vpk ones.
			intp idx = vecFullLocationPaths.Count()-1;
			while ( idx > 0 )
			{
				char szTemp[ MAX_PATH ];
				V_strcpy_safe( szTemp, vecFullLocationPaths[ idx ] );
				--idx;

				char *szDirVpk = V_stristr( szTemp, "_dir.vpk" );
				if ( szDirVpk != nullptr )
				{
					*szDirVpk = '\0';
					while ( idx >= 0 )
					{
						char *pszPath = vecFullLocationPaths[ idx ];
						if ( V_stristr( pszPath, szTemp ) != pszPath )
							break;
						delete pszPath;
						vecFullLocationPaths.Remove( idx );
						--idx;
					}
				}
			}
		}

		// Parse Path ID list
		CUtlStringList vecPathIDs;
		V_SplitString( pCur->GetName(), "+", vecPathIDs );
		FOR_EACH_VEC( vecPathIDs, idxPathID )
		{
			Q_StripPrecedingAndTrailingWhitespace( vecPathIDs[ idxPathID ] );
		}

		// Mount them.
		FOR_EACH_VEC( vecFullLocationPaths, idxLocation )
		{
			FOR_EACH_VEC( vecPathIDs, idxPathID )
			{
				const char *pPathID = vecPathIDs[ idxPathID ], *pFullPath = vecFullLocationPaths[ idxLocation ];

				FileSystem_AddLoadedSearchPath( initInfo, pPathID, pFullPath, bLowViolence );
				
				if ( Q_stricmp("game", pPathID) == 0 )
				{
					const char *ext = Q_GetFileExtension( pFullPath );
					// dimhotepus: Do not add stuff like .vpk/bin as nonsense.
					if ( !ext || Q_strcmp( ext, "vpk" ))
					{
						char szGameBinPath[MAX_PATH];
#ifdef PLATFORM_64BITS
						Q_snprintf( szGameBinPath, sizeof(szGameBinPath), "%s" CORRECT_PATH_SEPARATOR_S "bin" CORRECT_PATH_SEPARATOR_S "x64", pFullPath );
#else
						Q_snprintf( szGameBinPath, sizeof(szGameBinPath), "%s" CORRECT_PATH_SEPARATOR_S "bin", pFullPath );
#endif

						// 1. For each "Game" search path, it adds a "GameBin" path, in <dir>\bin[\x64]
						FileSystem_AddLoadedSearchPath( initInfo, "GAMEBIN", szGameBinPath, bLowViolence );
					}
					
					// 2. For each "Game" search path, it adds another "Game" path in front of it with _<langage> at the end.
					//    For example: c:\hl2\cstrike on a french machine would get a c:\hl2\cstrike_french path added to it.

					// 3. For the first "Game" search path, it adds a search path called "MOD".
					// dimhotepus: Not only for first, as ex. episode 1 depends on skill.cfg from hl2, so add hl2 as MOD too, but with lower priority.
					FileSystem_AddLoadedSearchPath( initInfo, "MOD", pFullPath, bLowViolence );

					if ( bFirstGamePath )
					{
						// 4. For the first "Game" search path, it adds a search path called "DEFAULT_WRITE_PATH".
						FileSystem_AddLoadedSearchPath( initInfo, "DEFAULT_WRITE_PATH", pFullPath, bLowViolence );

						bFirstGamePath = false;
					}
				}
			}
		}
	}

	pMainFile->deleteThis();

	// Also, mark specific path IDs as "by request only". That way, we won't waste time searching in them
	// when people forget to specify a search path.
	initInfo.m_pFileSystem->MarkPathIDByRequestOnly( "executable_path", true );
	initInfo.m_pFileSystem->MarkPathIDByRequestOnly( "gamebin", true );
	initInfo.m_pFileSystem->MarkPathIDByRequestOnly( "download", true );
	initInfo.m_pFileSystem->MarkPathIDByRequestOnly( "mod", true );
	initInfo.m_pFileSystem->MarkPathIDByRequestOnly( "game_write", true );
	initInfo.m_pFileSystem->MarkPathIDByRequestOnly( "mod_write", true );

#ifdef _DEBUG	
	// initInfo.m_pFileSystem->PrintSearchPaths();
#endif

	return FS_OK;
}

static bool DoesFileExistIn( const char *pDirectoryName, const char *pFilename )
{
	char filename[MAX_PATH];

	V_strcpy_safe( filename, pDirectoryName );
	Q_AppendSlash( filename, sizeof( filename ) );
	V_strcat_safe( filename, pFilename );
	Q_FixSlashes( filename );
	bool bExist = ( _access( filename, 0 ) == 0 );

	return bExist;
}

namespace
{
	SuggestGameInfoDirFn_t & GetSuggestGameInfoDirFn( void )
	{
		static SuggestGameInfoDirFn_t s_pfnSuggestGameInfoDir = nullptr;
		return s_pfnSuggestGameInfoDir;
	}
}; // `anonymous` namespace

SuggestGameInfoDirFn_t SetSuggestGameInfoDirFn( SuggestGameInfoDirFn_t pfnNewFn )
{
	SuggestGameInfoDirFn_t &rfn = GetSuggestGameInfoDirFn();
	SuggestGameInfoDirFn_t pfnOldFn = rfn;
	rfn = pfnNewFn;
	return pfnOldFn;
}

template<int outDirLen>
static FSReturnCode_t TryLocateGameInfoFile(char (&pOutDir)[outDirLen],
                                            bool bBubbleDir){
    // Retain a copy of suggested path for further attempts
    char spchCopyNameBuffer[outDirLen] = {};
	V_strcpy_safe( spchCopyNameBuffer, pOutDir );

	// Make appropriate slashes ('/' - Linux style)
	for ( char *pchFix = spchCopyNameBuffer,
		*pchEnd = pchFix + outDirLen;
		pchFix < pchEnd; ++ pchFix )
	{
		if ( '\\' == *pchFix )
		{
			*pchFix = '/';
		}
	}

	// Have a look in supplied path
	do
	{
		if ( DoesFileExistIn( pOutDir, GAMEINFO_FILENAME ) )
		{
			return FS_OK;
		}
	} 
	while ( bBubbleDir && Q_StripLastDir( pOutDir, outDirLen ) );

	// Make an attempt to resolve from "content -> game" directory
	V_strcpy_safe( pOutDir, spchCopyNameBuffer );
	if ( char *pchContentFix = Q_stristr( pOutDir, "/content/" ) )
	{
		sprintf( pchContentFix, "/game/" );
		memmove( pchContentFix + 6, pchContentFix + 9, pOutDir + outDirLen - (pchContentFix + 9) );

		// Try in the mapped "game" directory
		do
		{
			if ( DoesFileExistIn( pOutDir, GAMEINFO_FILENAME ) )
			{
				return FS_OK;
			}
		} 
		while ( bBubbleDir && Q_StripLastDir( pOutDir, outDirLen ) );
	}

	// Could not find it here
	return FS_MISSING_GAMEINFO_FILE;
}

template<int outDirLen>
FSReturnCode_t LocateGameInfoFile( const CFSSteamSetupInfo &fsInfo, char (&pOutDir)[outDirLen] )
{
	// Engine and Hammer don't want to search around for it.
	if ( fsInfo.m_bOnlyUseDirectoryName )
	{
		if ( !fsInfo.m_pDirectoryName )
			return SetupFileSystemError( false, FS_MISSING_GAMEINFO_FILE, "bOnlyUseDirectoryName=1 and pDirectoryName=nullptr." );

		bool bExists = DoesFileExistIn( fsInfo.m_pDirectoryName, GAMEINFO_FILENAME );
		if ( !bExists )
		{
			return SetupFileSystemError( true, FS_MISSING_GAMEINFO_FILE, "Setup file '%s' doesn't exist in subdirectory '%s'.\nCheck your -game parameter or VCONFIG setting.", GAMEINFO_FILENAME, fsInfo.m_pDirectoryName );
		}

		V_strcpy_safe( pOutDir, fsInfo.m_pDirectoryName );
		return FS_OK;
	}

	// First, check for overrides on the command line or environment variables.
	const char *pProject = GetVProjectCmdLineValue();

	if ( pProject )
	{
		if ( DoesFileExistIn( pProject, GAMEINFO_FILENAME ) )
		{
			Q_MakeAbsolutePath( pOutDir, outDirLen, pProject );
			return FS_OK;
		}
		
		if ( fsInfo.m_bNoGameInfo )
		{
			// fsInfo.m_bNoGameInfo is set by the Steam dedicated server, before it knows which mod to use.
			// Steam dedicated server doesn't need a gameinfo.txt, because we'll ask which mod to use, even if
			// -game is supplied on the command line.
			V_strcpy_safe( pOutDir, "" );
			return FS_OK;
		}
		else
		{
			// They either specified vproject on the command line or it's in their registry. Either way,
			// we don't want to continue if they've specified it but it's not valid.
			goto ShowError;
		}
	}

	if ( fsInfo.m_bNoGameInfo )
	{
		V_strcpy_safe( pOutDir, "" );
		return FS_OK;
	}

	// Ask the application if it can provide us with a game info directory
	{
		bool bBubbleDir = true;
		SuggestGameInfoDirFn_t pfnSuggestGameInfoDirFn = GetSuggestGameInfoDirFn();
		if ( pfnSuggestGameInfoDirFn &&
			( * pfnSuggestGameInfoDirFn )( &fsInfo, pOutDir, outDirLen, &bBubbleDir ) &&
			FS_OK == TryLocateGameInfoFile( pOutDir, bBubbleDir ) )
			return FS_OK;
	}

	// Try to use the environment variable / registry
	if ( ( pProject = getenv( GAMEDIR_TOKEN ) ) != nullptr )
	{
		Q_MakeAbsolutePath( pOutDir, outDirLen, pProject );

		if ( FS_OK == TryLocateGameInfoFile( pOutDir, false ) )
			return FS_OK;
	}

	if ( IsPC() )
	{
		Warning( "Warning: falling back to auto detection of vproject directory.\n" );
		
		// Now look for it in the directory they passed in.
		if ( fsInfo.m_pDirectoryName )
			Q_MakeAbsolutePath( pOutDir, outDirLen, fsInfo.m_pDirectoryName );
		else
			Q_MakeAbsolutePath( pOutDir, outDirLen, "." );

		if ( FS_OK == TryLocateGameInfoFile( pOutDir, true ) )
			return FS_OK;

		// Use the CWD
		if ( !Q_getwd( pOutDir ) )
			return SetupFileSystemError( false, FS_UNABLE_TO_INIT, 
				"Unable to get current directory." );

		if ( FS_OK == TryLocateGameInfoFile( pOutDir, true ) )
			return FS_OK;
	}

ShowError:
	return SetupFileSystemError( true, FS_MISSING_GAMEINFO_FILE, 
		"Unable to find %s. Solutions:\n\n"
		"1. Read http://www.valve-erc.com/srcsdk/faq.html#NoGameDir\n"
		"2. Run vconfig to specify which game you're working on.\n"
		"3. Add -game <path> on the command line where <path> is the directory that %s is in.\n",
		GAMEINFO_FILENAME, GAMEINFO_FILENAME );
}

bool DoesPathExistAlready( const char *pPathEnvVar, const char *pTestPath )
{
	// Fix the slashes in the input arguments.
	char correctedPathEnvVar[8192], correctedTestPath[MAX_PATH];
	V_strcpy_safe( correctedPathEnvVar, pPathEnvVar );
	Q_FixSlashes( correctedPathEnvVar );
	pPathEnvVar = correctedPathEnvVar;

	V_strcpy_safe( correctedTestPath, pTestPath );
	const size_t correctTestPathLen = strlen( correctedTestPath );
	Q_FixSlashes( correctedTestPath );
	if ( correctTestPathLen && PATHSEPARATOR( correctedTestPath[ correctTestPathLen - 1 ] ) )
		correctedTestPath[ correctTestPathLen - 1 ] = 0;

	pTestPath = correctedTestPath;

	size_t nTestPathLen = strlen( pTestPath );
	const char *pCurPos = pPathEnvVar;
	while ( 1 )
	{
		const char *pTestPos = Q_stristr( pCurPos, pTestPath );
		if ( !pTestPos )
			return false;

		// Ok, we found pTestPath in the path, but it's only valid if it's followed by an optional slash and a semicolon.
		pTestPos += nTestPathLen;
		if ( pTestPos[0] == 0 || pTestPos[0] == ';' || (PATHSEPARATOR( pTestPos[0] ) && pTestPos[1] == ';') )
			return true;
	
		// Advance our marker..
		pCurPos = pTestPos;
	}
}

FSReturnCode_t FileSystem_SetBasePaths( IFileSystem *pFileSystem )
{
	pFileSystem->RemoveSearchPaths( "EXECUTABLE_PATH" );

	char executablePath[MAX_PATH];
	if ( !FileSystem_GetExecutableDir( executablePath )	)
		return SetupFileSystemError( false, FS_INVALID_PARAMETERS, "FileSystem_GetExecutableDir failed." );

	pFileSystem->AddSearchPath( executablePath, "EXECUTABLE_PATH" );

	if ( !FileSystem_GetBaseDir( executablePath ) )
		return SetupFileSystemError( false, FS_INVALID_PARAMETERS, "FileSystem_GetBaseDir failed." );

	pFileSystem->AddSearchPath( executablePath, "BASE_PATH" );

	return FS_OK;
}

//-----------------------------------------------------------------------------
// Returns the name of the file system DLL to use
//-----------------------------------------------------------------------------
FSReturnCode_t FileSystem_GetFileSystemDLLName( char *pFileSystemDLL, size_t nMaxLen, bool &bSteam )
{
	bSteam = false;

	// Inside of here, we don't have a filesystem yet, so we have to assume that the filesystem_stdio or filesystem_steam
	// is in this same directory with us.
	char executablePath[MAX_PATH];
	if ( !FileSystem_GetExecutableDir( executablePath )	)
		return SetupFileSystemError( false, FS_INVALID_PARAMETERS, "FileSystem_GetExecutableDir failed." );

	// Use filesystem_steam if it exists?
	struct stat statBuf;

	// Assume we'll use local files
	Q_snprintf( pFileSystemDLL, nMaxLen, "%s%cfilesystem_stdio" DLL_EXT_STRING, executablePath, CORRECT_PATH_SEPARATOR );
	if ( stat( pFileSystemDLL, &statBuf ) == 0 )
	{
		bSteam = false;
		return FS_OK;
	}

	Q_snprintf( pFileSystemDLL, nMaxLen, "%s%cfilesystem_steam" DLL_EXT_STRING, executablePath, CORRECT_PATH_SEPARATOR );
	if ( stat( pFileSystemDLL, &statBuf ) == 0 )
	{
		bSteam = true;
		return FS_OK;
	}

	// dimhotepus: Report missed filesystem DLL.
	return SetupFileSystemError( false, FS_UNABLE_TO_INIT, "Please ensure game installed correctly.\n\n"
		"Unable to load filesystem *" DLL_EXT_STRING ": both stdio / steam are absent." );
}


//-----------------------------------------------------------------------------
// Sets up the steam environment + gets back the gameinfo.txt path
//-----------------------------------------------------------------------------
FSReturnCode_t FileSystem_SetupSteamEnvironment( CFSSteamSetupInfo &fsInfo )
{
	// First, locate the directory with gameinfo.txt.
	const FSReturnCode_t ret = LocateGameInfoFile( fsInfo, fsInfo.m_GameInfoPath );
	if ( ret != FS_OK )
		return ret;

	// This is so that processes spawned by this application will have the same VPROJECT
#ifdef WIN32
	char pEnvBuf[MAX_PATH+32];
	Q_snprintf( pEnvBuf, sizeof(pEnvBuf), "%s=%s", GAMEDIR_TOKEN, fsInfo.m_GameInfoPath );
	return !_putenv( pEnvBuf )
		? FS_OK
		: SetupFileSystemError( false, FS_UNABLE_TO_INIT, "Unable to set env variable %s: %s.", pEnvBuf, strerror(errno) );
#else
	return !setenv( GAMEDIR_TOKEN, fsInfo.m_GameInfoPath, 1 )
		? FS_OK
		: SetupFileSystemError( false, FS_UNABLE_TO_INIT, "Unable to set env variable %s: %s.", pEnvBuf, strerror(errno) );
#endif
}


//-----------------------------------------------------------------------------
// Loads the file system module
//-----------------------------------------------------------------------------
FSReturnCode_t FileSystem_LoadFileSystemModule( CFSLoadModuleInfo &fsInfo )
{
	// First, locate the directory with gameinfo.txt.
	FSReturnCode_t ret = FileSystem_SetupSteamEnvironment( fsInfo );
	if ( ret != FS_OK )
		return ret;

	// Now that the environment is setup, load the filesystem module.
	if ( !Sys_LoadInterface(
		fsInfo.m_pFileSystemDLLName,
		FILESYSTEM_INTERFACE_VERSION,
		&fsInfo.m_pModule,
		(void**)&fsInfo.m_pFileSystem ) )
	{
		return SetupFileSystemError( false, FS_UNABLE_TO_INIT, "Can't load %s.", fsInfo.m_pFileSystemDLLName );
	}

	if ( !fsInfo.m_pFileSystem->Connect( fsInfo.m_ConnectFactory ) )
		return SetupFileSystemError( false, FS_UNABLE_TO_INIT, "%s IFileSystem::Connect failed.", fsInfo.m_pFileSystemDLLName );

	if ( fsInfo.m_pFileSystem->Init() != INIT_OK )
		return SetupFileSystemError( false, FS_UNABLE_TO_INIT, "%s IFileSystem::Init failed.", fsInfo.m_pFileSystemDLLName );

	return FS_OK;
}


//-----------------------------------------------------------------------------
// Mounds a particular steam cache
//-----------------------------------------------------------------------------
FSReturnCode_t FileSystem_MountContent( CFSMountContentInfo &mountContentInfo )
{
	// This part is Steam-only.
	if ( mountContentInfo.m_pFileSystem->IsSteam() )
	{
		return SetupFileSystemError( false, FS_INVALID_PARAMETERS, "Should not be using filesystem_steam anymore!" );
	}

	return FileSystem_SetBasePaths( mountContentInfo.m_pFileSystem );
}

void FileSystem_SetErrorMode( FSErrorMode_t errorMode )
{
	g_FileSystemErrorMode = errorMode;
}

void FileSystem_ClearSteamEnvVars()
{
	CSteamEnvVars envVars;

	// Change the values and don't restore the originals.
	envVars.m_SteamAppId.SetValue( "" );
	envVars.m_SteamUserPassphrase.SetValue( "" );
	envVars.m_SteamAppUser.SetValue( "" );
	
	envVars.SetRestoreOriginalValue_ALL( false );
}

//-----------------------------------------------------------------------------
// Adds the platform folder to the search path.
//-----------------------------------------------------------------------------
void FileSystem_AddSearchPath_Platform( IFileSystem *pFileSystem, const char *szGameInfoPath )
{
	char platform[MAX_PATH];
	V_strcpy_safe( platform, szGameInfoPath );
	Q_StripTrailingSlash( platform );
	V_strcat_safe( platform, "/../platform", MAX_PATH );

	pFileSystem->AddSearchPath( platform, "PLATFORM" );
}
