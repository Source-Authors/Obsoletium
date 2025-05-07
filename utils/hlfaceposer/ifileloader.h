//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef IFILELOADER_H
#define IFILELOADER_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"
#include "tier0/platform.h"

class CWaveFile;

abstract_class IFileLoader
{
public:
	virtual void			AddWaveFilesToThread( CUtlVector< CWaveFile * >& wavefiles ) = 0;

	virtual intp			ProcessCompleted() = 0;

	virtual	void			Start() = 0;

	virtual int				GetPendingLoadCount() = 0;
};

extern IFileLoader *fileloader;

#endif // IFILELOADER_H
