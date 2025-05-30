// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"

#include "tier0/progressbar.h"
#include "tier0/platform.h"
#include "vstdlib/pch_vstdlib.h"

#include <atomic>

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
#include "tier0/memalloc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#endif

namespace {

std::atomic<ProgressReportHandler_t> g_report_handler_fn;

}

PLATFORM_INTERFACE void ReportProgress(char const *job_name, int total_units_to_do, int n_units_completed)
{
	const auto fn = g_report_handler_fn.load( std::memory_order::memory_order_relaxed );
	if (fn)
		fn( job_name, total_units_to_do, n_units_completed );
}

PLATFORM_INTERFACE ProgressReportHandler_t InstallProgressReportHandler( ProgressReportHandler_t pfn)
{
	return g_report_handler_fn.exchange( pfn );
}

