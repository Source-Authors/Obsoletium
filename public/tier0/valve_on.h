// Copyright Valve Corporation, All rights reserved.
//
// Note: intentionally has no include guard; it must take effect every time
// it is included.
//
// Purpose:	This turns on all Valve-specific #defines.  Because we sometimes
// call external include files from inside .cpp files, we need to wrap those
// includes like this:
//  #include "tier0/valve_off.h"
//  #include <external.h>
//  #include "tier0/valve_on.h"

#ifdef STEAM
//-----------------------------------------------------------------------------
// Unicode-related #defines (see wchartypes.h)
//-----------------------------------------------------------------------------
#ifdef ENFORCE_WCHAR
#define char DontUseChar_SeeWchartypes_h  //-V1059
#endif

//-----------------------------------------------------------------------------
// Memory-related #defines
//-----------------------------------------------------------------------------
#define malloc( cub ) HEY_DONT_USE_MALLOC_USE_PVALLOC             //-V1059
#define realloc( pvOld, cub ) HEY_DONT_USE_REALLOC_USE_PVREALLOC  //-V1059
#define _expand( pvOld, cub ) HEY_DONT_USE_EXPAND_USE_PVEXPAND    //-V1059
#define free( pv ) HEY_DONT_USE_FREE_USE_FREEPV                   //-V1059

#endif