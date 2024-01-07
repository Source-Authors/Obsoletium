//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: win32 dependant ASM code for CPU capability detection
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//

#include "tier0/platform.h"

#if defined( _X360 )

bool CheckMMXTechnology(void) { return false; }
bool CheckSSETechnology(void) { return false; }
bool CheckSSE2Technology(void) { return false; }
bool Check3DNowTechnology(void) { return false; }

#elif defined( _WIN32 ) && !defined( _X360 )

bool CheckMMXTechnology(void)
{
	auto *info = GetCPUInformation();
	return info->m_bMMX;
}

bool CheckSSETechnology(void)
{
	auto *info = GetCPUInformation();
	return info->m_bSSE;
}

bool CheckSSE2Technology(void)
{
	auto *info = GetCPUInformation();
	return info->m_bSSE2;
}

bool Check3DNowTechnology(void)
{
	auto *info = GetCPUInformation();
	return info->m_b3DNow;
}

#endif // _WIN32
