//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Defines a group of app systems that all have the same lifetime
// that need to be connected/initialized, etc. in a well-defined order
//
// $Revision: $
// $NoKeywords: $
//=============================================================================

#ifndef IAPPSYSTEMGROUP_H
#define IAPPSYSTEMGROUP_H

#include <type_traits>

#include "tier0/platform.h"
#include "tier1/interface.h"
#include "tier1/utlvector.h"
#include "tier1/utldict.h"
#include "appframework/IAppSystem.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class IAppSystem;
class CSysModule;
class IBaseInterface;
class IFileSystem;

//-----------------------------------------------------------------------------
// Handle to a DLL
//-----------------------------------------------------------------------------
using AppModule_t = intp;

enum
{
	APP_MODULE_INVALID = (AppModule_t)~0
};


//-----------------------------------------------------------------------------
// NOTE: The following methods must be implemented in your application
// although they can be empty implementations if you like...
//-----------------------------------------------------------------------------
abstract_class IAppSystemGroup
{
public:
	// An installed application creation function, you should tell the group
	// the DLLs and the singleton interfaces you want to instantiate.
	// Return false if there's any problems and the app will abort
	virtual bool Create( ) = 0;

	// Allow the application to do some work after AppSystems are connected but 
	// they are all Initialized.
	// Return false if there's any problems and the app will abort
	virtual bool PreInit() = 0;

	// Main loop implemented by the application
	virtual int Main( ) = 0;

	// Allow the application to do some work after all AppSystems are shut down
	virtual void PostShutdown() = 0;

	// Call an installed application destroy function, occurring after all modules
	// are unloaded
	virtual void Destroy() = 0;
};


//-----------------------------------------------------------------------------
// Specifies a module + interface name for initialization
//-----------------------------------------------------------------------------
struct AppSystemInfo_t
{
	const char *m_pModuleName;
	const char *m_pInterfaceName;
};


//-----------------------------------------------------------------------------
// This class represents a group of app systems that all have the same lifetime
// that need to be connected/initialized, etc. in a well-defined order
//-----------------------------------------------------------------------------
class CAppSystemGroup : public IAppSystemGroup
{
public:
	// Used to determine where we exited out from the system
	enum AppSystemGroupStage_t
	{
		CREATION = 0,
		CONNECTION,
		PREINITIALIZATION,
		INITIALIZATION,
		SHUTDOWN,
		POSTSHUTDOWN,
		DISCONNECTION,
		DESTRUCTION,

		NONE,	// This means no error
	};

public:
	// constructor
	CAppSystemGroup( CAppSystemGroup *pParentAppSystem = nullptr );
	virtual ~CAppSystemGroup() {}

	// Runs the app system group.
	// First, modules are loaded, next they are connected, followed by initialization
	// Then Main() is run
	// Then modules are shut down, disconnected, and unloaded
	int Run( );

	// Use this version in cases where you can't control the main loop and
	// expect to be ticked
	virtual int Startup();
	virtual void Shutdown();

	// Returns the stage at which the app system group ran into an error
	AppSystemGroupStage_t GetErrorStage() const;

protected:
	// These methods are meant to be called by derived classes of CAppSystemGroup

	// Methods to load + unload DLLs
	AppModule_t LoadModule( const char *pDLLName );
	AppModule_t LoadModule( CreateInterfaceFn factory );

	// Method to add various global singleton systems 
	IAppSystem *AddSystem( AppModule_t module, const char *pInterfaceName );

	template<typename TAppSystem>
	std::enable_if_t<std::is_base_of_v<IAppSystem, TAppSystem>, TAppSystem *>
	AddSystem( AppModule_t module, const char *pInterfaceName )
	{
		return static_cast<TAppSystem *>( AddSystem( module, pInterfaceName ) );
	}

	void AddSystem( IAppSystem *pAppSystem, const char *pInterfaceName );

	// Simpler method of doing the LoadModule/AddSystem thing.
	// Make sure the last AppSystemInfo has a NULL module name
	bool AddSystems( AppSystemInfo_t *pSystems );

	// Method to look up a particular named system...
	void *FindSystem( const char *pInterfaceName );

	template<typename TSystem>
	TSystem *FindSystem( const char *pInterfaceName )
	{
		return static_cast<TSystem *>( FindSystem( pInterfaceName ) );
	}

	// Gets at a class factory for the topmost appsystem group in an appsystem stack
	static CreateInterfaceFn GetFactory();

private:
	int OnStartup();
	void OnShutdown();

	void UnloadAllModules( );
	void RemoveAllSystems();

	// Method to connect/disconnect all systems
	bool ConnectSystems( );
	void DisconnectSystems();

	// Method to initialize/shutdown all systems
	InitReturnVal_t InitSystems();
	void ShutdownSystems();
 
	// Gets at the parent appsystem group
	CAppSystemGroup *GetParent();

	// Loads a module the standard way
	virtual CSysModule *LoadModuleDLL( const char *pDLLName );

	void	ReportStartupFailure( int nErrorStage, intp nSysIndex );

	struct Module_t
	{
		CSysModule *m_pModule;
		CreateInterfaceFn m_Factory;
		char *m_pModuleName;
	};

	CUtlVector<Module_t> m_Modules;
	CUtlVector<IAppSystem*> m_Systems;
	CUtlDict<intp> m_SystemDict;
	CAppSystemGroup *m_pParentAppSystem;
	AppSystemGroupStage_t m_nErrorStage;

	friend void *AppSystemCreateInterfaceFn(const char *pName, int *pReturnCode);
	friend class CSteamAppSystemGroup;
};


//-----------------------------------------------------------------------------
// This class represents a group of app systems that are loaded through steam
//-----------------------------------------------------------------------------
class CSteamAppSystemGroup : public CAppSystemGroup
{
public:
	CSteamAppSystemGroup( IFileSystem *pFileSystem = nullptr, CAppSystemGroup *pParentAppSystem = nullptr );

	// Used by CSteamApplication to set up necessary pointers if we can't do it in the constructor
	void Setup( IFileSystem *pFileSystem, CAppSystemGroup *pParentAppSystem );

protected:
	// Sets up the search paths
	bool SetupSearchPaths( const char *pStartingDir, bool bOnlyUseStartingDir, bool bIsTool );

	// Returns the game info path. Only works if you've called SetupSearchPaths first
	const char *GetGameInfoPath() const;

private:
	CSysModule *LoadModuleDLL( const char *pDLLName ) override;

	IFileSystem *m_pFileSystem;
	char m_pGameInfoPath[ MAX_PATH ];
};


//-----------------------------------------------------------------------------
// Helper empty decorator implementation of an IAppSystemGroup
//-----------------------------------------------------------------------------
template<typename CBaseClass> 
class CDefaultAppSystemGroup : public CBaseClass
{
public:
	bool Create( ) override { return true; }
	bool PreInit() override { return true; }
	void PostShutdown() override {}
	void Destroy() override {}
};


//-----------------------------------------------------------------------------
// Special helper for game info directory suggestion
//-----------------------------------------------------------------------------

class CFSSteamSetupInfo;	// Forward declaration

//
// SuggestGameInfoDirFn_t
//		Game info suggestion function.
//		Provided by the application to possibly detect the suggested game info
//		directory and initialize all the game-info-related systems appropriately.
// Parameters:
//		pFsSteamSetupInfo		steam file system setup information if available.
//		pchPathBuffer			buffer to hold game info directory path on return.
//		nBufferLength			length of the provided buffer to hold game info directory path.
//		pbBubbleDirectories		should contain "true" on return to bubble the directories up searching for game info file.
// Return values:
//		Returns "true" if the game info directory path suggestion is available and
//		was successfully copied into the provided buffer.
//		Returns "false" otherwise, interpreted that no suggestion will be used.
//
using SuggestGameInfoDirFn_t = bool (*) ( CFSSteamSetupInfo const *pFsSteamSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories );

//
// SetSuggestGameInfoDirFn
//		Installs the supplied game info directory suggestion function.
// Parameters:
//		pfnNewFn				the new game info directory suggestion function.
// Returns:
//		The previously installed suggestion function or NULL if none was installed before.
//		This function never fails.
//
SuggestGameInfoDirFn_t SetSuggestGameInfoDirFn( SuggestGameInfoDirFn_t pfnNewFn );

// dimhotepus: RAII wrapper for SetSuggestGameInfoDirFn.
class ScopedSuggestGameInfoDir
{
public:
  explicit ScopedSuggestGameInfoDir(SuggestGameInfoDirFn_t new_suggest_fn)
     : old_suggest_fn_{SetSuggestGameInfoDirFn(new_suggest_fn)} {}

  ~ScopedSuggestGameInfoDir()
  {
	SetSuggestGameInfoDirFn(old_suggest_fn_);
  }

private:
  const SuggestGameInfoDirFn_t old_suggest_fn_;
};


#endif // APPSYSTEMGROUP_H


