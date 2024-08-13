//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Matchmaking's "main menu"
//
//=============================================================================//

#ifndef WELCOMEDIALOG_H
#define WELCOMEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialog.h"

//-----------------------------------------------------------------------------
// Purpose: Main matchmaking menu
//-----------------------------------------------------------------------------
class CWelcomeDialog : public CBaseDialog
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CWelcomeDialog, CBaseDialog ); 

public:
	CWelcomeDialog(vgui::Panel *parent);

	void	PerformLayout( void ) override;
	void	OnCommand( const char *pCommand ) override;
	void	OnKeyCodePressed( vgui::KeyCode code ) override;

private:
	bool	m_bOnlineEnabled;
};


#endif // WELCOMEDIALOG_H
