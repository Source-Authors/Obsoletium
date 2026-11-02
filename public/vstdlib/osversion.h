//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef OSVERSION_H
#define OSVERSION_H
#pragma once

#include "vstdlib/vstdlib.h"

// Values are ordered by minimum-OS capability within each OS family.
enum EOSType
{
	k_eOSUnknown = -1,

	// macOS
	k_eMacOSUnknown = -102,
	k_eMacOS104 = -101,
	k_eMacOS105 = -100,
	k_eMacOS1058 = -99,
	k_eMacOS106 = -95,
	k_eMacOS1063 = -94,
	k_eMacOS107 = -90,
	k_eMacOS108 = -89,
	k_eMacOS109 = -88,
	k_eMacOS1010 = -87,
	k_eMacOS1011 = -86,
	k_eMacOS1012 = -85,
	k_eMacOS1013 = -84,
	k_eMacOS1014 = -83,
	k_eMacOS1015 = -82,
	k_eMacOS11 = -81,
	k_eMacOS12 = -80,
	k_eMacOS13 = -79,
	k_eMacOS14 = -78,
	k_eMacOS15 = -77,

	// Linux
	k_eLinuxUnknown = -203,
	k_eLinux22 = -202,
	k_eLinux24 = -201,
	k_eLinux26 = -200,
	k_eLinux3OrGreater = -199,

	// Windows
	k_eWinUnknown = 0,
	k_eWin311 = 1,
	k_eWin95,
	k_eWin98,
	k_eWinME,
	k_eWinNT,
	k_eWin2000,
	k_eWinXP,
	k_eWin2003,
	k_eWinVista,
	k_eWindows7,
	k_eWin2008,
	k_eWindows8,
	k_eWindows8_1,
	k_eWindows10,
	k_eWindows11,

	k_eWinMAX,
	// 19 macOS + 5 Linux + 16 Windows entries
	k_eOSTypeMax = 19 + 5 + k_eWinMAX
};

VSTDLIB_INTERFACE const char *GetNameFromOSType(EOSType eOSType);
VSTDLIB_INTERFACE const char *GetOSDetailString(char *pchOutBuf, int cchOutBuf);
VSTDLIB_INTERFACE EOSType GetOSType();
VSTDLIB_INTERFACE bool OSTypesAreCompatible(EOSType eOSTypeDetected, EOSType eOSTypeRequired);
VSTDLIB_INTERFACE const char *GetPlatformName(bool *pbIs64Bit);

#endif // OSVERSION_H
