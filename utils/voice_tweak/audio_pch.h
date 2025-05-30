// Copyright Valve Corporation, All rights reserved.

#ifndef SE_UTILS_VOICE_TWEAK_AUDIO_PCH_H_
#define SE_UTILS_VOICE_TWEAK_AUDIO_PCH_H_

#include "tier0/platform.h"

#define VC_EXTRALEAN  // Exclude rarely-used stuff from Windows headers
#define STRICT

// Access to Windows 10 features.
#define _WIN32_WINNT 0x0A00

#include <afxwin.h>    // MFC core and standard components
#include <afxext.h>    // MFC extensions
#include <afxdtctl.h>  // MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>  // MFC support for Windows Common Controls
#endif               // _AFX_NO_AFXCMN_SUPPORT

#include <process.h>
#include <mmsystem.h>

#include "tier1/utlvector.h"

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before
// the previous line.

#endif  // !SE_UTILS_VOICE_TWEAK_AUDIO_PCH_H_
