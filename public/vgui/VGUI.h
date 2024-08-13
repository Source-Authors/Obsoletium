//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Basic header for using vgui
//
// $NoKeywords: $
//=============================================================================//

#ifndef VGUI_H
#define VGUI_H

#ifdef _WIN32
#pragma once
#endif

#include <limits> 

#include "tier0/platform.h"

#define null 0L

#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    nullptr
#endif
#endif

using uchar = unsigned char;
using ushort = unsigned short;
using uint = unsigned int;
using ulong = unsigned long;

#ifndef _WCHAR_T_DEFINED
// DAL - wchar_t is a built in define in gcc 3.2 with a size of 4 bytes
#if !defined( __x86_64__ ) && !defined( __WCHAR_TYPE__  )
typedef unsigned short wchar_t;
#define _WCHAR_T_DEFINED
#endif
#endif

// do this in GOLDSRC only!!!
//#define Assert assert

namespace vgui
{
// handle to an internal vgui panel
// this is the only handle to a panel that is valid across dll boundaries
using VPANEL = uintp;

// handles to vgui objects
// NULL values signify an invalid value
using HScheme = unsigned long;
// Both -1 and 0 are used for invalid textures. Be careful.
using HTexture = unsigned long;
using HCursor = unsigned long;
using HPanel = unsigned long;

constexpr inline HPanel INVALID_PANEL{std::numeric_limits<HPanel>::max()};

// dimhotepus: x86-64 port. unsigned long -> uintp
using HFont = uintp;
constexpr HFont INVALID_FONT{0};  // the value of an invalid font handle
}

#include "tier1/strtools.h"

#if 0 // defined( OSX ) // || defined( LINUX )
// Disabled all platforms. Did a major cleanup of osxfont.cpp, and having this
//  turned off renders much closer to Windows and Linux and also uses the same
//  code paths (which is good).
#define USE_GETKERNEDCHARWIDTH 1
#else
#define USE_GETKERNEDCHARWIDTH 0
#endif


#endif // VGUI_H
