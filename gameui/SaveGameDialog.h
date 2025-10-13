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
	static void FindSaveSlot( OUT_Z_CAP(bufsize) char *buffer, intp bufsize );
	template<intp size>
	static void FindSaveSlot(OUT_Z_ARRAY char (&buffer)[size])
	{
		FindSaveSlot( buffer, size );
	}

protected:
	void OnCommand( const char *command ) override;
	void OnScanningSaveGames() override;
};

#endif // SAVEGAMEDIALOG_H
