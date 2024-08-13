//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Dialog for setting customizable game options
//
//=============================================================================//

#ifndef SESSIONOPTIONSDIALOG_H
#define SESSIONOPTIONSDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialog.h"

//-----------------------------------------------------------------------------
// Purpose: Simple menu to choose a matchmaking session type
//-----------------------------------------------------------------------------
class CSessionOptionsDialog : public CBaseDialog
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CSessionOptionsDialog, CBaseDialog ); 

public:
	CSessionOptionsDialog( vgui::Panel *parent );
	~CSessionOptionsDialog();

	void	OnCommand( const char *pCommand ) override;
	void	OnKeyCodePressed( vgui::KeyCode code ) override;
	void	OnThink() override;

	void	PerformLayout( void ) override;
	void	ApplySettings( KeyValues *pResourceData ) override;
	void	ApplySchemeSettings( vgui::IScheme *pScheme ) override;

	void	OverrideMenuItem( KeyValues *pKeys ) override;

	void			SetGameType( const char *pGametype );
	void			SetDialogKeys( KeyValues *pKeys );
	void			ApplyCommonProperties( KeyValues *pKeys );

	MESSAGE_FUNC_PARAMS( OnMenuItemChanged, "MenuItemChanged", data );

private:
	void			SetupSession( void );
	void			UpdateScenarioDisplay( void );

	int				GetMaxPlayersRecommendedOption( void );

	char			m_szCommand[MAX_COMMAND_LEN];
	char			m_szGametype[MAX_COMMAND_LEN];
	bool			m_bModifySession;

	KeyValues		*m_pDialogKeys;

	CUtlVector< sessionProperty_t >		m_SessionProperties;
	CUtlVector< CScenarioInfoPanel* >	m_pScenarioInfos;

	vgui::Label		*m_pRecommendedLabel;
};


#endif // SESSIONOPTIONSDIALOG_H
