//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "filesystem_tools.h"

#if defined( _WIN32 ) && !defined( _X360 )
#include <direct.h>
#include <io.h> // _chmod
#elif _LINUX
#include <unistd.h>
#endif

#include <sys/stat.h>

#include "tier1/KeyValues.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "tier2/tier2.h"

#ifdef MPI
	#include "vmpi.h"
	#include "vmpi_tools_shared.h"
	#include "vmpi_filesystem.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


// ---------------------------------------------------------------------------------------------------- //
// Module interface.
// ---------------------------------------------------------------------------------------------------- //

IBaseFileSystem *g_pFileSystem = nullptr;

// These are only used for tools that need the search paths that the engine's file system provides.
CSysModule		*g_pFullFileSystemModule = nullptr;

// ---------------------------------------------------------------------------
//
// These are the base paths that everything will be referenced relative to (textures especially)
// All of these directories include the trailing slash
//
// ---------------------------------------------------------------------------

// This is the the path of the initial source file (relative to the cwd)
char		qdir[1024];

// This is the base engine + mod-specific game dir (e.g. "c:\tf2\mytfmod\")
char		gamedir[1024];	

void FileSystem_SetupStandardDirectories( const char *pFilename, const char *pGameInfoPath )
{
	// Set qdir.
	if ( !pFilename )
	{
		pFilename = ".";
	}

	V_MakeAbsolutePath( qdir, pFilename, nullptr );
	V_StripFilename( qdir );
	V_strlower( qdir );
	if ( qdir[0] != 0 )
	{
		V_AppendSlash( qdir );
	}

	// Set gamedir.
	V_MakeAbsolutePath( gamedir, pGameInfoPath );
	V_AppendSlash( gamedir );
}


bool FileSystem_Init_Normal( const char *pFilename, FSInitType_t initType, bool bOnlyUseDirectoryName )
{
	if ( initType == FS_INIT_FULL )
	{
		// First, get the name of the module
		char fileSystemDLLName[MAX_PATH];
		bool bSteam;
		if ( FileSystem_GetFileSystemDLLName( fileSystemDLLName, bSteam ) != FS_OK )
			return false;

		// Next, load the module, call Connect/Init.
		CFSLoadModuleInfo loadModuleInfo;
		loadModuleInfo.m_pFileSystemDLLName = fileSystemDLLName;
		loadModuleInfo.m_pDirectoryName = pFilename;
		loadModuleInfo.m_bOnlyUseDirectoryName = bOnlyUseDirectoryName;
		loadModuleInfo.m_ConnectFactory = Sys_GetFactoryThis();
		loadModuleInfo.m_bSteam = bSteam;
		loadModuleInfo.m_bToolsMode = true;
		if ( FileSystem_LoadFileSystemModule( loadModuleInfo ) != FS_OK )
			return false;

		// Next, mount the content
		CFSMountContentInfo mountContentInfo;
		mountContentInfo.m_pDirectoryName=  loadModuleInfo.m_GameInfoPath;
		mountContentInfo.m_pFileSystem = loadModuleInfo.m_pFileSystem;
		mountContentInfo.m_bToolsMode = true;
		if ( FileSystem_MountContent( mountContentInfo ) != FS_OK )
			return false;
		
		// Finally, load the search paths.
		CFSSearchPathsInit searchPathsInit;
		searchPathsInit.m_pDirectoryName = loadModuleInfo.m_GameInfoPath;
		searchPathsInit.m_pFileSystem = loadModuleInfo.m_pFileSystem;
		if ( FileSystem_LoadSearchPaths( searchPathsInit ) != FS_OK )
			return false;

		// Store the data we got from filesystem_init.
		g_pFileSystem = g_pFullFileSystem = loadModuleInfo.m_pFileSystem;
		g_pFullFileSystemModule = loadModuleInfo.m_pModule;

		FileSystem_AddSearchPath_Platform( g_pFullFileSystem, loadModuleInfo.m_GameInfoPath );

		FileSystem_SetupStandardDirectories( pFilename, loadModuleInfo.m_GameInfoPath );
	}
	else
	{
		if ( !Sys_LoadInterfaceT(
			"filesystem_stdio",
			FILESYSTEM_INTERFACE_VERSION,
			&g_pFullFileSystemModule,
			&g_pFullFileSystem ) )
		{
			return false;
		}

		if ( g_pFullFileSystem->Init() != INIT_OK )
			return false;

		g_pFullFileSystem->RemoveAllSearchPaths();
		g_pFullFileSystem->AddSearchPath( "../platform", "PLATFORM" );
		g_pFullFileSystem->AddSearchPath( ".", "GAME" );

		g_pFileSystem = g_pFullFileSystem;
	}

	return true;
}


bool FileSystem_Init( const char *pBSPFilename, [[maybe_unused]] int maxMemoryUsage, FSInitType_t initType, bool bOnlyUseFilename )
{
	Assert( CommandLine()->GetCmdLine() != nullptr ); // Should have called CreateCmdLine by now.

	// If this app uses VMPI, then let VMPI intercept all filesystem calls.
#if defined( MPI )
	if ( g_bUseMPI )
	{
		if ( g_bMPIMaster )
		{
			if ( !FileSystem_Init_Normal( pBSPFilename, initType, bOnlyUseFilename ) )
				return false;

			g_pFileSystem = g_pFullFileSystem = VMPI_FileSystem_Init( maxMemoryUsage, g_pFullFileSystem );
			SendQDirInfo();
		}
		else
		{
			g_pFileSystem = g_pFullFileSystem = VMPI_FileSystem_Init( maxMemoryUsage, nullptr );
			RecvQDirInfo();
		}
		return true;
	}
#endif

	return FileSystem_Init_Normal( pBSPFilename, initType, bOnlyUseFilename );
}


void FileSystem_Term()
{
#if defined( MPI )
	if ( g_bUseMPI )
	{
		g_pFileSystem = g_pFullFileSystem = VMPI_FileSystem_Term();
	}
#endif

	if ( g_pFullFileSystem )
	{
		g_pFullFileSystem->Shutdown();
		g_pFullFileSystem = nullptr;
		g_pFileSystem = nullptr;
	}

	if ( g_pFullFileSystemModule )
	{
		Sys_UnloadModule( g_pFullFileSystemModule );
		g_pFullFileSystemModule = nullptr;
	}
}


CreateInterfaceFn FileSystem_GetFactory()
{
#if defined( MPI )
	if ( g_bUseMPI )
		return VMPI_FileSystem_GetFactory();
#endif
	return Sys_GetFactory( g_pFullFileSystemModule );
}
