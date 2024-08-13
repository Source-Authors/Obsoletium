//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MULTIPLAYERADVANCEDDIALOG_H
#define MULTIPLAYERADVANCEDDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include "scriptobject.h"
#include <vgui/KeyCode.h>

class CPanelListPanel;

//-----------------------------------------------------------------------------
// Purpose: Displays a game-specific list of options
//-----------------------------------------------------------------------------
class CMultiplayerAdvancedDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CMultiplayerAdvancedDialog, vgui::Frame ); 

public:
	CMultiplayerAdvancedDialog(vgui::Panel *parent);
	~CMultiplayerAdvancedDialog();

	void Activate() override;

private:

	void CreateControls();
	void DestroyControls();
	void GatherCurrentValues();
	void SaveValues();

	CInfoDescription *m_pDescription;

	mpcontrol_t *m_pList;

	CPanelListPanel *m_pListPanel;

	void OnCommand( const char *command ) override;
	void OnClose() override;
	void OnKeyCodeTyped(vgui::KeyCode code) override;
};


#endif // MULTIPLAYERADVANCEDDIALOG_H
