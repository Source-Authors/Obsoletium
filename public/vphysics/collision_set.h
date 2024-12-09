//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef SE_VPHYSICS_COLLISION_SET_H_
#define SE_VPHYSICS_COLLISION_SET_H_

// A set of collision rules
// NOTE: Defaults to all indices disabled
class IPhysicsCollisionSet
{
public:
	virtual ~IPhysicsCollisionSet() {}

	virtual void EnableCollisions( int index0, int index1 ) = 0;
	virtual void DisableCollisions( int index0, int index1 ) = 0;

	virtual bool ShouldCollide( int index0, int index1 ) = 0;
};

#endif
