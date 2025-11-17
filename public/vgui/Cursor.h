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

[[nodiscard]] constexpr const char* CursorCodeToString( HCursor code )
{
	switch (code)
	{
		case dc_user:
			return "dc_user";
		case dc_none:
			return "dc_none";
		case dc_arrow:
			return "dc_arrow";
		case dc_ibeam:
			return "dc_ibeam";
		case dc_hourglass:
			return "dc_hourglass";
		case dc_waitarrow:
			return "dc_waitarrow";
		case dc_crosshair:
			return "dc_crosshair";
		case dc_up:
			return "dc_up";
		case dc_sizenwse:
			return "dc_sizenwse";
		case dc_sizenesw:
			return "dc_sizenesw";
		case dc_sizewe:
			return "dc_sizewe";
		case dc_sizens:
			return "dc_sizens";
		case dc_sizeall:
			return "dc_sizeall";
		case dc_no:
			return "dc_no";
		case dc_hand:
			return "dc_hand";
		case dc_blank: // don't show any custom vgui cursor: just let windows do it stuff (for HTML widget)
			return "dc_blank";
		case dc_last:
			return "dc_last";
		case dc_alwaysvisible_push:
			return "dc_alwaysvisible_push";
		case dc_alwaysvisible_pop:
			return "dc_alwaysvisible_pop";
		default:
			return "Unknown";
	}
}

}

#endif // CURSOR_H
