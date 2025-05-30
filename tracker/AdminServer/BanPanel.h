//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef BANPANEL_H
#define BANPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/KeyValues.h"

#include <vgui_controls/Frame.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/PropertyPage.h>
#include "tier1/utlvector.h"

#include "BanContextMenu.h"
#include "RemoteServer.h"

//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying information about a game server
//-----------------------------------------------------------------------------
class CBanPanel : public vgui::PropertyPage, public IServerDataResponse
{
	DECLARE_CLASS_SIMPLE( CBanPanel, vgui::PropertyPage );
public:
	CBanPanel(vgui::Panel *parent, const char *name);
	virtual ~CBanPanel();

	void OnResetData() override;

protected:
	// property page handlers
	void OnPageShow() override;

	void OnThink() override;

	// server response on user data
	void OnServerDataResponse(const char *value, const char *response) override;
	void OnKeyCodeTyped(vgui::KeyCode code) override;

private:
	MESSAGE_FUNC( AddBan, "addban" );
	MESSAGE_FUNC( RemoveBan, "removeban" );
	MESSAGE_FUNC( ChangeBan, "changeban" );

	MESSAGE_FUNC_CHARPTR( RemoveBanByID, "removebanbyid", id );
	MESSAGE_FUNC_CHARPTR_CHARPTR( ChangeBanTimeByID, "AddBanValue", id, time );
	MESSAGE_FUNC_PARAMS( OnCvarChangeValue, "CvarChangeValue", kv );

	// returns true if the id string is an IP address, false if it's a WON or STEAM ID
	[[nodiscard]] static bool IsIPAddress(const char *id);

	// msg handlers
	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );
	void OnEffectPlayer(KeyValues *data);
	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );

	MESSAGE_FUNC( ImportBanList, "importban" );
	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );

	vgui::ListPanel *m_pBanListPanel;
	vgui::Button *m_pAddButton;
	vgui::Button *m_pRemoveButton;
	vgui::Button *m_pChangeButton;
	vgui::Button *m_pImportButton;

	CBanContextMenu *m_pBanContextMenu;
	// dimhotepus: float -> double.
	double m_flUpdateTime;
	bool m_bPageViewed;
};

#endif // BANPANEL_H