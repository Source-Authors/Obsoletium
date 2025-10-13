//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds the enumerated list of default cursors
//
// $NoKeywords: $
//=============================================================================//

#ifndef CURSOR_H
#define CURSOR_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/basetypes.h"
#include <vgui/VGUI.h>

namespace vgui
{

enum CursorCode
{
	dc_user,
	dc_none,
	dc_arrow,
	dc_ibeam,
	dc_hourglass,
	dc_waitarrow,
	dc_crosshair,
	dc_up,
	dc_sizenwse,
	dc_sizenesw,
	dc_sizewe,
	dc_sizens,
	dc_sizeall,
	dc_no,
	dc_hand,
	dc_blank, // don't show any custom vgui cursor, just let windows do it stuff (for HTML widget)
	dc_last,
	dc_alwaysvisible_push,
	dc_alwaysvisible_pop,
};

// dimhotepus: unsigned long -> uint32. x86-64
using HCursor = uint32;

}

#endif // CURSOR_H
