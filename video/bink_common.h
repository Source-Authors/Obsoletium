//========= Copyright Valve Corporation, All rights reserved. ============//
//
// File: bink_common.h
//
//  Bink limits and constants shared among all Bink functions
//
//=============================================================================


#ifndef BINK_COMMON_H
#define BINK_COMMON_H

#ifdef _WIN32
#pragma once
#endif

// constant that define the bounds of various inputs
static constexpr inline int	cMinVideoFrameWidth = 16;
static constexpr inline int	cMinVideoFrameHeight = 16;
static constexpr inline int	cMaxVideoFrameWidth = 2 * 2048;
static constexpr inline int	cMaxVideoFrameHeight = 2 * 2048;

static constexpr inline int	cMinFPS = 1;
static constexpr inline int	cMaxFPS = 600;

static constexpr inline float  cMinDuration = 0.016666666f;		// 1/60th second
static constexpr inline float  cMaxDuration = 3600.0f;				// 1 Hour

static constexpr inline int	cMinSampleRate = 11025;				// 1/4 CD sample rate
static constexpr inline int	cMaxSampleRate = 88200;				// 2x CD rate

#define NO_MORE_INTERESTING_TIMES   -2

extern char *COPY_STRING( const char *pString );

//-----------------------------------------------------------------------------
// Computes a power of two at least as big as the passed-in number
//-----------------------------------------------------------------------------
constexpr inline int ComputeGreaterPowerOfTwo( int n )
{
	int i = 1;
	while ( i < n )
	{
		i <<= 1;
	}
	return i;
}

#endif	// BINK_COMMON_H
