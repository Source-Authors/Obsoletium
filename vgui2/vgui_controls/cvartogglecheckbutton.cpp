//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <vgui_controls/cvartogglecheckbutton.h>

#include "tier1/KeyValues.h"

#include <vgui/ISurface.h>
#include <vgui/IScheme.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

static vgui::Panel *Create_CvarToggleCheckButton()
{
	return new CvarToggleCheckButton< ConVarRef >( NULL, NULL );
}

DECLARE_BUILD_FACTORY_CUSTOM_ALIAS( CvarToggleCheckButton<ConVarRef>, CvarToggleCheckButton, Create_CvarToggleCheckButton );

