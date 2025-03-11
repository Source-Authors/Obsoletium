// Copyright Valve Corporation, All rights reserved.

#ifndef SE_UTILS_STUDIOMDL_COLLISIONMODEL_H_
#define SE_UTILS_STUDIOMDL_COLLISIONMODEL_H_

#include "mathlib/vector.h"

void Cmd_CollisionText( void );
int DoCollisionModel( bool separateJoints );

// execute after simplification, before writing
void CollisionModel_Build( void );
// execute during writing
void CollisionModel_Write( long checkSum );

void CollisionModel_ExpandBBox( Vector &mins, Vector &maxs );

#endif  // !SE_UTILS_STUDIOMDL_COLLISIONMODEL_H_
