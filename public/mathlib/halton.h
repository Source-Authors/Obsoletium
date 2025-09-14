//========= Copyright Valve Corporation, All rights reserved. ============//
// $Id$

// halton.h - classes, etc for generating numbers using the Halton pseudo-random sequence.  See
// https://en.wikipedia.org/wiki/Halton_sequence.
//
// what this function is useful for is any sort of sampling/integration problem where
// you want to solve it by random sampling. Each call the NextValue() generates
// a random number between 0 and 1, in an unclumped manner, so that the space can be more
// or less evenly sampled with a minimum number of samples.
//
// It is NOT useful for generating random numbers dynamically, since the outputs aren't
// particularly random.
//
// To generate multidimensional sample values (points in a plane, etc), use two
// HaltonSequenceGenerator_t's, with different (primes) bases.

#ifndef HALTON_H
#define HALTON_H

#include "tier0/platform.h"
#include "vector.h"

#include <DirectXMath.h>

class HaltonSequenceGenerator_t
{
	int seed;
	int base;
	float fbase;											//< base as a float

public:
	HaltonSequenceGenerator_t(int base);					//< base MUST be prime, >=2

	[[nodiscard]] float GetElement(int element);

	[[nodiscard]] inline float NextValue()
	{
		return GetElement(seed++);
	}

};


class DirectionalSampler_t									//< pseudo-random sphere sampling
{
	HaltonSequenceGenerator_t zdot;
	HaltonSequenceGenerator_t vrot;
public:
	DirectionalSampler_t()
		: zdot(2),vrot(3)
	{
	}

	[[nodiscard]] Vector NextValue()
	{
		float zvalue=zdot.NextValue();
		zvalue=2*zvalue-1.0f;								// map from 0..1 to -1..1
		float phi=acosf(zvalue);
		// now, generate a random rotation angle for x/y
		float theta=2.0f*M_PI_F*vrot.NextValue();

		float sinTheta, cosTheta;
		DirectX::XMScalarSinCos(&sinTheta, &cosTheta, theta);

		const float sinPhi = sinf(phi);
		return {cosTheta * sinPhi, sinTheta * sinPhi, zvalue};
	}
};




#endif // halton_h
