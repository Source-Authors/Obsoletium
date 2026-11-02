//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Random number generator
//
// $Workfile: $
// $NoKeywords: $
//===========================================================================//

#include "vstdlib/random.h"
#include "tier0/dbg.h"

#include <atomic>
#include <cmath>

#include "tier0/memdbgon.h"

namespace
{

// Park-Miller minimal standard LCG, computed with Schrage's method so that
// nothing overflows 32-bit int.
constexpr int IA{16807}; //-V707
constexpr int IM{2147483647}; //-V707
constexpr int IQ{127773}; //-V707
constexpr int IR{2836}; //-V707
constexpr int NTAB{CUniformRandomStream::SHUFFLE_TABLE_SIZE};
constexpr int NDIV{1 + (IM - 1) / NTAB};

static_assert( IM == IA * IQ + IR, "Schrage factorization of IM is wrong." );
static_assert( NTAB > 0 && ( NTAB & ( NTAB - 1 ) ) == 0, "Shuffle table size must be a power of two." );

constexpr inline uint32 MAX_RANDOM_RANGE_32BIT{0x7FFF'FFFFU};

#ifdef PLATFORM_64BITS
// dimhotepus: int64 support.
constexpr inline uint64 MAX_RANDOM_RANGE_64BIT{ 0x7FFF'FFFF'FFFF'FFFFULL };
#endif

// fran1 -- return a random floating-point number on the interval [0,1)
constexpr inline float AM{1.0f / static_cast<float>( IM )}; //-V707
constexpr inline float EPS{1.2e-7f};
constexpr inline float RNMX{1.0f - EPS};

}  // namespace

//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------
static CUniformRandomStream s_UniformStream;
static CGaussianRandomStream s_GaussianStream;
// dimhotepus: Atomic, as InstallUniformRandomStream may run concurrently with
// Random* calls on other threads. Constant-initialized, so it is valid before
// any dynamic initializer runs.
static std::atomic<IUniformRandomStream *> s_pUniformStream{ &s_UniformStream };

static inline IUniformRandomStream *GetUniformStream()
{
	return s_pUniformStream.load( std::memory_order_acquire );
}

//-----------------------------------------------------------------------------
// Installs a global random number generator, which will affect the Random functions above
//-----------------------------------------------------------------------------
void InstallUniformRandomStream( IUniformRandomStream *pStream )
{
	s_pUniformStream.store( pStream ? pStream : &s_UniformStream, std::memory_order_release );
}


//-----------------------------------------------------------------------------
// A couple of convenience functions to access the library's global uniform stream
//-----------------------------------------------------------------------------
void RandomSeed( int iSeed )
{
	GetUniformStream()->SetSeed( iSeed );
}

float RandomFloat( float flMinVal, float flMaxVal )
{
	return GetUniformStream()->RandomFloat( flMinVal, flMaxVal );
}

float RandomFloatExp( float flMinVal, float flMaxVal, float flExponent )
{
	return GetUniformStream()->RandomFloatExp( flMinVal, flMaxVal, flExponent );
}

int RandomInt( int iMinVal, int iMaxVal )
{
	return GetUniformStream()->RandomInt( iMinVal, iMaxVal );
}

#ifdef PLATFORM_64BITS
// dimhotepus: int64 support.
int64 RandomInt64( int64 iMinVal, int64 iMaxVal )
{
	return GetUniformStream()->RandomInt64( iMinVal, iMaxVal );
}
#endif

float RandomGaussianFloat( float flMean, float flStdDev )
{
	return s_GaussianStream.RandomFloat( flMean, flStdDev );
}


//-----------------------------------------------------------------------------
//
// Implementation of the uniform random number stream
//
//-----------------------------------------------------------------------------
CUniformRandomStream::CUniformRandomStream()
	: m_idum{0}, m_iy{0}, m_iv{}
{
	SetSeed(0);
}

void CUniformRandomStream::SetSeed( int iSeed )
{
	AUTO_LOCK( m_mutex );
	// Negating is safe: only non-negative values are negated.
	m_idum = ( ( iSeed < 0 ) ? iSeed : -iSeed );
	m_iy = 0;
}

int CUniformRandomStream::GenerateRandomNumber()
{
	// Caller must hold m_mutex.
	int j;
	int k;

	if (m_idum <= 0 || !m_iy)
	{
		if (-(m_idum) < 1)
			m_idum=1;
		else
			m_idum = -(m_idum);

		for ( j=NTAB+7; j>=0; j--)
		{
			k = (m_idum)/IQ;
			m_idum = IA*(m_idum-k*IQ)-IR*k;
			if (m_idum < 0)
				m_idum += IM;
			if (j < NTAB)
				m_iv[j] = m_idum;
		}
		m_iy=m_iv[0];
	}
	k=(m_idum)/IQ;
	m_idum=IA*(m_idum-k*IQ)-IR*k;
	if (m_idum < 0)
		m_idum += IM;
	j=m_iy/NDIV;

	// We're seeing some strange memory corruption in the contents of s_pUniformStream.
	// Perhaps it's being caused by something writing past the end of this array?
	// Bounds-check in release to see if that's the case.
	if (j >= NTAB || j < 0)
	{
		DebuggerBreakIfDebugging();
		Warning("CUniformRandomStream had an array overrun: tried to write to element %d of 0..%d.\n", j, NTAB - 1);
		// Clamp j. NTAB is a power of two (see static_assert above).
		j &= NTAB - 1;
	}

	m_iy=m_iv[j];
	m_iv[j] = m_idum;

	return m_iy;
}

uint32 CUniformRandomStream::GenerateRandomBelow( uint32 uRange )
{
	// dimhotepus: Caller must hold m_mutex.
	Assert( uRange >= 2U && uRange - 1U <= MAX_RANDOM_RANGE_32BIT );

	// The following maps a uniform distribution on the interval [0,MAX_RANDOM_RANGE]
	// to a smaller, client-specified range of [0,x-1] in a way that doesn't bias
	// the uniform distribution unfavorably. Even for a worst case x, the loop is
	// guaranteed to be taken no more than half the time, so for that worst case x,
	// the average number of times through the loop is 2. For cases where x is
	// much smaller than MAX_RANDOM_RANGE, the average number of times through the
	// loop is very close to 1.
	const uint32 maxAcceptable = MAX_RANDOM_RANGE_32BIT - ( ( MAX_RANDOM_RANGE_32BIT + 1U ) % uRange );
	uint32 n;
	do
	{
		n = static_cast<uint32>( GenerateRandomNumber() );
	} while ( n > maxAcceptable );

	return n % uRange;
}

#ifdef PLATFORM_64BITS
// dimhotepus: int64 support.
uint64 CUniformRandomStream::GenerateRandom63Bits()
{
	// dimhotepus: Caller must hold m_mutex.
	//
	// The generator only yields IM - 1 = 2^31 - 2 distinct values, so it cannot
	// produce 63 bits in one call. Build the result from three uniform 21-bit
	// chunks. Each chunk rejects the top partial block of 2^21 values, which
	// happens with probability ~0.1%.
	constexpr unsigned kChunkBits{21};
	constexpr uint32 kChunkMask{( 1U << kChunkBits ) - 1U};
	// GenerateRandomNumber() - 1 is uniform in [0, kValues).
	constexpr uint32 kValues{static_cast<uint32>( IM ) - 1U};
	constexpr uint32 kLimit{kValues - ( kValues % ( 1U << kChunkBits ) )};
	static_assert( 3 * kChunkBits == 63, "Chunks must add up to 63 bits." );

	uint64 result = 0;
	for ( int i = 0; i < 3; ++i )
	{
		uint32 v;
		do
		{
			v = static_cast<uint32>( GenerateRandomNumber() ) - 1U;
		} while ( v >= kLimit );

		result = ( result << kChunkBits ) | ( v & kChunkMask );
	}
	return result;
}
#endif

float CUniformRandomStream::RandomFloat( float flLow, float flHigh )
{
	AUTO_LOCK( m_mutex );
	// float in [0,1)
	float fl = AM * static_cast<float>( GenerateRandomNumber() );
	if (fl > RNMX)
	{
		fl = RNMX;
	}
	return (fl * ( flHigh - flLow ) ) + flLow; // float in [low,high)
}

float CUniformRandomStream::RandomFloatExp( float flMinVal, float flMaxVal, float flExponent )
{
	AUTO_LOCK( m_mutex );
	// float in [0,1)
	float fl = AM * static_cast<float>( GenerateRandomNumber() );
	if (fl > RNMX)
	{
		fl = RNMX;
	}
	if ( flExponent != 1.0f )
	{
		fl = std::pow( fl, flExponent );
	}
	return (fl * ( flMaxVal - flMinVal ) ) + flMinVal; // float in [low,high)
}

int CUniformRandomStream::RandomInt( int iLow, int iHigh )
{
	// Unsigned math: wraps instead of signed overflow UB for wide ranges.
	const uint32 x = static_cast<uint32>( iHigh ) - static_cast<uint32>( iLow ) + 1U;

	// If you hit either of these assert, you're not getting back the random number that you thought you were.
	Assert( static_cast<int64>( x ) == static_cast<int64>( iHigh ) - iLow + 1 ); // Check that we didn't overflow int
	Assert( x - 1U <= MAX_RANDOM_RANGE_32BIT ); // Check that the values provide an acceptable range

	if ( x <= 1U || x - 1U > MAX_RANDOM_RANGE_32BIT )
	{
		Assert( iLow == iHigh ); // This is the only time it is OK to have a range containing a single number
		return iLow;
	}

	AUTO_LOCK( m_mutex );
	// Result is within [iLow, iHigh], so converting back to int is exact.
	return static_cast<int>( static_cast<uint32>( iLow ) + GenerateRandomBelow( x ) );
}

#ifdef PLATFORM_64BITS
// dimhotepus: int64 support.
int64 CUniformRandomStream::RandomInt64( int64 iLow, int64 iHigh )
{
	// Unsigned math: wraps instead of signed overflow UB for wide ranges.
	const uint64 x = static_cast<uint64>( iHigh ) - static_cast<uint64>( iLow ) + 1U;

	// If you hit this assert, you're not getting back the random number that you thought you were.
	Assert( x - 1U <= MAX_RANDOM_RANGE_64BIT ); // Check that the values provide an acceptable range

	if ( x <= 1U || x - 1U > MAX_RANDOM_RANGE_64BIT )
	{
		Assert( iLow == iHigh ); // This is the only time it is OK to have a range containing a single number
		return iLow;
	}

	AUTO_LOCK( m_mutex );

	uint64 offset;
	if ( x - 1U <= MAX_RANDOM_RANGE_32BIT )
	{
		// Same draw as RandomInt, so RandomIntp gives identical results on 32
		// and 64 bit builds for ranges that fit in int.
		offset = GenerateRandomBelow( static_cast<uint32>( x ) );
	}
	else
	{
		// Same rejection scheme as above, over a full 63-bit source.
		const uint64 maxAcceptable = MAX_RANDOM_RANGE_64BIT - ( ( MAX_RANDOM_RANGE_64BIT + 1U ) % x );
		uint64 n;
		do
		{
			n = GenerateRandom63Bits();
		} while ( n > maxAcceptable );

		offset = n % x;
	}

	// Result is within [iLow, iHigh], so converting back to int64 is exact.
	return static_cast<int64>( static_cast<uint64>( iLow ) + offset );
}
#endif

//-----------------------------------------------------------------------------
//
// Implementation of the gaussian random number stream
// We're gonna use the Box-Muller method (which actually generates 2
// gaussian-distributed numbers at once)
//
//-----------------------------------------------------------------------------
CGaussianRandomStream::CGaussianRandomStream( IUniformRandomStream *pUniformStream )
	: m_pUniformStream{nullptr}, m_bHaveValue{false}, m_flRandomValue{0.0f}
{
	AttachToStream( pUniformStream );
}


//-----------------------------------------------------------------------------
// Attaches to a random uniform stream
//-----------------------------------------------------------------------------
void CGaussianRandomStream::AttachToStream( IUniformRandomStream *pUniformStream )
{
	AUTO_LOCK( m_mutex );
	m_pUniformStream = pUniformStream;
	m_bHaveValue = false;
}


//-----------------------------------------------------------------------------
// Generates random numbers
//-----------------------------------------------------------------------------
float CGaussianRandomStream::RandomFloat( float flMean, float flStdDev )
{
	// dimhotepus: Lock order is always gaussian -> uniform, never the reverse.
	AUTO_LOCK( m_mutex );

	if (m_bHaveValue)
	{
		m_bHaveValue = false;
		return flStdDev * m_flRandomValue + flMean;
	}

	IUniformRandomStream *pUniformStream = m_pUniformStream ? m_pUniformStream : GetUniformStream();
	float rsq, v1, v2;

	// Pick 2 random #s from -1 to 1
	// Make sure they lie inside the unit circle. If they don't, try again
	do
	{
		v1 = 2.0f * pUniformStream->RandomFloat() - 1.0f;
		v2 = 2.0f * pUniformStream->RandomFloat() - 1.0f;
		rsq = v1*v1 + v2*v2;
	} while ((rsq > 1.0f) || (rsq == 0.0f));

	// The box-muller transformation to get the two gaussian numbers
	const float fac = std::sqrt( -2.0f * std::log( rsq ) / rsq );

	// Store off one value for later use
	m_flRandomValue = v1 * fac;
	m_bHaveValue = true;

	return flStdDev * (v2 * fac) + flMean;
}