//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef PLAYERPANEL_H
#define PLAYERPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/KeyValues.h"

#include <vgui_controls/Frame.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/PropertyPage.h>

#include "PlayerContextMenu.h"
#include "RemoteServer.h"

//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying information about a game server
//-----------------------------------------------------------------------------
class CPlayerPanel : public vgui::PropertyPage, public IServerDataResponse
{
	DECLARE_CLASS_SIMPLE( CPlayerPanel, vgui::PropertyPage );
public:
	CPlayerPanel(vgui::Panel *parent, const char *name);
	virtual ~CPlayerPanel();

	// returns the keyvalues for the currently selected row
	KeyValues *GetSelected(); 

protected:
	// property page handlers
	void OnResetData() override;
	void OnThink() override;
	void OnKeyCodeTyped(vgui::KeyCode code) override;

	// called when the server has returned a requested value
	void OnServerDataResponse(const char *value, const char *response) override;

private:
	// vgui overrides
	void OnCommand(const char *command) override;

	// msg handlers
	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );
	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );

	MESSAGE_FUNC( OnKickButtonPressed, "KickPlayer" );
	MESSAGE_FUNC( OnBanButtonPressed, "BanPlayer" );
	MESSAGE_FUNC( KickSelectedPlayers, "KickSelectedPlayers" );
	MESSAGE_FUNC_CHARPTR_CHARPTR( AddBanByID, "AddBanValue", id, time );

	vgui::ListPanel *m_pPlayerListPanel;
	vgui::Button *m_pKickButton;
	vgui::Button *m_pBanButton;

	CPlayerContextMenu *m_pPlayerContextMenu;

	float m_flUpdateTime;
};

#endif // PLAYERPANEL_H