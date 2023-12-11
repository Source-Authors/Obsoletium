// GNU LESSER GENERAL PUBLIC LICENSE
// Version 3, 29 June 2007
//
// Copyright (C) 2007 Free Software Foundation, Inc. <http://fsf.org/>
//
// Everyone is permitted to copy and distribute verbatim copies of this license
// document, but changing it is not allowed.
//
// This version of the GNU Lesser General Public License incorporates the terms
// and conditions of version 3 of the GNU General Public License, supplemented
// by the additional permissions listed below.

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

// dimhotepus: Custom fixes to compile without ATL.
#ifdef MPA_BUILD_WITHOUT_ATL

#ifdef UNICODE

typedef wchar_t *PTSTR, *LPTSTR;
typedef const wchar_t *PCTSTR, *LPCTSTR;

#else /* UNICODE */

typedef char *PTSTR, *LPTSTR;
typedef const char *PCTSTR, *LPCTSTR;

#endif /* UNICODE */

// dimhotepus: Add missed headers.
#include <tchar.h>   // _T
#include <malloc.h>  // malloc
#include <crtdbg.h>  // assert

#include <cassert>   // assert
#include <cstdio>    // sprintf_s
#include <cstdlib>   // atoi
#include <string>

#define CString std::string
#define MPA_USE_STRING_FOR_CSTRING 1

#else  // Original code.

#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN  // Exclude rarely-used stuff from Windows headers
#endif

// Modify the following defines if you have to target a platform prior to the
// ones specified below. Refer to MSDN for the latest info on corresponding
// values for different platforms.
#ifndef WINVER  // Allow use of features specific to Windows 95 and Windows NT 4
                // or later.
#define WINVER \
  0x0400  // Change this to the appropriate value to target Windows 98 and
          // Windows 2000 or later.
#endif

#ifndef _WIN32_WINNT  // Allow use of features specific to Windows NT 4 or
                      // later.
#define _WIN32_WINNT \
  0x0400  // Change this to the appropriate value to target Windows 98 and
          // Windows 2000 or later.
#endif

#ifndef _WIN32_WINDOWS  // Allow use of features specific to Windows 98 or
                        // later.
#define _WIN32_WINDOWS \
  0x0410  // Change this to the appropriate value to target Windows Me or later.
#endif

#ifndef _WIN32_IE  // Allow use of features specific to IE 4.0 or later.
#define _WIN32_IE \
  0x0400  // Change this to the appropriate value to target IE 5.0 or later.
#endif

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS  // some CString constructors will be
                                            // explicit

// turns off MFC's hiding of some common and often safely ignored warning
// messages
#define _AFX_ALL_WARNINGS

#include <afxwin.h>   // MFC core and standard components
#include <afxext.h>   // MFC extensions
#include <afxdisp.h>  // MFC Automation classes

#include <afxdtctl.h>  // MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>  // MFC support for Windows Common Controls
#endif               // _AFX_NO_AFXCMN_SUPPORT

#endif
