//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: An application framework 
//
//=============================================================================//

#ifdef POSIX
#error "Please exclude from compilation if POSIX"
#else
#include "appframework/appframework.h"
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "interface.h"
#include "filesystem.h"
#include "appframework/IAppSystemGroup.h"
#include "filesystem_init.h"
#include "vstdlib/cvar.h"
#include "xbox/xbox_console.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

extern "C" __declspec(dllimport) char* __stdcall GetCommandLineA();

FORWARD_DECLARE_HANDLE(HINSTANCE);

//-----------------------------------------------------------------------------
// Globals...
//-----------------------------------------------------------------------------
HINSTANCE s_HInstance;

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
int AppMain( void* hInstance, void*, const char*, int, CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );

	s_HInstance = (HINSTANCE)hInstance;
	CommandLine()->CreateCmdLine( ::GetCommandLineA() );

	return pAppSystemGroup->Run();
}

//-----------------------------------------------------------------------------
// Version of AppMain used by console applications
//-----------------------------------------------------------------------------
int AppMain( int argc, char **argv, CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );

	s_HInstance = NULL;
	CommandLine()->CreateCmdLine( argc, argv );

	return pAppSystemGroup->Run();
}

//-----------------------------------------------------------------------------
// Used to startup/shutdown the application
//-----------------------------------------------------------------------------
int AppStartup( void* hInstance, void*, const char*, int, CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );

	s_HInstance = (HINSTANCE)hInstance;
	CommandLine()->CreateCmdLine( ::GetCommandLineA() );

	return pAppSystemGroup->Startup();
}

int AppStartup( int argc, char **argv, CAppSystemGroup *pAppSystemGroup )
{
	Assert( pAppSystemGroup );

	s_HInstance = NULL;
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
{
	m_pChildAppSystemGroup = pAppSystemGroup;
	m_pFileSystem = NULL;
	m_bSteam = false;
}

//-----------------------------------------------------------------------------
// Create necessary interfaces
//-----------------------------------------------------------------------------
bool CSteamApplication::Create()
{
	FileSystem_SetErrorMode( FS_ERRORMODE_AUTO );

	char pFileSystemDLL[MAX_PATH];
	if ( FileSystem_GetFileSystemDLLName( pFileSystemDLL, m_bSteam ) != FS_OK )
		return false;
	
	// Add in the cvar factory
	AppModule_t cvarModule = LoadModule( VStdLib_GetICVarFactory() );
	AddSystem( cvarModule, CVAR_INTERFACE_VERSION );

	AppModule_t fileSystemModule = LoadModule( pFileSystemDLL );
	m_pFileSystem = (IFileSystem*)AddSystem( fileSystemModule, FILESYSTEM_INTERFACE_VERSION );
	if ( !m_pFileSystem )
	{
		Error( "Unable to load %s from %s", FILESYSTEM_INTERFACE_VERSION, pFileSystemDLL );
	}

	return true;
}

//-----------------------------------------------------------------------------
// The file system pointer is invalid at this point
//-----------------------------------------------------------------------------
void CSteamApplication::Destroy()
{
	m_pFileSystem = NULL;
}

//-----------------------------------------------------------------------------
// Pre-init, shutdown
//-----------------------------------------------------------------------------
bool CSteamApplication::PreInit()
{
	return true;
}

void CSteamApplication::PostShutdown()
{
}

//-----------------------------------------------------------------------------
// Run steam main loop
//-----------------------------------------------------------------------------
int CSteamApplication::Main()
{
	// Now that Steam is loaded, we can load up main libraries through steam
	if ( FileSystem_SetBasePaths( m_pFileSystem ) != FS_OK )
		return 0;

	m_pChildAppSystemGroup->Setup( m_pFileSystem, this );
	return m_pChildAppSystemGroup->Run();
}

//-----------------------------------------------------------------------------
// Use this version in cases where you can't control the main loop and
// expect to be ticked
//-----------------------------------------------------------------------------
int CSteamApplication::Startup()
{
	int nRetVal = BaseClass::Startup();
	if ( GetErrorStage() != NONE )
		return nRetVal;

	if ( FileSystem_SetBasePaths( m_pFileSystem ) != FS_OK )
		return 0;

	// Now that Steam is loaded, we can load up main libraries through steam
	m_pChildAppSystemGroup->Setup( m_pFileSystem, this );
	return m_pChildAppSystemGroup->Startup();
}

void CSteamApplication::Shutdown()
{
	m_pChildAppSystemGroup->Shutdown();
	BaseClass::Shutdown();
}

#endif
