//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "bitmap/float_bm.h"

#include <utility>
#include <cmath>

#include "mathlib/mathlib.h"
#include "tier0/platform.h"
#include "tier0/threadtools.h"
#include "tier0/progressbar.h"
#include "tier0/wchartypes.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct TBFCalculationContext
{
	int min_y,max_y;											// range to calculate in this thread
	int thread_number;
	int radius_in_pixels;
	float edge_threshold_value;
	FloatBitMap_t const *orig_bm;
	FloatBitMap_t *dest_bm;
};

static unsigned TBFCalculationThreadFN( void *ctx1 )
{
	auto *ctx = static_cast<TBFCalculationContext *>(ctx1);
	for(int y=ctx->min_y; y <= ctx->max_y; y++)
	{
		if ( ctx->thread_number == 0 )
			ReportProgress("Performing bilateral filter",(1+ctx->max_y-ctx->min_y),
						   y-ctx->min_y);
		for(int x=0; x < ctx->dest_bm->Width; x++)
			for(int c=0;c<4;c++)
			{
				float sum_weights=0;
				float filter_sum=0;
				float centerp=ctx->orig_bm->Pixel(x,y,c);
				for(int iy=-ctx->radius_in_pixels; iy <= ctx->radius_in_pixels; iy++)
					for(int ix=-ctx->radius_in_pixels; ix <= ctx->radius_in_pixels; ix++)
					{
						float this_p=ctx->orig_bm->PixelWrapped(x+ix,y+iy,c);
						
						// caluclate the g() term. We use a gaussian
						float exp1=(static_cast<float>(ix)*ix+iy*iy)*(1.0f/(2.0f*ctx->radius_in_pixels*.033f));
						float g=expf(-exp1);
						// calculate the "similarity" term. We use a triangle filter
						float cdiff=fabsf(centerp-this_p);
						float s= (cdiff>ctx->edge_threshold_value)?0:
							FLerp(1,0,0,ctx->edge_threshold_value,cdiff);
						sum_weights += s*g;
						filter_sum += s*g*this_p;
					}
				ctx->dest_bm->Pixel(x,y,c)=filter_sum/sum_weights;
			}
	}
	return 0;
}

void FloatBitMap_t::TileableBilateralFilter( int radius_in_pixels, 
											 float edge_threshold_value )
{
	constexpr uint8 kParallelContextsCount = 32;

	FloatBitMap_t orig( this );								// need a copy for the source
	TBFCalculationContext ctxs[kParallelContextsCount];
	ctxs[0].radius_in_pixels = radius_in_pixels;
	ctxs[0].edge_threshold_value = edge_threshold_value;
	ctxs[0].orig_bm = &orig;
	ctxs[0].dest_bm = this;
	int nthreads = min( kParallelContextsCount, GetCPUInformation()->m_nPhysicalProcessors );
	ThreadHandle_t waithandles[kParallelContextsCount];
	int starty=0;
	int ystep=Height/nthreads;

	for(int t=0;t<nthreads;t++)
	{
		if (t)
			ctxs[t]=ctxs[0];
		ctxs[t].thread_number=t;
		ctxs[t].min_y=starty;
		if (t != nthreads-1)
			ctxs[t].max_y=min(Height-1,starty+ystep-1);
		else
			ctxs[t].max_y=Height-1;
		waithandles[t]=CreateSimpleThread(TBFCalculationThreadFN, &ctxs[t]);
		starty+=ystep;
	}

	for(int t=0;t<nthreads;t++)
	{
		ThreadJoin( waithandles[t] );
	}
}

