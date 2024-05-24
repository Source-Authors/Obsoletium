//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: generates 4 randum numbers in the range 0..1 quickly, using SIMD
//
//=====================================================================================//

#include <cfloat>	// Needed for FLT_EPSILON
#include <cmath>

#include "tier0/basetypes.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"

#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "mathlib/ssemath.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// see knuth volume 3 for insight.

static constexpr inline float GetStepRand( uint32 seed )
{
	return (seed >> 16) / 65536.0f;
}

static constexpr inline float GetNextStepSeed( uint32 seed )
{
	return (seed + 1) * 3141592621u;
}

struct SIMDRandStreamContext
{
	fltx4 m_RandY[55];
	fltx4 *m_pRand_J, *m_pRand_K;

	void Seed( uint32 seed )
	{
		m_pRand_J = m_RandY + 23; m_pRand_K = m_RandY + 54;
		for (auto &&r : m_RandY)
		{
			float r1 = GetStepRand( seed );
			seed = GetNextStepSeed( seed );

			float r2 = GetStepRand( seed );
			seed = GetNextStepSeed( seed );

			float r3 = GetStepRand( seed );
			seed = GetNextStepSeed( seed );

			float r4 = GetStepRand( seed );

			r = DirectX::XMVectorSet( r1, r2, r3, r4 );
		}
	}

	inline fltx4 XM_CALLCONV RandSIMD()
	{
		// ret = rand[k]+rand[j]
		fltx4 retval = AddSIMD( *m_pRand_K, *m_pRand_J );
		
		// if ( ret>=1.0) ret-=1.0
		fltx4 overflow_mask = CmpGeSIMD( retval, Four_Ones );
		retval = SubSIMD( retval, AndSIMD( Four_Ones, overflow_mask ) );
		
		*m_pRand_K = retval;
		
		// update pointers w/ wrap-around
		if ( --m_pRand_J < m_RandY )
			m_pRand_J = m_RandY + 54;

		if ( --m_pRand_K < m_RandY )
			m_pRand_K = m_RandY + 54;
		
		return retval;
	}
};

constexpr inline int MAX_SIMULTANEOUS_RANDOM_STREAMS = 32;

static SIMDRandStreamContext s_SIMDRandContexts[MAX_SIMULTANEOUS_RANDOM_STREAMS];
static CInterlockedInt s_nRandContextsInUse[MAX_SIMULTANEOUS_RANDOM_STREAMS];

void SeedRandSIMD(uint32 seed)
{
	int i = 0;
	for ( auto &&c : s_SIMDRandContexts )
	{
		c.Seed( seed + i++ );
	}
}

fltx4 XM_CALLCONV RandSIMD( int nContextIndex )
{
	Assert( nContextIndex < (int)std::size(s_nRandContextsInUse) );

	return s_SIMDRandContexts[nContextIndex].RandSIMD();
}

int GetSIMDRandContext()
{
	for(;;)
	{
		int i = 0;
		for ( auto &&u : s_nRandContextsInUse )
		{
			if ( !u )				// available?
			{
				// try to take it!
				if ( u.AssignIf( 0, 1 ) )
				{
					return i;								// done!
				}
			}
			++i;
		}

		Assert(0);											// why don't we have enough buffers?
		ThreadSleep();
	}
}

void ReleaseSIMDRandContext( int nContext )
{
	Assert( nContext < (int)std::size(s_nRandContextsInUse) );

	s_nRandContextsInUse[ nContext ] = 0;
}

fltx4 XM_CALLCONV RandSIMD()
{
	return s_SIMDRandContexts[0].RandSIMD();
}
