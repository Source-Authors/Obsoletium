//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef ISERVERREFRESHRESPONSE_H
#define ISERVERREFRESHRESPONSE_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"

struct serveritem_t;

//-----------------------------------------------------------------------------
// Purpose: Callback interface for server updates
//-----------------------------------------------------------------------------
abstract_class IServerRefreshResponse
{
public:
	// called when the server has successfully responded
	virtual void ServerResponded(serveritem_t &server) = 0;

	// called when a server response has timed out
	virtual void ServerFailedToRespond(serveritem_t &server) = 0;

	// called when the current refresh list is complete
	virtual void RefreshComplete() = 0;
};



#endif // ISERVERREFRESHRESPONSE_H
