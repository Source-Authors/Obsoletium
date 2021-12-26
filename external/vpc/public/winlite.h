// Copyright Valve Corporation, All rights reserved.

#ifndef VPC_WINLITE_H_
#define VPC_WINLITE_H_

#ifdef _WIN32
//
// Prevent tons of unused windows definitions
//
#define WIN32_LEAN_AND_MEAN
#define NOWINRES
#define NOSERVICE
#define NOMCX
#define NOIME

#ifndef _X360
#include <Windows.h>
#include <VersionHelpers.h>
#endif

#undef PostMessage

#endif  // WIN32

#endif  // VPC_WINLITE_H_
