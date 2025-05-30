//========= Copyright Valve Corporation, All rights reserved. ============//
#ifndef ANALYSIS_ANNOTATIONS_H
#define ANALYSIS_ANNOTATIONS_H

#if _MSC_VER >= 1600 // VS 2010 and above.
//-----------------------------------------------------------------------------
// Upgrading important helpful warnings to errors
//-----------------------------------------------------------------------------
#pragma warning(error : 4789 ) // warning C4789: destination of memory copy is too small

// Suppress some code analysis warnings
// Include the annotation header file.
#include <sal.h>

// For temporarily suppressing warnings -- the warnings are suppressed for the next source line.
#define ANALYZE_SUPPRESS(wnum) __pragma(warning(suppress: wnum))
#define ANALYZE_SUPPRESS2(wnum1, wnum2) __pragma(warning(supress: wnum1  wnum2))
#define ANALYZE_SUPPRESS3(wnum1, wnum2, wnum3) __pragma(warning(suppress: wnum1 wnum2 wnum3))
#define ANALYZE_SUPPRESS4(wnum1, wnum2, wnum3, wnum4) __pragma(warning(suppress: wnum1 wnum2 wnum3 wnum4))

// Tag all printf style format strings with this
#define PRINTF_FORMAT_STRING _Printf_format_string_
#define SCANF_FORMAT_STRING _Scanf_format_string_impl_
// Various macros for specifying the capacity of the buffer pointed
// to by a function parameter. Variations include in/out/inout,
// CAP (elements) versus BYTECAP (bytes), and null termination or
// not (_Z).
#define IN_Z _In_z_
#define IN_OPT _In_opt_
#define IN_OPT_Z _In_opt_z_
#define IN_CAP(x) _In_count_(x)
#define IN_CAP_OPT(x) _In_opt_count_(x)
#define IN_Z_CAP(x) _In_z_count_(x)
#define IN_Z_CAP_C(x) _In_z_count_c_(x)
#define IN_BYTECAP(x) _In_bytecount_(x)
#define OUT_Z_CAP(x) _Out_z_cap_(x)
#define OUT_Z_CAP_OPT(x) _Out_opt_z_cap_(x)
#define OUT_CAP(x) _Out_cap_(x)
#define OUT_CAP_OPT(x) _Out_opt_cap_(x)
#define OUT_CAP_C(x) _Out_cap_c_(x) // Output buffer with specified *constant* capacity in elements
#define OUT_BYTECAP(x) _Out_bytecap_(x)
#define OUT_BYTECAP_C(x) _Out_bytecap_c_(x)
#define OUT_OPT_BYTECAP(x) _Out_opt_bytecap_(x)
#define OUT_Z_BYTECAP(x) _Out_z_bytecap_(x)
#define INOUT_BYTECAP(x) _Inout_bytecap_(x)
#define INOUT_Z _Inout_z_
#define INOUT_Z_CAP(x) _Inout_z_cap_(x)
#define INOUT_Z_BYTECAP(x) _Inout_z_bytecap_(x)
// dimhotepus: Add return may be null annotation.
#define RET_MAY_BE_NULL _Ret_maybenull_
// These macros are use for annotating array reference parameters, typically used in functions
// such as V_strcpy_safe. Because they are array references the capacity is already known.
#define IN_Z_ARRAY _Pre_z_
#define OUT_Z_ARRAY _Post_z_
#define INOUT_Z_ARRAY _Prepost_z_
// Used for annotating functions to describe their return types.
#define MUST_CHECK_RETURN _Check_return_
// Use the macros above to annotate string functions that fill buffers as shown here,
// in order to give VS's /analyze more opportunities to find bugs.
// void V_wcsncpy( OUT_Z_BYTECAP(maxLenInBytes) wchar_t *pDest, wchar_t const *pSrc, int maxLenInBytes );
// int V_snwprintf( OUT_Z_CAP(maxLenInCharacters) wchar_t *pDest, int maxLenInCharacters, PRINTF_FORMAT_STRING const wchar_t *pFormat, ... );

#endif // _MSC_VER >= 1600 // VS 2010 and above.

#endif // ANALYSIS_ANNOTATIONS_H
