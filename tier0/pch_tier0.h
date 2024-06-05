// Copyright Valve Corporation, All rights reserved.

// tier0
#include "tier0/basetypes.h"
#include "tier0/dbgflag.h"
#include "tier0/dbg.h"
#ifdef STEAM
#include "tier0/memhook.h"
#endif
#include "tier0/validator.h"

// First include standard libraries
#include "tier0/valve_off.h"

#include <malloc.h>
#include <memory.h>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <climits>
#include <map>

#ifdef POSIX
#include <cctype>
#include <climits>
#define _MAX_PATH PATH_MAX
#endif

#include "tier0/valve_on.h"
