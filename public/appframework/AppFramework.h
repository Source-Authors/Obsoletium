//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: An application framework 
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#ifndef APPFRAMEWORK_H
#define APPFRAMEWORK_H

#include "appframework/IAppSystemGroup.h"
#include "tier0/annotations.h"


//-----------------------------------------------------------------------------
// Gets the application instance..
//-----------------------------------------------------------------------------
[[nodiscard]] void *GetAppInstance();


//-----------------------------------------------------------------------------
// Sets the application instance, should only be used if you're not calling AppMain.
//-----------------------------------------------------------------------------
void SetAppInstance( void* hInstance );


//-----------------------------------------------------------------------------
// Main entry point for the application
//-----------------------------------------------------------------------------
int AppMain( void* hInstance, void* hPrevInstance, const char* lpCmdLine, int nCmdShow, CAppSystemGroup *pAppSystemGroup );
int AppMain( int argc, char **argv, CAppSystemGroup *pAppSystemGroup );


//-----------------------------------------------------------------------------
// Used to startup/shutdown the application
//-----------------------------------------------------------------------------
int AppStartup( void* hInstance, void* hPrevInstance, const char* lpCmdLine, int nCmdShow, CAppSystemGroup *pAppSystemGroup );
int AppStartup( int argc, char **argv, CAppSystemGroup *pAppSystemGroup );
void AppShutdown( CAppSystemGroup *pAppSystemGroup );


//-----------------------------------------------------------------------------
// Macros to create singleton application objects for windowed + console apps
//-----------------------------------------------------------------------------
#if defined( _WIN32 ) || defined( WIN32 )
#define DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR( _globalVarName ) \
	int __stdcall WinMain( IN_P HINSTANCE hInstance, IN_OPT HINSTANCE hPrevInstance, IN_P char *lpCmdLine, IN_P int nCmdShow )	\
	{																							\
		return AppMain( hInstance, hPrevInstance, lpCmdLine, nCmdShow, &_globalVarName );		\
	}
#elif defined( OSX ) || defined( __APPLE__ )
#define DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR( _globalVarName ) \
	int main( int argc, char **argv )										\
	{																							\
		extern int ValveCocoaMain( int argc, char **argv, CAppSystemGroup *pAppSystemGroup ); \
		return ValveCocoaMain( argc, argv, &_globalVarName ); \
	}
#elif defined( LINUX ) || defined( __linux__ )
#define DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR( _globalVarName ) \
	int main( int argc, char **argv )										\
	{																							\
		extern int ValveLinuxWindowedMain( int argc, char **argv, CAppSystemGroup *pAppSystemGroup ); \
		return ValveLinuxWindowedMain( argc, argv, &_globalVarName ); \
	}
#else
#error "Please define your platform"
#endif

#define DEFINE_CONSOLE_APPLICATION_OBJECT_GLOBALVAR( _globalVarName ) \
	int main( int argc, char **argv )			\
	{											\
		return AppMain( argc, argv, &_globalVarName );	\
	}

// dimhotepus: Identifiers containing double underscore are reserved for the
// implementation in C++ (UB to define them). Renamed __s_* -> s_*.
#define DEFINE_WINDOWED_APPLICATION_OBJECT( _className )	\
	static _className s_ApplicationObject;					\
	DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR( s_ApplicationObject )

#define DEFINE_CONSOLE_APPLICATION_OBJECT( _className )	\
	static _className s_ApplicationObject;				\
	DEFINE_CONSOLE_APPLICATION_OBJECT_GLOBALVAR( s_ApplicationObject )


//-----------------------------------------------------------------------------
// This class is a helper class used for steam-based applications.
// It loads up the file system in preparation for using it to load other
// required modules from steam.
//-----------------------------------------------------------------------------
class CSteamApplication : public CAppSystemGroup
{
	using BaseClass = CAppSystemGroup;

public:
	// dimhotepus: explicit to prevent accidental implicit conversion from pointer.
	explicit CSteamApplication( CSteamAppSystemGroup *pAppSystemGroup );

	// dimhotepus: Application owns system state, copying is never valid.
	CSteamApplication( const CSteamApplication & ) = delete;
	CSteamApplication &operator=( const CSteamApplication & ) = delete;

	// Implementation of IAppSystemGroup
	bool Create( ) override;
	bool PreInit( ) override;
	int Main( ) override;
	void PostShutdown() override;
	void Destroy() override;

	// Use this version in cases where you can't control the main loop and
	// expect to be ticked
	int Startup() override;
	void Shutdown() override;

protected:
	// dimhotepus: Default initialize to avoid garbage on early failure paths.
	IFileSystem *m_pFileSystem{nullptr};
	CSteamAppSystemGroup *m_pChildAppSystemGroup{nullptr};
	bool m_bSteam{false};
};


//-----------------------------------------------------------------------------
// Macros to help create singleton application objects for windowed + console steam apps
//-----------------------------------------------------------------------------
#define DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT_GLOBALVAR( _className, _varName )	\
	static CSteamApplication s_SteamApplicationObject( &_varName );	\
	DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR( s_SteamApplicationObject )

#define DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( _className )	\
	static _className s_ApplicationObject;				\
	static CSteamApplication s_SteamApplicationObject( &s_ApplicationObject );	\
	DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR( s_SteamApplicationObject )

#define DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT_GLOBALVAR( _className, _varName )	\
	static CSteamApplication s_SteamApplicationObject( &_varName );	\
	DEFINE_CONSOLE_APPLICATION_OBJECT_GLOBALVAR( s_SteamApplicationObject )

#define DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT( _className )	\
	static _className s_ApplicationObject;			\
	static CSteamApplication s_SteamApplicationObject( &s_ApplicationObject );	\
	DEFINE_CONSOLE_APPLICATION_OBJECT_GLOBALVAR( s_SteamApplicationObject )

#endif // APPFRAMEWORK_H