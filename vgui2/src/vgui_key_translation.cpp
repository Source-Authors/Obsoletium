//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "vgui_key_translation.h"

#if defined( WIN32 )
#define NOMINMAX
#include <wtypes.h>
#include <WinUser.h>
#endif

#include "tier0/dbg.h"

#ifdef POSIX
#define VK_RETURN -1
#endif

#include "tier2/tier2.h"
#include "inputsystem/iinputsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

vgui::KeyCode KeyCode_VirtualKeyToVGUI( int key )
{
	// Some tools load vgui for localization and never use input
	if ( !g_pInputSystem )
		return KEY_NONE;
	return g_pInputSystem->VirtualKeyToButtonCode( key );
}

int KeyCode_VGUIToVirtualKey( vgui::KeyCode code )
{
	// Some tools load vgui for localization and never use input
	if ( !g_pInputSystem )
		return VK_RETURN;

	return g_pInputSystem->ButtonCodeToVirtualKey( code );
}
