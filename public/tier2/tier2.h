//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A higher level link library for general use in the game and tools.
//
//===========================================================================//


#ifndef TIER2_H
#define TIER2_H

#include "tier1/tier1.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IFileSystem;
class IMaterialSystem;
class IColorCorrectionSystem;
class IMaterialSystemHardwareConfig;
class IDebugTextureInfo;
class IVBAllocTracker;
class IInputSystem;
class INetworkSystem;
class IP4;
class IMdlLib;
class IQueuedLoader;


//-----------------------------------------------------------------------------
// These tier2 libraries must be set by any users of this library.
// They can be set by calling ConnectTier2Libraries or InitDefaultFileSystem.
// It is hoped that setting this, and using this library will be the common mechanism for
// allowing link libraries to access tier2 library interfaces
//-----------------------------------------------------------------------------
extern IFileSystem *g_pFullFileSystem;
extern IMaterialSystem *materials;
extern IMaterialSystem *g_pMaterialSystem;
extern IInputSystem *g_pInputSystem;
extern INetworkSystem *g_pNetworkSystem;
extern IMaterialSystemHardwareConfig *g_pMaterialSystemHardwareConfig;
extern IDebugTextureInfo *g_pMaterialSystemDebugTextureInfo;
extern IVBAllocTracker *g_VBAllocTracker;
extern IColorCorrectionSystem *colorcorrection;
extern IP4 *p4; //-V707
extern IMdlLib *mdllib;
extern IQueuedLoader *g_pQueuedLoader;


//-----------------------------------------------------------------------------
// Call this to connect to/disconnect from all tier 2 libraries.
// It's up to the caller to check the globals it cares about to see if ones are missing
//-----------------------------------------------------------------------------
void ConnectTier2Libraries( CreateInterfaceFn *pFactoryList, intp nFactoryCount );
void DisconnectTier2Libraries();


//-----------------------------------------------------------------------------
// Call this to get the file system set up to stdio for utilities, etc:
//-----------------------------------------------------------------------------
void InitDefaultFileSystem();
void ShutdownDefaultFileSystem();

// dimhotepus: Add RAII wrapper over InitDefaultFileSystem.
class ScopedDefaultFileSystem
{
public:
  ScopedDefaultFileSystem()
  {
	InitDefaultFileSystem();
  }
  ~ScopedDefaultFileSystem()
  {
	ShutdownDefaultFileSystem();
  }

  ScopedDefaultFileSystem(ScopedDefaultFileSystem &) = delete;
  ScopedDefaultFileSystem(ScopedDefaultFileSystem &&) = delete;
  ScopedDefaultFileSystem& operator=(ScopedDefaultFileSystem &) = delete;
  ScopedDefaultFileSystem& operator=(ScopedDefaultFileSystem &&) = delete;
};


//-----------------------------------------------------------------------------
// for simple utilities using valve libraries, call the entry point below in main(). It will
// init a filesystem for you, init mathlib, and create the command line.
//-----------------------------------------------------------------------------
void InitCommandLineProgram( int argc, char **argv );
void ShutdownCommandLineProgram();

// dimhotepus: Add RAII wrapper over InitCommandLineProgram.
class ScopedCommandLineProgram
{
public:
  ScopedCommandLineProgram( int argc, char **argv )
  {
	InitCommandLineProgram( argc, argv );
  }
  ~ScopedCommandLineProgram()
  {
	ShutdownCommandLineProgram();
  }

  ScopedCommandLineProgram(ScopedCommandLineProgram &) = delete;
  ScopedCommandLineProgram(ScopedCommandLineProgram &&) = delete;
  ScopedCommandLineProgram& operator=(ScopedCommandLineProgram &) = delete;
  ScopedCommandLineProgram& operator=(ScopedCommandLineProgram &&) = delete;
};


//-----------------------------------------------------------------------------
// Helper empty implementation of an IAppSystem for tier2 libraries
//-----------------------------------------------------------------------------
template< class IInterface, int ConVarFlag = 0 > 
class CTier2AppSystem : public CTier1AppSystem< IInterface, ConVarFlag >
{
	using BaseClass = CTier1AppSystem<IInterface, ConVarFlag>;

public:
	CTier2AppSystem( bool bIsPrimaryAppSystem = true ) : BaseClass( bIsPrimaryAppSystem )
	{
	}

	bool Connect( CreateInterfaceFn factory ) override
	{
		if ( !BaseClass::Connect( factory ) )
			return false;

		if ( BaseClass::IsPrimaryAppSystem() )
		{
			ConnectTier2Libraries( &factory, 1 );
		}

		return true;
	}

	InitReturnVal_t Init() override
	{
		InitReturnVal_t nRetVal = BaseClass::Init();
		if ( nRetVal != INIT_OK )
			return nRetVal;

		return INIT_OK;
	}

	void Shutdown() override
	{
		BaseClass::Shutdown();
	}

	void Disconnect() override
	{
		if ( BaseClass::IsPrimaryAppSystem() )
		{
			DisconnectTier2Libraries();
		}
		BaseClass::Disconnect();
	}
};


#endif // TIER2_H

