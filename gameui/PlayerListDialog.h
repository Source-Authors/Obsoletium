//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PLAYERLISTDIALOG_H
#define PLAYERLISTDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include "steam/steamclientpublic.h"

//-----------------------------------------------------------------------------
// Purpose: List of players, their ingame-name and their friends-name
//-----------------------------------------------------------------------------
class CPlayerListDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CPlayerListDialog, vgui::Frame ); 

public:
	CPlayerListDialog(vgui::Panel *parent);
	~CPlayerListDialog();

	void Activate() override;

private:
  	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );
	void OnCommand(const char *command) override;

	void ToggleMuteStateOfSelectedUser();
	void RefreshPlayerProperties();

	void OnKeyCodePressed( vgui::KeyCode code ) override
	{
		if ( code == KEY_XBUTTON_B )
		{
			Close();
		}
		else
		{
			BaseClass::OnKeyCodePressed(code);
		}
	}

	vgui::ListPanel *m_pPlayerList;
	vgui::Button *m_pMuteButton;
};

#endif // PLAYERLISTDIALOG_H
