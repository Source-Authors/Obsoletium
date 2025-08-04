//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef EFFECT_COLOR_TABLES_H
#define EFFECT_COLOR_TABLES_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/basetypes.h"

struct colorentry_t
{
	byte	index;
	
	byte	r;
	byte	g;
	byte	b;
};

// dimhotepus: return size_t.
template<size_t size>
[[nodiscard]] constexpr inline size_t COLOR_TABLE_SIZE(colorentry_t (&table)[size])
{
	return size;
}

// Commander mode indicators (HL2)
enum
{
	COMMAND_POINT_RED = 0,
	COMMAND_POINT_BLUE,
	COMMAND_POINT_GREEN,
	COMMAND_POINT_YELLOW,
};

// Commander mode table
// dimhotepus: Colors are unsigned int, so 1 -> 255.
static colorentry_t commandercolors[] =
{
	{ COMMAND_POINT_RED,	255,	0,		0	},
	{ COMMAND_POINT_BLUE,	0,		0,		255	},
	{ COMMAND_POINT_GREEN,	0,		255,	0	},
	{ COMMAND_POINT_YELLOW,	255,	255,	0	},
};

static colorentry_t bloodcolors[] =
{
	{ BLOOD_COLOR_RED,		72,		0,		0	},
	{ BLOOD_COLOR_YELLOW,	195,	195,	0	},
	{ BLOOD_COLOR_MECH,		20,		20,		20	},
	{ BLOOD_COLOR_GREEN,	195,	195,	0	},
};

#endif // EFFECT_COLOR_TABLES_H
