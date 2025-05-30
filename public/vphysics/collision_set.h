//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef SE_VPHYSICS_COLLISION_SET_H_
#define SE_VPHYSICS_COLLISION_SET_H_

#include "tier0/platform.h"

// A set of collision rules
// NOTE: Defaults to all indices disabled
abstract_class IPhysicsCollisionSet
{
public:
	virtual ~IPhysicsCollisionSet() {}

	virtual void EnableCollisions( int index0, int index1 ) = 0;
	virtual void DisableCollisions( int index0, int index1 ) = 0;

	[[nodiscard]] virtual bool ShouldCollide( int index0, int index1 ) = 0;
};

#endif
