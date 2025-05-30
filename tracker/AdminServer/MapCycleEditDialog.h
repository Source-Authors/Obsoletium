//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef MAPCYCLEEDITDIALOG_H
#define MAPCYCLEEDITDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include "tier1/utlvector.h"
#include "tier1/utlsymbol.h"

//-----------------------------------------------------------------------------
// Purpose: Dialog for editing the game server map cycle list
//-----------------------------------------------------------------------------
class CMapCycleEditDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CMapCycleEditDialog, vgui::Frame ); 
public:
	CMapCycleEditDialog(vgui::Panel *parent, const char *name);
	virtual ~CMapCycleEditDialog();

	virtual void Activate(vgui::Panel *updateTarget, CUtlVector<CUtlSymbol> &availableMaps, CUtlVector<CUtlSymbol> &mapCycle);

protected:
	void OnCommand(const char *command) override;
	void PerformLayout() override;

private:
	MESSAGE_FUNC_PTR( OnItemSelected, "ItemSelected", panel );

	vgui::ListPanel *m_pAvailableMapList;
	vgui::ListPanel *m_pMapCycleList;
	vgui::Button *m_RightArrow;
	vgui::Button *m_LeftArrow;
	vgui::Button *m_UpArrow;
	vgui::Button *m_DownArrow;
};


#endif // MAPCYCLEEDITDIALOG_H
