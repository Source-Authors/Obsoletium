//========= Copyright Valve Corporation, All rights reserved. ============//
//
//=======================================================================================//

#ifndef IDOWNLOADSYSTEM_H
#define IDOWNLOADSYSTEM_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------

#include "interface.h"
#include "platform.h"

//----------------------------------------------------------------------------------------

#define INTERFACEVERSION_DOWNLOADSYSTEM		"DownloadSystem001"

//----------------------------------------------------------------------------------------

struct RequestContext_t;

//----------------------------------------------------------------------------------------

class IDownloadSystem : public IBaseInterface
{
public:
	virtual unsigned long CreateDownloadThread( RequestContext_t *pContext ) = 0;
};

//----------------------------------------------------------------------------------------

#endif // IDOWNLOADSYSTEM_H