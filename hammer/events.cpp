//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: event time stamping for hammer
//
//===========================================================================//

#include "stdafx.h"
#include "tier0/platform.h"
#include "mathlib/mathlib.h"
#include "hammer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static std::atomic_int g_EventTimeCounters[100];
static double g_EventTimes[ssize(g_EventTimeCounters)];

void SignalUpdate(int ev)
{
	Assert(ev >= 0 && ev < ssize(g_EventTimes));
	g_EventTimes[ev]=Plat_FloatTime();
	g_EventTimeCounters[ev].fetch_add(1, std::memory_order::memory_order_relaxed);
}

int GetUpdateCounter(int ev)
{
	Assert(ev >= 0 && ev < ssize(g_EventTimes));
	return g_EventTimeCounters[ev].load(std::memory_order::memory_order_relaxed);
}

double GetUpdateTime(int ev)
{
	Assert(ev >= 0 && ev < ssize(g_EventTimes));
	return g_EventTimes[ev];
}

void SignalGlobalUpdate()
{
	double stamp=Plat_FloatTime();
	for(intp i=0;i<ssize(g_EventTimes);i++)
	{
		g_EventTimes[i] = stamp;
		g_EventTimeCounters[i].fetch_add(1, std::memory_order::memory_order_relaxed);
	}
}
