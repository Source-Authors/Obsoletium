//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef BUDGETPANELCONTAINER_H
#define BUDGETPANELCONTAINER_H
#ifdef _WIN32
#pragma once
#endif


#include "tier1/KeyValues.h"

#include <vgui_controls/Frame.h>
#include <vgui_controls/PHandle.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/PropertyPage.h>
#include "tier1/utlvector.h"
#include "RemoteServer.h"


class CBudgetPanelAdmin;


//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying information about a game server
//-----------------------------------------------------------------------------
class CBudgetPanelContainer : public vgui::PropertyPage, public IServerDataResponse
{
	DECLARE_CLASS_SIMPLE( CBudgetPanelContainer, vgui::PropertyPage );
public:
	CBudgetPanelContainer(vgui::Panel *parent, const char *name);
	virtual ~CBudgetPanelContainer();


// Panel overrides.
public:
	void Paint() override;
	void PerformLayout() override;

	// PropertyPage overrides.
	void OnPageShow() override;
	void OnPageHide() override;

	// IServerDataResponse overrides.
	void OnServerDataResponse(const char *value, const char *response) override;

private:
	CBudgetPanelAdmin *m_pBudgetPanelAdmin;
};


#endif // BUDGETPANELCONTAINER_H
