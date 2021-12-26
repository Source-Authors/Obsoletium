// Copyright Valve Corporation, All rights reserved.

#ifndef VPC_VSTDLIB_CVAR_H_
#define VPC_VSTDLIB_CVAR_H_

#include "vstdlib/vstdlib.h"
#include "icvar.h"

// Returns a CVar dictionary for tool usage
VSTDLIB_INTERFACE CreateInterfaceFn VStdLib_GetICVarFactory();

#endif  // VPC_VSTDLIB_CVAR_H_
