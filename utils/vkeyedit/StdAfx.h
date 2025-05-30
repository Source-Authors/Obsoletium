// Copyright Valve Corporation, All rights reserved.

#if !defined(AFX_STDAFX_H__4EEE588D_48F4_45A3_87B1_DAFF7613BEED__INCLUDED_)
#define AFX_STDAFX_H__4EEE588D_48F4_45A3_87B1_DAFF7613BEED__INCLUDED_

#include "tier0/platform.h"

#define VC_EXTRALEAN  // Exclude rarely-used stuff from Windows headers
#define STRICT

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <afxwin.h>  // MFC core and standard components
#include <afxext.h>  // MFC extensions
#include <afxcview.h>
#include <afxdisp.h>   // MFC Automation classes
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>  // MFC support for Windows Common Controls
#endif               // _AFX_NO_AFXCMN_SUPPORT
#include <afxpriv.h>

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before
// the previous line.

#endif  // !defined(AFX_STDAFX_H__4EEE588D_48F4_45A3_87B1_DAFF7613BEED__INCLUDED_)
