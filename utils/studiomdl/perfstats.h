// Copyright Valve Corporation, All rights reserved.

#ifndef SE_UTILS_STUDIOMDL_PERFSTATS_H_
#define SE_UTILS_STUDIOMDL_PERFSTATS_H_

#include "studio.h"
#include "optimize.h"

enum
{
	SPEWPERFSTATS_SHOWSTUDIORENDERWARNINGS = 1,
	SPEWPERFSTATS_SHOWPERF = 2,
	SPEWPERFSTATS_SPREADSHEET = 4,
};

void SpewPerfStats( studiohdr_t *pStudioHdr, const char *pFilename, unsigned int flags );

#endif  // !SE_UTILS_STUDIOMDL_PERFSTATS_H_
