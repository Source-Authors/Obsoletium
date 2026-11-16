//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Pieces of the application framework, shared between POSIX systems (Mac OS X, Linux, etc)
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//
#include "appframework/AppFramework.h"
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "tier1/interface.h"
#include "filesystem.h"
#include "appframework/IAppSystemGroup.h"
#include "filesystem_init.h"
#include "tier1/convar.h"
#include "vstdlib/cvar.h"
#include "togl/rendermechanism.h"

// NOTE: This has to be the last file included! (turned off below, since this is included like a header)
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Globals...
//-----------------------------------------------------------------------------
// dimhotepus: Internal linkage, same as on Windows (was exported global).
static HINSTANCE s_HInstance;

//-----------------------------------------------------------------------------
// HACK: Since I don't want to refit vgui yet...
//-----------------------------------------------------------------------------
void *GetAppInstance()
{
	return s_HInstance;
}


//-----------------------------------------------------------------------------
// Sets the application instance, should only be used if you're not calling AppMain.
//-----------------------------------------------------------------------------
void SetAppInstance( void* hInstance )
{
	s_HInstance = (HINSTANCE)hInstance;
}


//-----------------------------------------------------------------------------
// Version of AppMain used by windows applications
//-----------------------------------------------------------------------------

int AppMain( void*, void*, const char*, int, CAppSystemGroup * )
{
	// Windowed WinMain-style entry point does not exist on POSIX.
	Assert( 0 );
	return -1;
}

//-----------------------------------------------------------------------------
// Version of AppMain used by console applications
//-----------------------------------------------------------------------------
int AppMain( int argc, char **argv, CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );

	s_HInstance = nullptr;
	CommandLine()->CreateCmdLine( argc, argv );

	return pAppSystemGroup->Run( );
}

//-----------------------------------------------------------------------------
// Used to startup/shutdown the application
//-----------------------------------------------------------------------------
// dimhotepus: Declared in AppFramework.h but were missing on POSIX (link errors for users).
int AppStartup( void*, void*, const char*, int, CAppSystemGroup * )
{
	// Windowed WinMain-style entry point does not exist on POSIX.
	Assert( 0 );
	return -1;
}

int AppStartup( int argc, char **argv, CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );

	s_HInstance = nullptr;
	CommandLine()->CreateCmdLine( argc, argv );

	return pAppSystemGroup->Startup();
}

void AppShutdown( CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );
	pAppSystemGroup->Shutdown();
}


//-----------------------------------------------------------------------------
//
// Default implementation of an application meant to be run using Steam
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSteamApplication::CSteamApplication( CSteamAppSystemGroup *pAppSystemGroup )
	: m_pChildAppSystemGroup{ pAppSystemGroup }
{
	Assert( pAppSystemGroup );
}


//-----------------------------------------------------------------------------
// Create necessary interfaces
//-----------------------------------------------------------------------------
bool CSteamApplication::Create( )
{
	FileSystem_SetErrorMode( FS_ERRORMODE_NONE );

	char pFileSystemDLL[MAX_PATH];
	if ( FileSystem_GetFileSystemDLLName( pFileSystemDLL, m_bSteam ) != FS_OK )
		return false;

	// Add in the cvar factory
	AppModule_t cvarModule = LoadModule( VStdLib_GetICVarFactory() );
	AddSystem( cvarModule, CVAR_INTERFACE_VERSION );	

	AppModule_t fileSystemModule = LoadModule( pFileSystemDLL );
	m_pFileSystem = AddSystem<IFileSystem>( fileSystemModule, FILESYSTEM_INTERFACE_VERSION );
	if ( !m_pFileSystem )
	{
		Error( "Unable to load %s", pFileSystemDLL );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// The file system pointer is invalid at this point
//-----------------------------------------------------------------------------
void CSteamApplication::Destroy()
{
	m_pFileSystem = nullptr;
}


//-----------------------------------------------------------------------------
// Pre-init, shutdown
//-----------------------------------------------------------------------------
bool CSteamApplication::PreInit( )
{
	return true;
}

void CSteamApplication::PostShutdown( )
{
}


//-----------------------------------------------------------------------------
// Run steam main loop
//-----------------------------------------------------------------------------
int CSteamApplication::Main( )
{
	// dimhotepus: Same as Windows version, set base paths first. Return failure code
	// instead of 0 (success exit code).
	if ( FileSystem_SetBasePaths( m_pFileSystem ) != FS_OK )
		return -1;

	// Now that Steam is loaded, we can load up main libraries through steam
	m_pChildAppSystemGroup->Setup( m_pFileSystem, this );
	return m_pChildAppSystemGroup->Run( );
}


int CSteamApplication::Startup()
{
	int nRetVal = BaseClass::Startup();
	if ( GetErrorStage() != NONE )
		return nRetVal;
	
	// dimhotepus: Return failure code instead of 0 (success).
	if ( FileSystem_SetBasePaths( m_pFileSystem ) != FS_OK )
		return -1;
	
	// Now that Steam is loaded, we can load up main libraries through steam
	m_pChildAppSystemGroup->Setup( m_pFileSystem, this );
	return m_pChildAppSystemGroup->Startup();
}


void CSteamApplication::Shutdown()
{
	m_pChildAppSystemGroup->Shutdown();
	BaseClass::Shutdown();
}

// Turn off memdbg macros (turned on up top) since this is included like a header
#include "tier0/memdbgoff.h"