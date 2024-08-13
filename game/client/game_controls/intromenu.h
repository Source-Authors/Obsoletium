//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef INTROMENU_H
#define INTROMENU_H
#ifdef _WIN32
#pragma once
#endif


#include <vgui_controls/Frame.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Label.h>

#include <game/client/iviewport.h>

namespace vgui
{
	class TextEntry;
}

class CIntroMenu : public vgui::Frame, public IViewPortPanel
{
private:
	DECLARE_CLASS_SIMPLE_OVERRIDE( CIntroMenu, vgui::Frame );

public:
	CIntroMenu( IViewPort *pViewPort );
	virtual ~CIntroMenu();

	void ApplySchemeSettings( vgui::IScheme *pScheme ) override;

	const char *GetName( void ) override { return PANEL_INTRO; }
	void SetData( KeyValues *data ) override{ return; }
	void Reset() override;
	void Update() override;
	bool NeedsUpdate( void ) override { return false; }
	bool HasInputElements( void ) override { return true; }
	void ShowPanel( bool bShow ) override;

	// both vgui::Frame and IViewPortPanel define these, so explicitly define them here as passthroughs to vgui
	vgui::VPANEL GetVPanel( void ) override { return BaseClass::GetVPanel(); }
  	bool IsVisible() override { return BaseClass::IsVisible(); }
  	void SetParent( vgui::VPANEL parent ) override { BaseClass::SetParent( parent ); }

	GameActionSet_t GetPreferredActionSet() override { return GAME_ACTION_SET_IN_GAME_HUD; }

protected:
	// vgui overrides
	void OnCommand( const char *command ) override;

	IViewPort		*m_pViewPort;
	vgui::Label		*m_pTitleLabel;
};

#endif // INTROMENU_H
