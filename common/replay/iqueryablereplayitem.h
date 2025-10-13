//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef IQUERYABLEREPLAYITEM_H
#define IQUERYABLEREPLAYITEM_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "interface.h"
#include "iqueryablereplayitem.h"
#include "replay/replayhandle.h"
#include "replay/replaytime.h"

//----------------------------------------------------------------------------------------

class CReplay;

//----------------------------------------------------------------------------------------

using QueryableReplayItemHandle_t = int;

//----------------------------------------------------------------------------------------

abstract_class IQueryableReplayItem : public IBaseInterface
{
public:
	[[nodiscard]] virtual const CReplayTime	&GetItemDate() const = 0;
	[[nodiscard]] virtual bool				IsItemRendered() const = 0;
	virtual CReplay				*GetItemReplay() = 0;
	[[nodiscard]] virtual ReplayHandle_t		GetItemReplayHandle() const = 0;
	[[nodiscard]] virtual QueryableReplayItemHandle_t	GetItemHandle() const = 0;	// Get the handle of this item
	[[nodiscard]] virtual const wchar_t		*GetItemTitle() const = 0;
	virtual void				SetItemTitle( const wchar_t *pTitle ) = 0;
	[[nodiscard]] virtual float				GetItemLength() const = 0;
	virtual void				*GetUserData() = 0;
	virtual void				SetUserData( void *pUserData ) = 0;
	[[nodiscard]] virtual bool				IsItemAMovie() const = 0;
};

//----------------------------------------------------------------------------------------

#endif // IQUERYABLEREPLAYITEM_H