//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef GAMEPANELINFO_H
#define GAMEPANELINFO_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/KeyValues.h"

#include <vgui_controls/Frame.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/AnimatingImagePanel.h>
#include <vgui_controls/Image.h>
#include <vgui_controls/PropertyPage.h>

#undef PropertySheet

#include <vgui_controls/PropertySheet.h>

#include "BanContextMenu.h"
#include "chatpanel.h"
#include "rawlogpanel.h"
#include "serverconfigpanel.h"
#include "playerpanel.h"
#include "banpanel.h"
#include "graphpanel.h"
#include "serverinfopanel.h"

#define PASSWORD_LEN 64
#define MOD_LEN 64

#include "imanageserver.h" // IManageServer interface


class CBudgetPanelContainer;

//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying information about a game server
//-----------------------------------------------------------------------------
class CGamePanelInfo : public vgui::Frame, public IManageServer
{
	DECLARE_CLASS_SIMPLE( CGamePanelInfo, vgui::Frame ); 
public:
	CGamePanelInfo(vgui::Panel *parent, const char *name, const char *mod);
	virtual ~CGamePanelInfo();

	// IManageServer interface extras
	void ShowPage() { Activate(); }
	void AddToConsole(const char *msg);
	void SetAsRemoteServer(bool remote);

protected:
	// message handlers
	void OnStop();
	MESSAGE_FUNC( OnHelp, "Help" );
	void OnMasterRequestRestart();
	void OnMasterOutOfDate( const char *msg);
	MESSAGE_FUNC( OnRestartServer, "RestartServer" );
	MESSAGE_FUNC( OnUpdateTitle, "UpdateTitle" );
	void SetNewTitle(bool connectionFailed, const char *additional_text); // sets the windows title

	// vgui overrides
	void OnTick() override;
	void OnClose() override;
	void PerformLayout() override;
	void ActivateBuildMode() override;
	void OnCommand(const char *command) override;

private:
	// methods
	vgui::ComboBox *m_pViewCombo;
	vgui::AnimatingImagePanel *m_pAnimImagePanel;
	
	// GUI pabels
	// main property sheet
	vgui::PropertySheet *m_pDetailsSheet;

	// panels in the sheet
	CPlayerPanel *m_pPlayerListPanel;
	CBanPanel *m_pBanListPanel;
	CRawLogPanel *m_pServerLogPanel;
	CChatPanel *m_pServerChatPanel;
	CServerConfigPanel *m_pServerConfigPanel;
	CGraphPanel *m_pGraphsPanel;
	CServerInfoPanel *m_pServerInfoPanel;
	CBudgetPanelContainer *m_pBudgetPanel;

	// state
	bool m_bRemoteServer;
	bool m_bShuttingDown;

	vgui::DHANDLE<vgui::QueryBox> m_hRestartQueryBox;
	vgui::DHANDLE<vgui::QueryBox> m_hOutOfDateQueryBox;
};

#endif // GAMEPANELINFO_H
