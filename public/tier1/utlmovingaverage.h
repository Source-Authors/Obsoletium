//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Simple moving average class
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef MOVING_AVERAGE_H
#define MOVING_AVERAGE_H

#include <cfloat>
#include <cmath>
#include "tier0/platform.h"
#include "tier0/basetypes.h"

template<uint32 TBufferSize> class CUtlMovingAverage
{
	static_assert( TBufferSize > 0, "CUtlMovingAverage requires a non-zero buffer size" );

public:
	CUtlMovingAverage() :
		m_nValuesPushed( 0 ),
		m_nNextIndex( 0 ),
		m_flTotal( 0.0 )
	{
		// dimhotepus: Ensure init.
		for ( uint32 i = 0; i < TBufferSize; ++i )
			m_Buffer[i] = 0.0f;
	}

	void Reset()
	{
		m_nValuesPushed = 0;
		// dimhotepus: Ensure reset.
		m_nNextIndex = 0;
		m_flTotal = 0.0;
	}

	[[nodiscard]] uint32 GetTotalValuesPushed() const
	{
		return m_nValuesPushed;
	}

	[[nodiscard]] float GetAverage() const
	{
		const uint32 n = MIN( TBufferSize, m_nValuesPushed );
		return n ? static_cast<float>( m_flTotal / static_cast<double>( n ) ) : 0.0f;
	}

	void GetAverageAndAbsRange( float *pflOutAverage, float *pflOutAbsRange, float *pflMinTime, float *pflMaxTime ) const
	{
		if ( m_nValuesPushed == 0 )
		{
			*pflOutAverage = 0.0f;
			*pflOutAbsRange = 0.0f;
			*pflMinTime = 0.0f;
			*pflMaxTime = 0.0f;
			return;
		}

		const float flAverage = GetAverage();
		const uint32 nNumValues = MIN( TBufferSize, m_nValuesPushed );

		float flAbsRange = 0.0f;
		float flMinValue = FLT_MAX;
		float flMaxValue = -FLT_MAX;

		for ( uint32 i = 0; i < nNumValues; ++i )
		{
			const float v = m_Buffer[i];
			flAbsRange = MAX( flAbsRange, fabsf( v - flAverage ) );
			flMinValue = MIN( flMinValue, v );
			flMaxValue = MAX( flMaxValue, v );
		}

		*pflOutAverage = flAverage;
		*pflOutAbsRange = flAbsRange;
		*pflMinTime = flMinValue;
		*pflMaxTime = flMaxValue;
	}

	void PushValue( float v )
	{
		// dimhotepus: Once the window is full, the slot being overwritten holds the oldest value
		if ( m_nValuesPushed >= TBufferSize )
		{
			m_flTotal -= m_Buffer[m_nNextIndex];
		}
		m_flTotal += v;

		m_Buffer[m_nNextIndex] = v;
		m_nNextIndex = ( m_nNextIndex + 1 ) % TBufferSize;

		// Saturate the count instead of resetting, so the window stays valid
		if ( m_nValuesPushed != ~uint32( 0 ) )
		{
			++m_nValuesPushed;
		}
	}

private:
	float m_Buffer[TBufferSize];
	uint32 m_nValuesPushed;
	uint32 m_nNextIndex;

	double m_flTotal;
};

#endif // MOVING_AVERAGE_H
