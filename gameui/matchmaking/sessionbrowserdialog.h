//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Present a list of sessions from which the player can choose a game to join.
//
//=====================================================================================//

#ifndef SESSIONBROWSERDIALOG_H
#define SESSIONBROWSERDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialog.h"

class CSessionBrowserDialog : public CBaseDialog
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CSessionBrowserDialog, CBaseDialog ); 

public:
	CSessionBrowserDialog( vgui::Panel *parent, KeyValues *pDialogKeys );
	~CSessionBrowserDialog();

	void	PerformLayout() override;
	void	ApplySettings( KeyValues *inResourceData ) override;
	void	ApplySchemeSettings( vgui::IScheme *pScheme ) override;
	void	OnKeyCodePressed( vgui::KeyCode code ) override;
	void	OnCommand( const char *pCommand ) override;
	void	OnThink() override;

	void	SwapMenuItems( int iOne, int iTwo ) override;

	void			UpdateScenarioDisplay( void );
	void			SessionSearchResult( int searchIdx, void *pHostData, XSESSION_SEARCHRESULT *pResult, int ping );

	KeyValues	*m_pDialogKeys;

	CUtlVector< CScenarioInfoPanel* >	m_pScenarioInfos;
	CUtlVector< int >					m_ScenarioIndices;
	CUtlVector< int >					m_SearchIndices;
	CUtlVector< int >					m_GameStates;
	CUtlVector< int >					m_GameTimes;
	CUtlVector< XUID >					m_XUIDs;
};


#endif // SESSIONBROWSERDIALOG_H
