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

// dimhotepus: Drop pesky null define
// #define null 0L

using uchar = unsigned char;
using ushort = unsigned short;
using uint = unsigned int;
using ulong = unsigned long;

// dimhotepus: wchar_t is a built-in type in C++; it must never be typedef'd
// here.  The old "GNUC" guard was a typo for __GNUC__ and only compiled by
// accident when <cstddef> happened to be included first.

// do this in GOLDSRC only!!!
//#define Assert assert

namespace vgui
{
// handle to an internal vgui panel
// this is the only handle to a panel that is valid across dll boundaries
using VPANEL = uintp;

// handles to vgui objects
// NULL values signify an invalid value

// dimhotepus: x64 port. unsigned long -> uintp.
using HScheme = uintp;
// dimhotepus: x64 port. unsigned long -> uint32.
// Both -1 and 0 are used for invalid textures. Be careful.
using HTexture = uint32;
// dimhotepus: x64 port. unsigned long -> uint32.
using HCursor = uint32;
// dimhotepus: x64 port. unsigned long -> uint32.
using HPanel = uint32;

constexpr inline HPanel INVALID_PANEL{std::numeric_limits<HPanel>::max()};

// dimhotepus: x86-64 port. unsigned long -> uintp
using HFont = uintp;
constexpr inline HFont INVALID_FONT{0};  // the value of an invalid font handle
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
