//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

===== h_export.cpp ========================================================

  Entity classes exported by Halflife.

*/

#if defined(_WIN32) && !defined(_XBOX)

#include "winlite.h"
#include "datamap.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

HMODULE win32DLLHandle;

// Required DLL entry point
BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID )
{
	// ensure data sizes are stable
	static_assert( sizeof(inputfunc_t) == sizeof(void*) );

	if ( fdwReason == DLL_PROCESS_ATTACH )
    {
		win32DLLHandle = hinstDLL;
    }
	else if ( fdwReason == DLL_PROCESS_DETACH )
    {
		win32DLLHandle = nullptr;
    }
	return TRUE;
}

#endif

