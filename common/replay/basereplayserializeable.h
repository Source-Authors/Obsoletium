//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef REPLAYSERIALIIZEABLE_H
#define REPLAYSERIALIIZEABLE_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "replay/ireplayserializeable.h"
#include "replay/replayhandle.h"

//----------------------------------------------------------------------------------------

class CBaseReplaySerializeable : public IReplaySerializeable
{
public:
	CBaseReplaySerializeable();

	void			SetHandle( ReplayHandle_t h ) override;
	[[nodiscard]] ReplayHandle_t	GetHandle() const override;
	bool			Read( KeyValues *pIn ) override;
	void			Write( KeyValues *pOut ) override;
	[[nodiscard]] const char		*GetFilename() const override;
	[[nodiscard]] const char		*GetFullFilename() const override;
	[[nodiscard]] const char		*GetDebugName() const override;
	void			SetLocked( bool bLocked ) override;
	[[nodiscard]] bool			IsLocked() const override;
	void			OnDelete() override;
	void			OnUnload() override;
	virtual void			OnAddedToDirtyList();

private:
	ReplayHandle_t			m_hThis;
	bool					m_bLocked;
};

//----------------------------------------------------------------------------------------

#endif // REPLAYSERIALIIZEABLE_H