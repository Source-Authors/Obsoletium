//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SAVEGAMEDIALOG_H
#define SAVEGAMEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "BaseSaveGameDialog.h"
#include "SaveGameBrowserDialog.h"
#include "vgui_controls/KeyRepeat.h"

//-----------------------------------------------------------------------------
// Purpose: Save game dialog
//-----------------------------------------------------------------------------
class CSaveGameDialog : public CBaseSaveGameDialog
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CSaveGameDialog, CBaseSaveGameDialog );

public:
	CSaveGameDialog( vgui::Panel *parent );
	~CSaveGameDialog();

	void Activate() override;
	static void FindSaveSlot( char *buffer, int bufsize );

protected:
	void OnCommand( const char *command ) override;
	void OnScanningSaveGames() override;
};

#define SAVE_NUM_ITEMS 4

//
//
//
class CAsyncCtxSaveGame;

class CSaveGameDialogXbox : public CSaveGameBrowserDialog
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CSaveGameDialogXbox, CSaveGameBrowserDialog );

public:
					CSaveGameDialogXbox( vgui::Panel *parent );
	void	PerformSelectedAction( void ) override;
	void	UpdateFooterOptions( void ) override;
	void	OnCommand( const char *command ) override;
	void	OnDoneScanningSaveGames( void ) override;

private:
	friend class CAsyncCtxSaveGame;
	void			InitiateSaving();
	void			SaveCompleted( CAsyncCtxSaveGame *pCtx );

private:
	bool					m_bGameSaving;
	bool					m_bNewSaveAvailable;
	SaveGameDescription_t	m_NewSaveDesc;
};

#endif // SAVEGAMEDIALOG_H
