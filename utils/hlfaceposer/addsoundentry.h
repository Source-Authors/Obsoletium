//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ADDSOUNDENTRY_H
#define ADDSOUNDENTRY_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/basetypes.h"

FORWARD_DECLARE_HANDLE(HWND);

#include "basedialogparams.h"
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct CAddSoundParams : public CBaseDialogParams
{
	CAddSoundParams()
	{
		m_szWaveFile[ 0 ] = 0;
		m_szSoundName[ 0 ] = 0;
		m_szScriptName[ 0 ] = 0;
		m_bAllowExistingSound = false;
		m_bReadOnlySoundName = false;
	}

	char		m_szWaveFile[ 256 ];

	// i/o input text
	char		m_szSoundName[ 256 ];
	char		m_szScriptName[ 256 ];
	bool		m_bAllowExistingSound;
	bool		m_bReadOnlySoundName;
};

// Display/create dialog
intp AddSound( CAddSoundParams *params, HWND parent );

#endif // ADDSOUNDENTRY_H
