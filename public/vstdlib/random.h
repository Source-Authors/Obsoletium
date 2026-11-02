//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Random number generator
//
// $Workfile: $
// $NoKeywords: $
//===========================================================================//

#ifndef VSTDLIB_RANDOM_H
#define VSTDLIB_RANDOM_H

#include "vstdlib/vstdlib.h"
#include "tier0/basetypes.h"
#include "tier0/threadtools.h"
#include "tier1/interface.h"

//-----------------------------------------------------------------------------
// A generator of uniformly distributed random numbers
//-----------------------------------------------------------------------------
class VSTDLIB_CLASS IUniformRandomStream
{
public:
	// Sets the seed of the random number generator
	virtual void	SetSeed( int iSeed ) = 0;

	// Generates random numbers
	virtual float	RandomFloat( float flMinVal = 0.0f, float flMaxVal = 1.0f ) = 0;
	virtual int		RandomInt( int iMinVal, int iMaxVal ) = 0;
	virtual float	RandomFloatExp( float flMinVal = 0.0f, float flMaxVal = 1.0f, float flExponent = 1.0f ) = 0;

#ifdef PLATFORM_64BITS
	// dimhotepus: int64 support.
	virtual int64	RandomInt64( int64 iMinVal, int64 iMaxVal ) = 0;
#endif

	// dimhotepus: CPU arch word support.
	inline intp RandomIntp( intp minVal, intp maxVal )
	{
#ifdef PLATFORM_64BITS
		return this->RandomInt64( minVal, maxVal );
#else
		return this->RandomInt( static_cast<int>( minVal ), static_cast<int>( maxVal ) );
#endif
	}

protected:
	// dimhotepus: Non-virtual to keep the vtable layout unchanged, protected so that
	// deleting a stream through an interface pointer (UB) fails to compile.
	~IUniformRandomStream() = default;
};


//-----------------------------------------------------------------------------
// The standard generator of uniformly distributed random numbers.
// dimhotepus: Park-Miller minimal standard LCG with Bays-Durham shuffle (ran1).
// Thread-safe. Seeded sequences are only reproducible when one thread uses
// the stream.
//-----------------------------------------------------------------------------
class VSTDLIB_CLASS CUniformRandomStream : public IUniformRandomStream
{
public:
	// dimhotepus: Size of the shuffle table. Must be a power of two.
	enum : int { SHUFFLE_TABLE_SIZE = 32 };

	CUniformRandomStream();
	virtual ~CUniformRandomStream() = default;

	// Sets the seed of the random number generator
	void	SetSeed( int iSeed ) override;

	// Generates random numbers
	float	RandomFloat( float flMinVal = 0.0f, float flMaxVal = 1.0f ) override;
	int		RandomInt( int iMinVal, int iMaxVal ) override;
	float	RandomFloatExp( float flMinVal = 0.0f, float flMaxVal = 1.0f, float flExponent = 1.0f ) override;

#ifdef PLATFORM_64BITS
	// dimhotepus: int64 support.
	int64	RandomInt64( int64 iMinVal, int64 iMaxVal ) override;
#endif

private:
	// dimhotepus: All Generate* helpers require m_mutex to be held by the caller.

	// dimhotepus: Returns a value in [1, 2^31 - 2].
	int		GenerateRandomNumber();
	// dimhotepus: Returns a uniform value in [0, uRange), 2 <= uRange <= 2^31.
	uint32	GenerateRandomBelow( uint32 uRange );

#ifdef PLATFORM_64BITS
	// dimhotepus: int64 support. Returns a uniform value in [0, 2^63).
	uint64	GenerateRandom63Bits();
#endif

	int m_idum;
	int m_iy;
	int m_iv[SHUFFLE_TABLE_SIZE];

MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()
// DLL export looks safe.
MSVC_DISABLE_WARNING(4251)
	CThreadFastMutex m_mutex;
MSVC_END_WARNING_OVERRIDE_SCOPE()
};


//-----------------------------------------------------------------------------
// A generator of gaussian distributed random numbers
//-----------------------------------------------------------------------------
class VSTDLIB_CLASS CGaussianRandomStream
{
public:
	// Passing in NULL will cause the gaussian stream to use the
	// installed global random number generator
	CGaussianRandomStream( IUniformRandomStream *pUniformStream = nullptr );

	// Attaches to a random uniform stream
	void	AttachToStream( IUniformRandomStream *pUniformStream = nullptr );

	// Generates random numbers
	float	RandomFloat( float flMean = 0.0f, float flStdDev = 1.0f );

private:
	IUniformRandomStream *m_pUniformStream;
	bool	m_bHaveValue;
	float	m_flRandomValue;

MSVC_BEGIN_WARNING_OVERRIDE_SCOPE()
// DLL export looks safe.
MSVC_DISABLE_WARNING(4251)
	CThreadFastMutex m_mutex;
MSVC_END_WARNING_OVERRIDE_SCOPE()
};

//-----------------------------------------------------------------------------
// A couple of convenience functions to access the library's global uniform stream
//-----------------------------------------------------------------------------
VSTDLIB_INTERFACE void	RandomSeed( int iSeed );
VSTDLIB_INTERFACE float	RandomFloat( float flMinVal = 0.0f, float flMaxVal = 1.0f );
VSTDLIB_INTERFACE float	RandomFloatExp( float flMinVal = 0.0f, float flMaxVal = 1.0f, float flExponent = 1.0f );
VSTDLIB_INTERFACE int	RandomInt( int iMinVal, int iMaxVal );

// dimhotepus: int64 support.
#ifdef PLATFORM_64BITS
VSTDLIB_INTERFACE int64	RandomInt64( int64 iMinVal, int64 iMaxVal );
#endif

// dimhotepus: CPU arch word support.
// dimhotepus: For ranges that fit in int, returns the same values on 32 and 64 bit builds.
inline intp RandomIntp( intp minVal, intp maxVal )
{
#ifdef PLATFORM_64BITS
	return RandomInt64( minVal, maxVal );
#else
	return RandomInt( static_cast<int>( minVal ), static_cast<int>( maxVal ) );
#endif
}

VSTDLIB_INTERFACE float	RandomGaussianFloat( float flMean = 0.0f, float flStdDev = 1.0f );

//-----------------------------------------------------------------------------
// IUniformRandomStream interface for free functions
//-----------------------------------------------------------------------------
class VSTDLIB_CLASS CDefaultUniformRandomStream : public IUniformRandomStream
{
public:
	virtual ~CDefaultUniformRandomStream() = default;
	void	SetSeed( int iSeed ) override												{ RandomSeed( iSeed ); }
	float	RandomFloat( float flMinVal, float flMaxVal ) override						{ return ::RandomFloat( flMinVal, flMaxVal ); }
	int		RandomInt( int iMinVal, int iMaxVal ) override								{ return ::RandomInt( iMinVal, iMaxVal ); }
	float	RandomFloatExp( float flMinVal, float flMaxVal, float flExponent ) override	{ return ::RandomFloatExp( flMinVal, flMaxVal, flExponent ); }

#ifdef PLATFORM_64BITS
	// dimhotepus: int64 support.
	int64   RandomInt64( int64 iMinVal, int64 iMaxVal ) override							{ return ::RandomInt64( iMinVal, iMaxVal ); }
#endif
};

//-----------------------------------------------------------------------------
// Installs a global random number generator, which will affect the Random functions above.
// dimhotepus: Thread-safe. The installed stream must outlive every call that may still use it.
//-----------------------------------------------------------------------------
VSTDLIB_INTERFACE void	InstallUniformRandomStream( IUniformRandomStream *pStream );


#endif // VSTDLIB_RANDOM_H