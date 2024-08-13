//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: To accomplish X360 TCR 22, we need to call present ever 66msec
// at least during loading screens.	This amazing hack will do it
// by overriding the allocator to tick it every so often.
//
// $NoKeywords: $
//===========================================================================//

#include "LoadScreenUpdate.h"
#include "tier0/memalloc.h"
#include "tier1/delegates.h"
#include "tier0/threadtools.h"
#include "tier2/tier2.h"
#include "materialsystem/imaterialsystem.h"
#include "tier0/dbg.h"

