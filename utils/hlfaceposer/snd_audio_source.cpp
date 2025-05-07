//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "snd_audio_source.h"



extern CAudioSource *Audio_CreateMemoryWave( const char *pName );

//-----------------------------------------------------------------------------
// Purpose: Simple wrapper to crack naming convention and create the proper wave source
// Input  : *pName - WAVE filename
// Output : CAudioSource
//-----------------------------------------------------------------------------
CAudioSource *AudioSource_Create( const char *pName )
{
	if ( !pName )
		return nullptr;

//	if ( pName[0] == '!' )		// sentence
		;

	// Names that begin with "*" are streaming.
	// Skip over the * and create a streamed source
	if ( pName[0] == '*' )
	{

		return nullptr;
	}

	// These are loaded into memory directly
	return Audio_CreateMemoryWave( pName );
}

#include "hlfaceposer.h"
#include "ifaceposersound.h"

CAudioSource::~CAudioSource( void )
{
	CAudioMixer *mixer;
	
	while ( 1 )
	{
		mixer = sound->FindMixer( this );
		if ( !mixer )
			break;

		sound->StopSound( mixer );
	}

	sound->EnsureNoModelReferences( this );
}

CAudioSource::CAudioSource( void )
{
}
