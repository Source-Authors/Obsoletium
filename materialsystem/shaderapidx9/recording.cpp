//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "recording.h"

#include "shaderapi/ishaderutil.h"
#include "shaderapidx8_global.h"

#include "materialsystem/imaterialsystem.h"

#include "togl/rendermechanism.h"
#include "tier1/utlvector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef RECORDING

namespace {

CUtlVector<unsigned char> g_pRecordingBuffer;
unsigned char g_ArgsRemaining = 0;
intp g_CommandStartIdx = 0;

// set this to true in the debugger to actually record commands.
bool g_bDoRecord = true;

// Opens the recording file
FILE* OpenRecordingFile()
{
#ifdef CRASH_RECORDING
	static FILE *fp = 0;
#else
	FILE* fp = 0;
#endif
	static bool g_CantOpenFile = false;
	static bool g_NeverOpened = true;
	if (!g_CantOpenFile)
	{
#ifdef PLATFORM_64BITS
		constexpr char kRecordingFileName[]{"shaderdx8_x86-64.rec"};
#else
		constexpr char kRecordingFileName[]{"shaderdx8_x86.rec"};
#endif

#ifdef CRASH_RECORDING
		if( g_NeverOpened )
		{
			fp = fopen( kRecordingFileName, "wbc" );
		}
#else
		fp = fopen( kRecordingFileName, g_NeverOpened ? "wb" : "ab" );
#endif
		if (!fp)
		{
			Warning( "Unable to open recording file %s!\n", kRecordingFileName );
			g_CantOpenFile = true;
		}
		g_NeverOpened = false;
	}
	return fp;
}

// Writes to the recording file
void WriteRecordingFile()
{
	constexpr intp COMMAND_BUFFER_SIZE = 32768;

	// Store the command size
	*(intp*)&g_pRecordingBuffer[g_CommandStartIdx] = 
		g_pRecordingBuffer.Count() - g_CommandStartIdx;

#ifndef CRASH_RECORDING
	// When not crash recording, flush when buffer gets too big, 
	// or when Present() is called
	if ((g_pRecordingBuffer.Count() < COMMAND_BUFFER_SIZE) &&
		(g_pRecordingBuffer[g_CommandStartIdx+4] != DX8_PRESENT))
		return;
#endif

	FILE* fp = OpenRecordingFile();
	if (fp)
	{
		// store the command size
		fwrite( g_pRecordingBuffer.Base(), 1, g_pRecordingBuffer.Count(), fp );
		fflush( fp );
#ifndef CRASH_RECORDING
		fclose( fp );
#endif
	}

	g_pRecordingBuffer.RemoveAll();
}

}

// Write the buffered crap out on shutdown.
void FinishRecording()
{
#ifndef CRASH_RECORDING
	FILE* fp = OpenRecordingFile();
	if (fp)
	{
		// store the command size
		fwrite( g_pRecordingBuffer.Base(), 1, g_pRecordingBuffer.Count(), fp );
		fflush( fp );
	}

	g_pRecordingBuffer.RemoveAll();
#endif
}

// Records a command
void RecordCommand( RecordingCommands_t cmd, unsigned char numargs )
{
	if( !g_bDoRecord )
	{
		return;
	}
	Assert( g_ArgsRemaining == 0 );

	g_CommandStartIdx = g_pRecordingBuffer.AddMultipleToTail( 6 );

	// save space for the total command size
	g_pRecordingBuffer[g_CommandStartIdx+4] = cmd;
	g_pRecordingBuffer[g_CommandStartIdx+5] = numargs;
	g_ArgsRemaining = numargs;
	if (g_ArgsRemaining == 0)
		WriteRecordingFile();
}

// Records an argument for a command, flushes when the command is done
void RecordArgument( const void * pMemory, intp size )
{
	if( !g_bDoRecord )
	{
		return;
	}
	Assert( g_ArgsRemaining > 0 );

	const intp tail = g_pRecordingBuffer.Count();
	g_pRecordingBuffer.AddMultipleToTail( size );
	memcpy( &g_pRecordingBuffer[tail], pMemory, size );

	if (--g_ArgsRemaining == 0)
		WriteRecordingFile();
}

#endif // RECORDING
