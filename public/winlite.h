//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef WINLITE_H
#define WINLITE_H

#ifdef _WIN32

// Prevent tons of unused windows definitions
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Strict types mode.
#ifndef STRICT
#define STRICT
#endif

// Access to Windows 10 features.
#define _WIN32_WINNT 0x0A00

#define NOMINMAX
#define NOWINRES
#define NOSERVICE
#define NOMCX
#define NOIME

#if !defined( _X360 )
#include <Windows.h>
#include <VersionHelpers.h>
#endif

#undef PostMessage

#endif // WIN32

#endif // WINLITE_H
