//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Fast low quality noise suitable for real time use
//
//=====================================================================================//

#include <float.h>	// Needed for FLT_EPSILON
#include <memory.h>
#include <cmath>

#include "tier0/basetypes.h"
#include "tier0/dbg.h"

#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "mathlib/ssemath.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#include "noisedata.h"


#define MAGIC_NUMBER (1<<15)								// gives 8 bits of fraction

static fltx4 Four_MagicNumbers = { MAGIC_NUMBER, MAGIC_NUMBER, MAGIC_NUMBER, MAGIC_NUMBER };


alignas(16) static int32 idx_mask[4] = {0xffff, 0xffff, 0xffff, 0xffff};

#define MASK255 (*((fltx4 *)(& idx_mask )))

// returns 0..1
static inline float GetLatticePointValue( int idx_x, int idx_y, int idx_z )
{
	NOTE_UNUSED(perm_d);
	NOTE_UNUSED(impulse_ycoords);
	NOTE_UNUSED(impulse_zcoords);

	int ret_idx = perm_a[idx_x & 0xff];
	ret_idx = perm_b[( idx_y + ret_idx ) & 0xff];
	ret_idx = perm_c[( idx_z + ret_idx ) & 0xff];
	return impulse_xcoords[ret_idx];

}

fltx4 XM_CALLCONV NoiseSIMD( DirectX::FXMVECTOR x, DirectX::FXMVECTOR y, DirectX::FXMVECTOR z )
{
	// use magic to convert to integer index
	fltx4 x_idx = AndSIMD( MASK255, AddSIMD( x, Four_MagicNumbers ) );
	fltx4 y_idx = AndSIMD( MASK255, AddSIMD( y, Four_MagicNumbers ) );
	fltx4 z_idx = AndSIMD( MASK255, AddSIMD( z, Four_MagicNumbers ) );

	fltx4 lattice000 = Four_Zeros, lattice001 = Four_Zeros, lattice010 = Four_Zeros, lattice011 = Four_Zeros;
	fltx4 lattice100 = Four_Zeros, lattice101 = Four_Zeros, lattice110 = Four_Zeros, lattice111 = Four_Zeros;

	// FIXME: Converting the input vectors to int indices will cause load-hit-stores (48 bytes)
	//        Converting the indexed noise values back to vectors will cause more (128 bytes)
	//        The noise table could store vectors if we chunked it into 2x2x2 blocks.
	fltx4 xfrac = Four_Zeros, yfrac = Four_Zeros, zfrac = Four_Zeros;
#define DOPASS(i)															\
    do {	unsigned int xi = SubInt( x_idx, i );								\
		unsigned int yi = SubInt( y_idx, i );								\
		unsigned int zi = SubInt( z_idx, i );								\
		SubFloat( xfrac, i ) = (xi & 0xff)*(1.0f/256.0f);						\
		SubFloat( yfrac, i ) = (yi & 0xff)*(1.0f/256.0f);						\
		SubFloat( zfrac, i ) = (zi & 0xff)*(1.0f/256.0f);						\
		xi>>=8;																\
		yi>>=8;																\
		zi>>=8;																\
																			\
		SubFloat( lattice000, i ) = GetLatticePointValue( xi,yi,zi );		\
		SubFloat( lattice001, i ) = GetLatticePointValue( xi,yi,zi+1 );		\
		SubFloat( lattice010, i ) = GetLatticePointValue( xi,yi+1,zi );		\
		SubFloat( lattice011, i ) = GetLatticePointValue( xi,yi+1,zi+1 );	\
		SubFloat( lattice100, i ) = GetLatticePointValue( xi+1,yi,zi );		\
		SubFloat( lattice101, i ) = GetLatticePointValue( xi+1,yi,zi+1 );	\
		SubFloat( lattice110, i ) = GetLatticePointValue( xi+1,yi+1,zi );	\
		SubFloat( lattice111, i ) = GetLatticePointValue( xi+1,yi+1,zi+1 );	\
    } while (false)

	DOPASS( 0 );
	DOPASS( 1 );
	DOPASS( 2 );
	DOPASS( 3 );

	// now, we have 8 lattice values for each of four points as m128s, and interpolant values for
	// each axis in m128 form in [xyz]frac. Perfom the trilinear interpolation as SIMD ops

	// first, do x interpolation
	fltx4 l2d00 = MaddSIMD( xfrac, SubSIMD( lattice100, lattice000 ), lattice000 );
	fltx4 l2d01 = MaddSIMD( xfrac, SubSIMD( lattice101, lattice001 ), lattice001 );
	fltx4 l2d10 = MaddSIMD( xfrac, SubSIMD( lattice110, lattice010 ), lattice010 );
	fltx4 l2d11 = MaddSIMD( xfrac, SubSIMD( lattice111, lattice011 ), lattice011 );

	// now, do y interpolation
	fltx4 l1d0 = MaddSIMD( yfrac, SubSIMD( l2d10, l2d00 ), l2d00 );
	fltx4 l1d1 = MaddSIMD( yfrac, SubSIMD( l2d11, l2d01 ), l2d01 );

	// final z interpolation
	fltx4 rslt = MaddSIMD( zfrac, SubSIMD( l1d1, l1d0 ), l1d0 );

	// map to 0..1
	return MulSIMD( Four_Twos, SubSIMD( rslt, Four_PointFives ) );


}

fltx4 XM_CALLCONV NoiseSIMD( FourVectors const &pos )
{
	return NoiseSIMD( pos.x, pos.y, pos.z );
}
