// Copyright Valve Corporation, All rights reserved.

#if defined(PLATFORM_WINDOWS_PC)
#include "winlite.h"
#elif defined(_PS3)
#include <cellstatus.h>
#include <sys/prx.h>
#endif

#include "tier0/platform.h"

// First include standard libraries
#include "tier0/valve_off.h"

#include <ctype.h>
#include <cassert>
#include <cstdio>
#include <cmath>
#include <climits>
#include <cstddef>

#ifdef PLATFORM_POSIX
#include <unistd.h>
#define _MAX_PATH PATH_MAX
#endif

#include "tier0/valve_on.h"

#include "tier0/basetypes.h"
#include "tier0/dbgflag.h"
#include "tier0/dbg.h"

#ifdef STEAM
#include "tier0/memhook.h"
#endif

#include "tier0/validator.h"
#include "tier0/fasttimer.h"
