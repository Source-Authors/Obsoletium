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

constexpr inline int NTAB{32}; //-V112

//-----------------------------------------------------------------------------
// A generator of uniformly distributed random numbers
//-----------------------------------------------------------------------------
class VSTDLIB_CLASS IUniformRandomStream
{
public:
	//virtual ~IUniformRandomStream() { }

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
		return static_cast<int>( this->RandomInt( static_cast<int>( minVal ), static_cast<int>( maxVal ) ) );
#endif
	}
};


//-----------------------------------------------------------------------------
// The standard generator of uniformly distributed random numbers
//-----------------------------------------------------------------------------
class VSTDLIB_CLASS CUniformRandomStream : public IUniformRandomStream
{
public:
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
	int		GenerateRandomNumber();

#ifdef PLATFORM_64BITS
	// dimhotepus: int64 support.
	int64	GenerateRandomNumber64();
#endif

	int m_idum;
	int m_iy;
	int m_iv[NTAB];

#ifdef PLATFORM_64BITS
	// dimhotepus: int64 support.
	int64 m_idum64;
	int64 m_iy64;
	int64 m_iv64[NTAB];
#endif

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
inline intp RandomIntp( intp minVal, intp maxVal )
{
#ifdef PLATFORM_64BITS
	return RandomInt64( minVal, maxVal );
#else
	return static_cast<int>( RandomInt( static_cast<int>( minVal ), static_cast<int>( maxVal ) ) );
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
// Installs a global random number generator, which will affect the Random functions above
//-----------------------------------------------------------------------------
VSTDLIB_INTERFACE void	InstallUniformRandomStream( IUniformRandomStream *pStream );


#endif // VSTDLIB_RANDOM_H
