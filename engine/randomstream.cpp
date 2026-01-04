//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: random steam class
//
//===========================================================================//

#include <tier0/dbg.h>
#include <vstdlib/random.h>
#include "cdll_int.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
//
// implementation of IUniformRandomStream
//
//-----------------------------------------------------------------------------
class CEngineUniformRandomStream final : public IUniformRandomStream
{
public:
	// Sets the seed of the random number generator
	void	SetSeed( int iSeed ) override
	{
		// Never call this from the client or game!
		Assert(0);
	}

	// Generates random numbers
	float	RandomFloat( float flMinVal = 0.0f, float flMaxVal = 1.0f ) override
	{
		return ::RandomFloat( flMinVal, flMaxVal );
	}

	float	RandomFloatExp( float flMinVal = 0.0f, float flMaxVal = 1.0f, float flExponent = 1.0f ) override
	{
		return ::RandomFloatExp( flMinVal, flMaxVal, flExponent );
	}

	int		RandomInt( int iMinVal, int iMaxVal ) override
	{
		return ::RandomInt( iMinVal, iMaxVal );
	}
	
#ifdef PLATFORM_64BITS
	// dimhotepus: int64 support.
	int64	RandomInt64( int64 iMinVal, int64 iMaxVal ) override
	{
		return ::RandomInt64( iMinVal, iMaxVal );
	}
#endif
};

static CEngineUniformRandomStream s_EngineRandomStream;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEngineUniformRandomStream, IUniformRandomStream, 
	VENGINE_CLIENT_RANDOM_INTERFACE_VERSION, s_EngineRandomStream );
