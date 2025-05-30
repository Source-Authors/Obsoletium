//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef TRACE_H
#define TRACE_H

#ifdef _WIN32
#pragma once
#endif


#include "mathlib/mathlib.h"

// Note: These flags need to match the bspfile.h DISPTRI_TAG_* flags.
#define DISPSURF_FLAG_SURFACE		(1<<0)
#define DISPSURF_FLAG_WALKABLE		(1<<1)
#define DISPSURF_FLAG_BUILDABLE		(1<<2)
#define DISPSURF_FLAG_SURFPROP1		(1<<3)
#define DISPSURF_FLAG_SURFPROP2		(1<<4)

//=============================================================================
// Base Trace Structure
// - shared between engine/game dlls and tools (vrad)
//=============================================================================

class CBaseTrace
{
public:

	// Displacement flags tests.
	bool IsDispSurface() const				{ return ( ( dispFlags & DISPSURF_FLAG_SURFACE ) != 0 ); }
	bool IsDispSurfaceWalkable() const		{ return ( ( dispFlags & DISPSURF_FLAG_WALKABLE ) != 0 ); }
	bool IsDispSurfaceBuildable() const		{ return ( ( dispFlags & DISPSURF_FLAG_BUILDABLE ) != 0 ); }
	bool IsDispSurfaceProp1() const			{ return ( ( dispFlags & DISPSURF_FLAG_SURFPROP1 ) != 0 ); }
	bool IsDispSurfaceProp2() const			{ return ( ( dispFlags & DISPSURF_FLAG_SURFPROP2 ) != 0 ); }

public:

	// these members are aligned!!
	Vector			startpos;				// start position
	Vector			endpos;					// final position
	cplane_t		plane;					// surface normal at impact

	float			fraction;				// time completed, 1.0 = didn't hit anything

	int				contents;				// contents on other side of surface hit
	unsigned short	dispFlags;				// displacement flags for marking surfaces with data

	bool			allsolid;				// if true, plane is not valid
	bool			startsolid;				// if true, the initial point was in a solid area

	CBaseTrace() 
		: startpos(), endpos(),
		fraction{FLOAT32_NAN}, contents{0}, dispFlags{0},
		allsolid{false}, startsolid{false}
	{
		memset( &plane, 0, sizeof(plane) );
	}

	// No copy constructors allowed
	CBaseTrace(const CBaseTrace& vOther) = default;
	CBaseTrace& operator=(const CBaseTrace& vOther) = default;
};

#endif // TRACE_H
