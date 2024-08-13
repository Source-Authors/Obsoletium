//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Displays a leaderboard
//
//=============================================================================//

#ifndef LEADERBOARDDIALOG_H
#define LEADERBOARDDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialog.h"

//-----------------------------------------------------------------------------
// Purpose: Display player leaderboards
//-----------------------------------------------------------------------------
class CLeaderboardDialog : public CBaseDialog
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CLeaderboardDialog, CBaseDialog ); 

public:
	CLeaderboardDialog(vgui::Panel *parent);
	~CLeaderboardDialog();

	void	ApplySchemeSettings( vgui::IScheme *pScheme ) override;
	void	ApplySettings( KeyValues *pResourceData ) override;
	void	PerformLayout( void ) override;
	void	OnCommand( const char *pCommand ) override;
	void	OnKeyCodePressed( vgui::KeyCode code ) override;
	void	HandleKeyRepeated( vgui::KeyCode code ) override;

	bool			GetPlayerStats( int rank, bool bFriends = false );
	void			UpdateLeaderboard( int iNewRank );
	void			AddLeaderboardEntry( const char **pEntries, int ct );
	void			CleanupStats();

private:
	vgui::Panel		*m_pProgressBg;
	vgui::Panel		*m_pProgressBar;
	vgui::Label		*m_pProgressPercent;
	vgui::Label		*m_pNumbering;
	vgui::Label		*m_pUpArrow;
	vgui::Label		*m_pDownArrow;
	vgui::Label		*m_pBestMoments;

	int			m_iBaseRank;
	int			m_iActiveRank;
	int			m_iMaxRank;
	int			m_cColumns;
	int			m_iRangeBase;

#if defined( _X360 )
	XUSER_STATS_READ_RESULTS *m_pStats;
#endif
};


#endif // LEADERBOARDDIALOG_H
