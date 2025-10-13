// Copyright Valve Corporation, All rights reserved.
//
// Purpose: Provide a shared place for library fucntions to report progress % for display

#ifndef SE_PUBLIC_TIER0_PROGRESSBAR_H_
#define SE_PUBLIC_TIER0_PROGRESSBAR_H_

#include "platform.h"

PLATFORM_INTERFACE void ReportProgress(char const *job_name, int total_units_to_do, 
									   int n_units_completed);

using ProgressReportHandler_t = void (*)( char const*, int, int );

// install your own handler. returns previous handler
PLATFORM_INTERFACE ProgressReportHandler_t InstallProgressReportHandler( ProgressReportHandler_t pfn);

#endif  // !SE_PUBLIC_TIER0_PROGRESSBAR_H_
