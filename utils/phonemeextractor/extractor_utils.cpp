//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "winlite.h"
#include "tier1/strtools.h"
#include <stdio.h>

char *va( const char *fmt, ... )
{
	va_list args;
	static char output[4][1024];
	static int outbuffer = 0;

	outbuffer++;
	va_start( args, fmt );
	vprintf( fmt, args );
	V_vsprintf_safe( output[ outbuffer & 3 ], fmt, args );
	return output[ outbuffer & 3 ];
}