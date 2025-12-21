//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include <cstdlib>
#include <cstdio>
#include <cmath>

#include "vstdlib/random.h"

int main( int argc, char **argv )
{
	if( argc != 2 )
	{
		fprintf( stderr, "Usage: genrandomvectors <numvects>\n" );
		return EINVAL;
	}

	int nVectors = atoi( argv[1] );
	RandomSeed( 0 );

	for( int i = 0; i < nVectors; i++ )
	{
		float z = RandomFloat( -1.0f, 1.0f );
		float t = RandomFloat( 0, 2 * M_PI_F );
		float r = sqrt( 1.0f - z * z );
		float x = r * cos( t );
		float y = r * sin( t );
		printf( "Vector{ %f, %f, %f },\n", x, y, z );
	}

	return 0;
}
