//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Create an output wave stream.  Used to record audio for in-engine movies or
// mixer debugging.
//
//=====================================================================================//

#include "audio_pch.h"

// dimhotepus: COM_CopyFile declaration
#include "common.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IFileSystem *g_pFileSystem;

// Create a wave file
bool WaveCreateTmpFile( const char *filename, int rate, int bits, int nChannels )
{
	char tmpfilename[MAX_PATH];
	V_StripExtension( filename, tmpfilename );
	V_DefaultExtension( tmpfilename, ".WAV" );

	FileHandle_t file = g_pFileSystem->Open( tmpfilename, "wb" );
	if ( !file )
		return false;

	RunCodeAtScopeExit(g_pFileSystem->Close(file));

	int chunkid = LittleLong( RIFF_ID );
	int chunksize = LittleLong( 0 );
	g_pFileSystem->Write( &chunkid, sizeof(int), file );
	g_pFileSystem->Write( &chunksize, sizeof(int), file );

	chunkid = LittleLong( RIFF_WAVE );
	g_pFileSystem->Write( &chunkid, sizeof(int), file );

	// create a 16-bit PCM stereo output file
	PCMWAVEFORMAT fmt = {};
	fmt.wf.wFormatTag = LittleWord( (short)WAVE_FORMAT_PCM );
	fmt.wf.nChannels = LittleWord( (short)nChannels );
	fmt.wf.nSamplesPerSec = LittleDWord( rate );
	fmt.wf.nAvgBytesPerSec = LittleDWord( rate * bits * nChannels / 8 );
	fmt.wf.nBlockAlign = LittleWord( (short)( 2 * nChannels) );
	fmt.wBitsPerSample = LittleWord( (short)bits );

	chunkid = LittleLong( WAVE_FMT );
	chunksize = LittleLong( sizeof(fmt) );
	g_pFileSystem->Write( &chunkid, sizeof(int), file );
	g_pFileSystem->Write( &chunksize, sizeof(int), file );
	g_pFileSystem->Write( &fmt, sizeof( PCMWAVEFORMAT ), file );

	chunkid = LittleLong( WAVE_DATA );
	chunksize = LittleLong( 0 );
	g_pFileSystem->Write( &chunkid, sizeof(int), file );
	g_pFileSystem->Write( &chunksize, sizeof(int), file );

	return true;
}

bool WaveAppendTmpFile( const char *filename, void *pBuffer, int sampleBits, int numSamples )
{
	char tmpfilename[MAX_PATH];
	V_StripExtension( filename, tmpfilename );
	V_DefaultExtension( tmpfilename, ".WAV" );

	FileHandle_t file = g_pFileSystem->Open( tmpfilename, "r+b" );
	if ( !file )
		return false;
	
	RunCodeAtScopeExit(g_pFileSystem->Close(file));

	g_pFileSystem->Seek( file, 0, FILESYSTEM_SEEK_TAIL );
	g_pFileSystem->Write( pBuffer, numSamples * sampleBits/8, file );

	return true;
}

bool WaveFixupTmpFile( const char *filename )
{
	char tmpfilename[MAX_PATH];
	V_StripExtension( filename, tmpfilename );
	V_DefaultExtension( tmpfilename, ".WAV" );

	FileHandle_t file = g_pFileSystem->Open( tmpfilename, "r+b" );
	if ( !file )
	{
		Warning( "WaveFixupTmpFile('%s') failed to open file for editing.\n", tmpfilename );
		return false;
	}
	
	RunCodeAtScopeExit(g_pFileSystem->Close(file));
	
	// file size goes in RIFF chunk
	int size = g_pFileSystem->Size( file ) - 2*sizeof( int );
	// offset to data chunk
	int headerSize = (sizeof(int)*5 + sizeof(PCMWAVEFORMAT));
	// size of data chunk
	int dataSize = size - headerSize;

	size = LittleLong( size );
	g_pFileSystem->Seek( file, sizeof( int ), FILESYSTEM_SEEK_HEAD );
	g_pFileSystem->Write( &size, sizeof( int ), file );

	// skip the header and the 4-byte chunk tag and write the size
	dataSize = LittleLong( dataSize );
	g_pFileSystem->Seek( file, headerSize+sizeof( int ), FILESYSTEM_SEEK_HEAD );
	g_pFileSystem->Write( &dataSize, sizeof( int ), file );

	return true;
}

CON_COMMAND( movie_fixwave, "Fixup corrupted .wav file if engine crashed during startmovie/endmovie, etc." )
{
	if ( args.ArgC() != 2 )
	{
		Msg ("Usage: movie_fixwave wavname\n");
		return;
	}

	char const *wavname = args.Arg( 1 );
	if ( !g_pFileSystem->FileExists( wavname ) )
	{
		Warning( "movie_fixwave: File '%s' does not exist\n", wavname );
		return;
	}

	char tmpfilename[256];
	V_StripExtension( wavname, tmpfilename );
	V_strcat_safe( tmpfilename, "_fixed" );
	V_DefaultExtension( tmpfilename, ".wav" );

	// Now copy the file
	Msg( "Copying '%s' to '%s'\n", wavname, tmpfilename );
	COM_CopyFile( wavname, tmpfilename );

	Msg( "Performing fixup on '%s'\n", tmpfilename );
	WaveFixupTmpFile( tmpfilename );
}
