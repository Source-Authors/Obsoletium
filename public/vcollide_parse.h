//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef VCOLLIDE_PARSE_H
#define VCOLLIDE_PARSE_H
#ifdef _WIN32
#pragma once
#endif

#include "vphysics_interface.h"

struct solid_t
{
	int		index;
	char	name[512];
	char	parent[512];
	char	surfaceprop[512];
	Vector	massCenterOverride;
	objectparams_t params;
};

struct fluid_t
{
	int index;
	char surfaceprop[512];

	fluidparams_t params;
};

//-----------------------------------------------------------------------------
// Purpose: Pass this into the parser to handle the keys that vphysics does not
// parse.
//-----------------------------------------------------------------------------
abstract_class IVPhysicsKeyHandler
{
public:
	virtual void ParseKeyValue( void *pData, const char *pKey, const char *pValue ) = 0;
	virtual void SetDefaults( void *pData ) = 0;
};


abstract_class IVPhysicsKeyParser
{
public:
	virtual ~IVPhysicsKeyParser() {}

	[[nodiscard]] virtual const char *GetCurrentBlockName() = 0;
	[[nodiscard]] virtual bool		Finished() = 0;
	virtual void		ParseSolid( solid_t *pSolid, IVPhysicsKeyHandler *unknownKeyHandler ) = 0;
	virtual void		ParseFluid( fluid_t *pFluid, IVPhysicsKeyHandler *unknownKeyHandler ) = 0;
	virtual void		ParseRagdollConstraint( constraint_ragdollparams_t *pConstraint, IVPhysicsKeyHandler *unknownKeyHandler ) = 0;
	virtual void		ParseSurfaceTable( intp *table, IVPhysicsKeyHandler *unknownKeyHandler ) = 0;
	virtual void		ParseCustom( void *pCustom, IVPhysicsKeyHandler *unknownKeyHandler ) = 0;
	virtual void		ParseVehicle( vehicleparams_t *pVehicle, IVPhysicsKeyHandler *unknownKeyHandler ) = 0;
	virtual void		SkipBlock() = 0;
};

#endif // VCOLLIDE_PARSE_H
