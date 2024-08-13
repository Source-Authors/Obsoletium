//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "tier0/platform.h"
#include "mpaheaderinfo/stdafx.h"
#include "mpaheaderinfo/MPAFile.h"
#include "filesystem.h"
#include "soundchars.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlrbtree.h"

#include "memdbgon.h"

extern IFileSystem *g_pFullFileSystem;

struct MP3Duration_t
{
    FileNameHandle_t h;
    float           duration;

    static bool LessFunc( const MP3Duration_t& lhs, const MP3Duration_t& rhs )
    {
        return lhs.h < rhs.h;
    }
};

CUtlRBTree< MP3Duration_t, int >    g_MP3Durations( 0, 0, MP3Duration_t::LessFunc );

float GetMP3Duration_Helper( char const *filename )
{
	float duration = 60.0f;

	// See if it's in the RB tree already...
	char fn[ 512 ];
	Q_snprintf( fn, sizeof( fn ), "sound/%s", PSkipSoundChars( filename ) );

	MP3Duration_t search = {};
	search.h = g_pFullFileSystem->FindOrAddFileName( fn );
	
	auto idx = g_MP3Durations.Find( search );
	if ( idx != g_MP3Durations.InvalidIndex() )
	{
		return g_MP3Durations[ idx ].duration;
	}

	try
	{
		CMPAFile MPAFile( fn );
		duration = (float)MPAFile.GetLengthSec();
	}
	catch ( std::exception &e )
	{
		// dimhotepus: Dump exception info.
		Warning( "Unable to get %s file length: %s.", filename, e.what() );
	}

	search.duration = duration;
	g_MP3Durations.Insert( search );

	return duration;
}
