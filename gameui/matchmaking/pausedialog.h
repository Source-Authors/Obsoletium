//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Multiplayer pause menu
//
//=============================================================================//

#ifndef PAUSEDIALOG_H
#define PAUSEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialog.h"

//-----------------------------------------------------------------------------
// Purpose: Multiplayer pause menu
//-----------------------------------------------------------------------------
class CPauseDialog : public CBaseDialog
{
	DECLARE_CLASS_SIMPLE_OVERRIDE( CPauseDialog, CBaseDialog ); 

public:
	CPauseDialog( vgui::Panel *parent );

	void Activate( void ) override;
	void OnKeyCodePressed( vgui::KeyCode code ) override;
};


#endif // PAUSEDIALOG_H
