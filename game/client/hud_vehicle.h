//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HUD_CROSSHAIR_H
#define HUD_CROSSHAIR_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/Panel.h>

namespace vgui
{
	class IScheme;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudVehicle : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CHudVehicle, vgui::Panel );
public:
	CHudVehicle( const char *pElementName );

	bool	ShouldDraw() override;

	void	ApplySchemeSettings( vgui::IScheme *scheme ) override;
	void	Paint( void ) override;

private:

	IClientVehicle	*GetLocalPlayerVehicle();
};

#endif // HUD_CROSSHAIR_H
