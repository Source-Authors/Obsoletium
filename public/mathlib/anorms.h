//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ANORMS_H
#define ANORMS_H
#ifdef _WIN32
#pragma once
#endif


#include "mathlib/mathlib.h"
#include "mathlib/vector.h"


#define NUMVERTEXNORMALS	162

// the angle between consecutive g_anorms[] vectors is ~14.55 degrees
constexpr inline float VERTEXNORMAL_CONE_INNER_ANGLE{DEG2RAD(7.275f)};

extern Vector g_anorms[NUMVERTEXNORMALS];


#endif // ANORMS_H
