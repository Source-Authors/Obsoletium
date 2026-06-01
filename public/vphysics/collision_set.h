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

	virtual void EnableCollisions( intp index0, intp index1 ) = 0;
	virtual void DisableCollisions( intp index0, intp index1 ) = 0;

	// dimhotepus: Add const.
	[[nodiscard]] virtual bool ShouldCollide( intp index0, intp index1 ) const = 0;
};

#endif
