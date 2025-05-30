//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSDLL_H
#define PHYSDLL_H
#pragma once


#ifdef __cplusplus
#include "vphysics_interface.h"
class IPhysics;
class IPhysicsCollision;
class IPhysicsSurfaceProps;

extern CreateInterfaceFnT<IPhysicsCollision> GetPhysicsFactory(void);
extern CreateInterfaceFnT<IPhysicsSurfaceProps> GetPhysicsFactory2(void);

extern "C" {
#endif

// tools need to force the path
void					PhysicsDLLPath( const char *pPathname );

#ifdef __cplusplus
}
#endif

#endif // PHYSDLL_H
