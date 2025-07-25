//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef LOADINGDIALOG_H
#define LOADINGDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>
#include <vgui_controls/HTML.h>

//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying level loading status
//-----------------------------------------------------------------------------
class CLoadingDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CLoadingDialog, vgui::Frame ); 
public:
	CLoadingDialog( vgui::Panel *parent );
	~CLoadingDialog();

	void Open();
	bool SetProgressPoint(float fraction);
	void SetStatusText(const char *statusText);
	void SetSecondaryProgress(float progress);
	void SetSecondaryProgressText(const char *statusText);
	bool SetShowProgressText( bool show );

	void DisplayGenericError(const char *failureReason, const char *extendedReason = NULL);
	void DisplayVACBannedError();
	void DisplayNoSteamConnectionError();
	void DisplayLoggedInElsewhereError();

protected:
	void OnCommand(const char *command) override;
	void PerformLayout() override;
	void OnThink() override;
	void OnClose() override;
	void OnKeyCodeTyped(vgui::KeyCode code) override;
	void OnKeyCodePressed(vgui::KeyCode code) override;
	
private:
	void SetupControlSettings( bool bForceShowProgressText );
	void SetupControlSettingsForErrorDisplay( const char *settingsFile );
	void HideOtherDialogs( bool bHide );

	vgui::ProgressBar	*m_pProgress;
	vgui::ProgressBar	*m_pProgress2;
	vgui::Label			*m_pInfoLabel;
	vgui::Label			*m_pTimeRemainingLabel;
	vgui::Button		*m_pCancelButton;
	vgui::Panel			*m_pLoadingBackground;

	bool	m_bShowingSecondaryProgress;
	float	m_flSecondaryProgress;
	// dimhotepus: float -> double.
	double	m_flLastSecondaryProgressUpdateTime;
	// dimhotepus: float -> double.
	double	m_flSecondaryProgressStartTime;
	bool	m_bShowingVACInfo;
	bool	m_bCenter;
	float	m_flProgressFraction;	

	CPanelAnimationVar( int, m_iAdditionalIndentX, "AdditionalIndentX", "0" );
	CPanelAnimationVar( int, m_iAdditionalIndentY, "AdditionalIndentY", "0" );
};

// singleton accessor
CLoadingDialog *LoadingDialog();


#endif // LOADINGDIALOG_H
