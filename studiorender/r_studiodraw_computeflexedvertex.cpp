//========= Copyright c 1996-2008, Valve Corporation, All rights reserved. ============//

#include "tier0/platform.h"

#ifdef PLATFORM_WINDOWS

#include "studiorender.h"
#include "studio.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imorph.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterial.h"
#include "optimize.h"
#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include <malloc.h>
#include "mathlib/vmatrix.h"
#include "studiorendercontext.h"
#include "tier2/tier2.h"
#include "tier0/vprof.h"
//#include "tier0/miniprofiler.h"
#include <algorithm>
#include "filesystem.h"

#define PROFILE_THIS_FILE 0


//DLL_IMPORT CLinkedMiniProfiler *g_pOtherMiniProfilers;
#if PROFILE_THIS_FILE

#if !ENABLE_HARDWARE_PROFILER
#error "Can't profile without profiler enabled"
#endif

CLinkedMiniProfiler g_mp_morph_Vx("morph_Vx", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_Vw("morph_Vw", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_lower_bound("morph_lower_bound", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph("morph", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_V1("morph_V1", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_V2("morph_V2", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_V3("morph_V3", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_V4("morph_V4", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_V5("morph_V5", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_V6("morph_V6", &g_pOtherMiniProfilers);
CLinkedMiniProfiler g_mp_morph_V7("morph_V7", &g_pOtherMiniProfilers);

CLinkedMiniProfiler* g_mp_ComputeFlexedVertex_StreamOffset[8] = 
{
	NULL,
	&g_mp_morph_V1,
	&g_mp_morph_V2,
	&g_mp_morph_V3,
	&g_mp_morph_V4,
	&g_mp_morph_V5,
	&g_mp_morph_V6,
	&g_mp_morph_V7
};
#else
uint32 g_mp_morph_Vx[2];
uint32 g_mp_morph_Vw[2];
#endif

template
void CCachedRenderData::ComputeFlexedVertex_StreamOffset<mstudiovertanim_t>( studiohdr_t *pStudioHdr, mstudioflex_t *pflex, 
														 mstudiovertanim_t *pvanim, int vertCount, float w1, float w2, float w3, float w4 );
template
void CCachedRenderData::ComputeFlexedVertex_StreamOffset<mstudiovertanim_wrinkle_t>( studiohdr_t *pStudioHdr, mstudioflex_t *pflex, 
														 mstudiovertanim_wrinkle_t *pvanim, int vertCount, float w1, float w2, float w3, float w4 );

// vectorized
void CCachedRenderData::ComputeFlexedVertex_StreamOffset_Optimized( studiohdr_t *pStudioHdr, mstudioflex_t *pflex, mstudiovertanim_t *pvanim, int vertCount, float w1, float w2, float w3, float w4 )
{
#if PROFILE_THIS_FILE
	CMiniProfilerGuard mpguard(&g_mp_morph);
#endif
	{
		ComputeFlexedVertex_StreamOffset( pStudioHdr, pflex, pvanim, vertCount, w1, w2, w3, w4);
	}
}


void CCachedRenderData::ComputeFlexedVertexWrinkle_StreamOffset_Optimized( studiohdr_t *pStudioHdr, mstudioflex_t *pflex, mstudiovertanim_wrinkle_t *pvanim, int vertCount, float w1, float w2, float w3, float w4)
{
#if PROFILE_THIS_FILE
	CMiniProfilerGuard mpguard(&g_mp_morph);
#endif

	{
		ComputeFlexedVertex_StreamOffset( pStudioHdr, pflex, pvanim, vertCount, w1, w2, w3, w4);
	}
}

#endif // PLATFORM_WINDOWS
