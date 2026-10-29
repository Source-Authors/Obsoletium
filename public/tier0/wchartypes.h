// Copyright Valve Corporation, All rights reserved.
//
// Purpose:	All of our code is completely Unicode.  Instead of char, you
// should use wchar, uint8, or char8, as explained below.

#ifndef SE_PUBLIC_TIER0_WCHARTYPES_H_
#define SE_PUBLIC_TIER0_WCHARTYPES_H_

#ifdef _INC_TCHAR
#error "Must include tier0 type headers before tchar.h"
#endif

// Temporarily turn off Valve defines
#include "valve_off.h"

// dimhotepus: wchar_t is a built-in type in C++; it must never be typedef'd
// here.  The old "GNUC" guard was a typo for __GNUC__ and only compiled by
// accident when <cstddef> happened to be included first.

// char8
// char8 is equivalent to char, and should be used when you really need a char
// (for example, when calling an external function that's declared to take
// chars).
using char8 = char;

// uint8
// uint8 is equivalent to byte (but is preferred over byte for clarity).  Use
// this whenever you mean a byte (for example, one byte of a network packet).
using uint8 = unsigned char;
using byte = unsigned char;

// wchar
// wchar is a single character of text (16 bits on Windows, 32 bits on
// Linux/macOS, as all of our text is Unicode).  Use this whenever you mean a
// piece of text (for example, in a string).
using wchar = wchar_t;
// typedef char wchar;

// WFILE
// This is a Unicode version of __FILE__
#define WIDEN2(x) L##x
#define WIDEN(x) WIDEN2(x)
// dimhotepus: __WFILE__ -> WFILE
#define WFILE WIDEN(__FILE__)

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
// dimhotepus: stricmp -> strcasecmp for POSIX.
#define _tcsicmp strcasecmp  // POSIX, <strings.h>
#define _tcscmp strcmp
#define _tcscpy strcpy
#define _tcsncpy strncpy
#define _tcsrchr strrchr
#define _tcslen strlen
#define _tfopen fopen
#define _stprintf sprintf
#define _ftprintf fprintf
#define _vsntprintf vsnprintf
#define _tprintf printf
#define _sntprintf snprintf
#define _T(s) s
#endif

#if defined(_UNICODE)
using tchar = wchar;
#define tstring std::wstring
// dimhotepus: __TFILE__ -> TFILE
#define TFILE WFILE
#define TCHAR_IS_WCHAR
#else
using tchar = char;
#define tstring std::string
// dimhotepus: __TFILE__ -> TFILE
#define TFILE __FILE__
#define TCHAR_IS_CHAR
#endif

// dimhotepus: _WIN32 is set by the compiler; WIN32 only exists if the project
// defines it.
#if defined(_WIN32)
using uchar16 = wchar_t;
using uchar32 = unsigned int;
#else
using uchar16 = unsigned short;
using uchar32 = wchar_t;
#endif
static_assert(sizeof(uchar16) == 2, "uchar16 must be 16 bits");
static_assert(sizeof(uchar32) == 4, "uchar32 must be 32 bits");

#ifdef FORCED_UNICODE
#undef _UNICODE
#undef FORCED_UNICODE
#endif

// Turn valve defines back on
#include "valve_on.h"

#endif  // !SE_PUBLIC_TIER0_WCHARTYPES_H_