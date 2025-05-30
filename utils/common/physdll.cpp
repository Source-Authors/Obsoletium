//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "physdll.h"
#include "filesystem_tools.h"

static CSysModule *pPhysicsModule = NULL;
CreateInterfaceFnT<IPhysicsCollision> GetPhysicsFactory( void )
{
	PhysicsDLLPath( "vphysics" DLL_EXT_STRING );
	if ( !pPhysicsModule )
		return nullptr;

	return Sys_GetFactory<IPhysicsCollision>( pPhysicsModule );
}

CreateInterfaceFnT<IPhysicsSurfaceProps> GetPhysicsFactory2( void )
{
	PhysicsDLLPath( "vphysics" DLL_EXT_STRING );
	if ( !pPhysicsModule )
		return nullptr;

	return Sys_GetFactory<IPhysicsSurfaceProps>(pPhysicsModule);
}


void PhysicsDLLPath( const char *pPathname )
{
	if ( !pPhysicsModule )
	{
		pPhysicsModule = g_pFullFileSystem->LoadModule( pPathname );
	}
}
