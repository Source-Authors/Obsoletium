//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "host_phonehome.h"
#include "tier0/icommandline.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// FIXME, this is BS
bool IsExternalBuild()
{
	if (CommandLine()->FindParm("-publicbuild"))
	{
		return true;
	}

	if (!CommandLine()->FindParm("-internalbuild") &&
		!CommandLine()->CheckParm("-dev"))
	{
		return true;
	}

	return false;
}
