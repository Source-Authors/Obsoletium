//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: linux dependant ASM code for CPU capability detection
//
// $Workfile:     $
// $NoKeywords: $
//============================================================================//

#include "tier0/platform.h"

bool CheckMMXTechnology()
{
	// dimhotepus: Use portable CPU info.
	return GetCPUInformation()->m_bMMX;
}

bool CheckSSETechnology()
{
	// dimhotepus: Use portable CPU info.
	return GetCPUInformation()->m_bSSE;
}

bool CheckSSE2Technology()
{
	// dimhotepus: Use portable CPU info.
	return GetCPUInformation()->m_bSSE2;
}

bool Check3DNowTechnology()
{
	// dimhotepus: Use portable CPU info.
	return GetCPUInformation()->m_b3DNow;
}
