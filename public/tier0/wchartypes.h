// Copyright Valve Corporation, All rights reserved.
//
// Purpose:	All of our code is completely Unicode.  Instead of char, you should
// use wchar, uint8, or char8, as explained below.

#ifndef SE_PUBLIC_TIER0_WCHARTYPES_H_
#define SE_PUBLIC_TIER0_WCHARTYPES_H_

#ifdef _INC_TCHAR
#error "Must include tier0 type headers before tchar.h"
#endif

// Temporarily turn off Valve defines
#include "valve_off.h"

#if !defined(_WCHAR_T_DEFINED) && !defined(GNUC)
typedef unsigned short wchar_t;
#define _WCHAR_T_DEFINED
#endif

// char8
// char8 is equivalent to char, and should be used when you really need a char
// (for example, when calling an external function that's declared to take
// chars).
using char8 = char;

// uint8
// uint8 is equivalent to byte (but is preferred over byte for clarity).  Use this
// whenever you mean a byte (for example, one byte of a network packet).
using uint8 = unsigned char;
using byte = unsigned char;

// wchar
// wchar is a single character of text (currently 16 bits, as all of our text is
// Unicode).  Use this whenever you mean a piece of text (for example, in a string).
using wchar = wchar_t;
//typedef char wchar;

// __WFILE__
// This is a Unicode version of __FILE__
#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)
#define __WFILE__ WIDEN(__FILE__)

#ifdef STEAM
#ifndef _UNICODE
#define FORCED_UNICODE
#endif
#define _UNICODE
#endif

#ifdef _WIN32
#include <tchar.h>
#else
#define _tcsstr strstr
#define _tcsicmp stricmp
#define _tcscmp strcmp
#define _tcscpy strcpy
#define _tcsncpy strncpy
#define _tcsrchr strrchr
#define _tcslen strlen
#define _tfopen fopen
#define _stprintf sprintf 
#define _ftprintf fprintf
#define _vsntprintf _vsnprintf
#define _tprintf printf
#define _sntprintf _snprintf
#define _T(s) s
#endif

#if defined(_UNICODE)
typedef wchar tchar;
#define tstring std::wstring
#define __TFILE__ __WFILE__
#define TCHAR_IS_WCHAR
#else
using tchar = char;
#define tstring std::string
#define __TFILE__ __FILE__
#define TCHAR_IS_CHAR
#endif

#if defined( WIN32 )
using uchar16 = wchar_t;
using uchar32 = unsigned int;
#else
typedef unsigned short uchar16;
typedef wchar_t uchar32;
#endif

#ifdef FORCED_UNICODE
#undef _UNICODE
#endif

// Turn valve defines back on
#include "valve_on.h"


#endif  // !SE_PUBLIC_TIER0_WCHARTYPES_H_
