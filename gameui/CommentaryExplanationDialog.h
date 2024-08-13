//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef COMMENTARYEXPLANATIONDIALOG_H
#define COMMENTARYEXPLANATIONDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "utlvector.h"
#include <vgui/KeyCode.h>

//-----------------------------------------------------------------------------
// Purpose: Dialog that explains the commentary mode
//-----------------------------------------------------------------------------
class CCommentaryExplanationDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CCommentaryExplanationDialog, vgui::Frame );

public:
	CCommentaryExplanationDialog(vgui::Panel *parent, char *pszFinishCommand);
	~CCommentaryExplanationDialog();

	void OnKeyCodeTyped(vgui::KeyCode code) override;
	void OnKeyCodePressed(vgui::KeyCode code) override;
	void OnCommand( const char *command ) override;
	void OnClose( void ) override;

private:
	char m_pszFinishCommand[ 512 ];
};

#endif // COMMENTARYEXPLANATIONDIALOG_H
