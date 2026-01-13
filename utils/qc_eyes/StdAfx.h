//========= Copyright Valve Corporation, All rights reserved. ============//
// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#ifndef SE_UTILS_QC_EYES_STDAFX_H_
#define SE_UTILS_QC_EYES_STDAFX_H_

#include "tier0/platform.h"

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN

// Strict types mode.
#ifndef STRICT
#define STRICT
#endif

#define NOGDICAPMASKS     // - CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOMEMMGR          // - GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE        // - typedef METAFILEPICT
#define NOMINMAX          // - Macros min(a, b) and max(a, b)
#define NOOPENFILE        // - OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSERVICE         // - All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND           // - Sound driver routines
#define NOCOMM            // - COMM driver routines
#define NOKANJI           // - Kanji support stuff.
#define NOPROFILER        // - Profiler interface.
#define NODEFERWINDOWPOS  // - DeferWindowPos routines
#define NOMCX             // - Modem Configuration Extensions
#define NOIME             // - Input method editor
#define NOCRYPT           // - No crypto
#define NOWINRES          // - No windows resources.

#define NOWH              // - SetWindowsHook and WH_*

// Access to Windows 10 features.
#define _WIN32_WINNT 0x0A00

#include "winlite.h"

#include <afxwin.h>    // MFC core and standard components
#include <afxext.h>    // MFC extensions
#include <afxdisp.h>   // MFC Automation classes
#include <afxdtctl.h>  // MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>  // MFC support for Windows Common Controls
#endif               // _AFX_NO_AFXCMN_SUPPORT

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before
// the previous line.

#endif  // !defined(SE_UTILS_QC_EYES_STDAFX_H_)
