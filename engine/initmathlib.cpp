//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//


#include "mathlib/mathlib.h"
#include "convar.h" // ConVar define
#include "view.h"
#include "gl_cvars.h" // mat_overbright
#include "cmd.h" // Cmd_*
#include "console.h"  // ConMsg

#include "materialsystem/imaterialsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static bool s_bAllow3DNow = true;
static bool s_bAllowSSE2 = true;

void InitMathlib()
{
	MathLib_Init( GAMMA, TEXGAMMA, 0.0f, OVERBRIGHT, s_bAllow3DNow, true, s_bAllowSSE2, true );
}

/*
===============
R_SSE2
===============
*/
CON_COMMAND( r_sse2, "Enable/disable SSE2 code" )
{
	if (args.ArgC() == 1)
	{
		s_bAllowSSE2 = true;
	}
	else
	{
		s_bAllowSSE2 = atoi( args[1] ) ? true : false;
	}

	InitMathlib();
	ConMsg( "SSE2 code is %s\n", MathLib_SSE2Enabled() ? "enabled" : "disabled" );
}

/*
===============
R_3DNow
===============
*/
CON_COMMAND( r_3dnow, "Enable/disable 3DNow code" )
{
	if (args.ArgC() == 1)
	{
		s_bAllow3DNow = true;
	}
	else
	{
		s_bAllow3DNow  = atoi( args[1] ) ? true : false;
	}

	InitMathlib();
	ConMsg( "3DNow code is %s\n", MathLib_3DNowEnabled() ? "enabled" : "disabled" );
}