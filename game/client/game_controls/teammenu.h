//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TEAMMENU_H
#define TEAMMENU_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/Button.h>

#include <game/client/iviewport.h>

#include <vgui/KeyCode.h>
#include <utlvector.h>

namespace vgui
{
	class RichText;
	class HTML;
}
class TeamFortressViewport;


//-----------------------------------------------------------------------------
// Purpose: Displays the team menu
//-----------------------------------------------------------------------------
class CTeamMenu : public vgui::Frame, public IViewPortPanel
{
private:
	DECLARE_CLASS_SIMPLE_OVERRIDE( CTeamMenu, vgui::Frame );

public:
	CTeamMenu(IViewPort *pViewPort);
	virtual ~CTeamMenu();

	const char *GetName( void ) override { return PANEL_TEAM; }
	void SetData(KeyValues *) override {};
	void Reset() override {};
	void Update() override;
	bool NeedsUpdate( void ) override { return false; }
	bool HasInputElements( void ) override { return true; }
	void ShowPanel( bool bShow ) override;

	// both vgui::Frame and IViewPortPanel define these, so explicitly define them here as passthroughs to vgui
	vgui::VPANEL GetVPanel( void ) override { return BaseClass::GetVPanel(); }
  	bool IsVisible() override { return BaseClass::IsVisible(); }
	void SetParent( vgui::VPANEL parent ) override { BaseClass::SetParent( parent ); }

	GameActionSet_t GetPreferredActionSet() override { return GAME_ACTION_SET_IN_GAME_HUD; }

public:
	
	void AutoAssign();
	
protected:

	// int GetNumTeams() { return m_iNumTeams; }
	
	// VGUI2 overrides
	void ApplySchemeSettings(vgui::IScheme *pScheme) override;
	void OnKeyCodePressed(vgui::KeyCode code) override;

	// helper functions
	virtual void SetLabelText(const char *textEntryName, const char *text);
	virtual void LoadMapPage( const char *mapName );
	// virtual void MakeTeamButtons( void );
	
	// command callbacks
	// MESSAGE_FUNC_INT( OnTeamButton, "TeamButton", team );

	IViewPort	*m_pViewPort;
	vgui::RichText *m_pMapInfo;
	vgui::HTML *m_pMapInfoHTML;
//	int m_iNumTeams;
	ButtonCode_t m_iJumpKey;
	ButtonCode_t m_iScoreBoardKey;

	char m_szMapName[ MAX_PATH ];
};


#endif // TEAMMENU_H
