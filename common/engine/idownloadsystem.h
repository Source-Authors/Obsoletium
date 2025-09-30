//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef IDOWNLOADSYSTEM_H
#define IDOWNLOADSYSTEM_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "tier1/interface.h"
#include "tier0/threadtools.h"

//----------------------------------------------------------------------------------------

#define INTERFACEVERSION_DOWNLOADSYSTEM		"DownloadSystem001"

//----------------------------------------------------------------------------------------

struct RequestContext_t;

//----------------------------------------------------------------------------------------

class IDownloadSystem : public IBaseInterface
{
public:
	// dimhotepus: unsigned long -> ThreadId_t.
	virtual ThreadId_t CreateDownloadThread( RequestContext_t *pContext ) = 0;
};

//----------------------------------------------------------------------------------------

#endif // IDOWNLOADSYSTEM_H