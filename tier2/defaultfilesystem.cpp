//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A higher level link library for general use in the game and tools.
//
//===========================================================================//

#include <tier2/tier2.h>
#include <tier0/platform.h>
#include <filesystem_init.h>


static CSysModule *g_pFullFileSystemModule = NULL;

void* DefaultCreateInterfaceFn(const char *, int *pReturnCode)
{
	if ( pReturnCode )
	{
		*pReturnCode = 0;
	}
	return NULL;
}

void InitDefaultFileSystem( void )
{
	AssertMsg( !g_pFullFileSystem, "Already set up the file system" );

	if ( !Sys_LoadInterface( "filesystem_stdio", FILESYSTEM_INTERFACE_VERSION,
		&g_pFullFileSystemModule, (void**)&g_pFullFileSystem ) )
	{
		if ( !Sys_LoadInterface( "filesystem_steam", FILESYSTEM_INTERFACE_VERSION,
			&g_pFullFileSystemModule, (void**)&g_pFullFileSystem ) )
		{
			Warning("Unable to find interface '%s' in both filesystem_stdio"
				DLL_EXT_STRING " and filesystem_steam" DLL_EXT_STRING ".\n",
				FILESYSTEM_INTERFACE_VERSION);
			// dimhotepus: Signal error code.
			exit(1);
		}
	}

	if ( !g_pFullFileSystem->Connect( DefaultCreateInterfaceFn ) )
	{
		Warning("Unable to connect file system interfaces.\n");
		// dimhotepus: Signal error code.
		exit(2);
	}

	if ( g_pFullFileSystem->Init() != INIT_OK )
	{
		Warning("Unable to init file system interfaces.\n");
		// dimhotepus: Signal error code.
		exit(3);
	}

	g_pFullFileSystem->RemoveAllSearchPaths();
	g_pFullFileSystem->AddSearchPath( "", "LOCAL", PATH_ADD_TO_HEAD );
}

void ShutdownDefaultFileSystem(void)
{
	AssertMsg( g_pFullFileSystem, "File system not set up" );
	g_pFullFileSystem->Shutdown();
	g_pFullFileSystem->Disconnect();
	Sys_UnloadModule( g_pFullFileSystemModule );
}
