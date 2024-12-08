//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "client_pch.h"

#include "cbenchmark.h"

#include "tier0/vcrmode.h"
#include "tier1/KeyValues.h"
#include "vstdlib/random.h"

#include "filesystem_engine.h"
#include "sys.h"
#include "sv_uploaddata.h"
#include "FindSteamServers.h"
#include "cl_steamauth.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

constexpr inline char DEFAULT_RESULTS_FOLDER[]{"results"};
constexpr inline char DEFAULT_RESULTS_FILENAME[]{"results.txt"};

CBenchmarkResults g_BenchmarkResults;
extern ConVar host_framerate;
extern void GetMaterialSystemConfigForBenchmarkUpload(KeyValues *dataToUpload);

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBenchmarkResults::CBenchmarkResults()
{
	m_bIsTestRunning = false;
	m_szFilename[0] = 0;
	m_flStartTime = -1;
	m_iStartFrame = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBenchmarkResults::IsBenchmarkRunning() const
{
	return m_bIsTestRunning;
}
	
//-----------------------------------------------------------------------------
// Purpose: starts recording data
//-----------------------------------------------------------------------------
void CBenchmarkResults::StartBenchmark( const CCommand &args )
{
	const char *pszFilename = DEFAULT_RESULTS_FILENAME;
	
	if ( args.ArgC() > 1 )
	{
		pszFilename = args[1];
	}

	// check path first
	if ( !COM_IsValidPath( pszFilename ) )
	{
		ConMsg( "bench_start %s: invalid path.\n", pszFilename );
		return;
	}

	m_bIsTestRunning = true;

	SetResultsFilename( pszFilename );

	// set any necessary settings
	host_framerate.SetValue( 1.0f / host_state.interval_per_tick );

	// get the current frame and time
	m_iStartFrame = host_framecount;
	m_flStartTime = realtime;
}
	
//-----------------------------------------------------------------------------
// Purpose: writes out results to file
//-----------------------------------------------------------------------------
void CBenchmarkResults::StopBenchmark()
{
	m_bIsTestRunning = false;

	// reset
	host_framerate.SetValue( 0 );

	// print out some stats
	const int numticks = host_framecount - m_iStartFrame;
	const float averageFramerate = static_cast<float>(numticks / ( realtime - m_flStartTime ));
	Msg( "Average framerate: %.2f\n", averageFramerate );
	
	// work out where to write the file
	g_pFileSystem->CreateDirHierarchy( DEFAULT_RESULTS_FOLDER, "MOD" );
	char szFilename[256];
	V_sprintf_safe( szFilename, "%s\\%s", DEFAULT_RESULTS_FOLDER, m_szFilename );

	// write out the data as keyvalues
	auto kv = KeyValues::AutoDelete( "benchmark" );
	kv->SetFloat( "framerate", averageFramerate );
	kv->SetInt( "build", build_number() );

	// get material system info
	GetMaterialSystemConfigForBenchmarkUpload( kv );

	// save
	kv->SaveToFile( g_pFileSystem, szFilename, "MOD" );
}

//-----------------------------------------------------------------------------
// Purpose: Sets which file the results will be written to
//-----------------------------------------------------------------------------
void CBenchmarkResults::SetResultsFilename( const char *pFilename )
{
	Q_strncpy( m_szFilename, pFilename, sizeof( m_szFilename ) );
	Q_DefaultExtension( m_szFilename, ".txt", sizeof( m_szFilename ) );
}

//-----------------------------------------------------------------------------
// Purpose: uploads the most recent results to Steam
//-----------------------------------------------------------------------------
void CBenchmarkResults::Upload()
{
#ifndef SWDS
	// dimhotepus: Notify when nothing to upload.
	if ( !m_szFilename[0] )
	{
		Warning("No benchmark results to upload. Please use bench_start <file_name> and bench_end.\n");
		return;
	}
	
	// dimhotepus: Notify when no Steam.
	if (!Steam3Client().SteamUtils())
	{
		Warning("Steam connection required. Please login into Steam.\n");
		return;
	}

	uint32 cserIP = 0;
	uint16 cserPort = 0;
	uint32 remainingAttempts = 50;
	while ( cserIP == 0 && remainingAttempts-- > 0 )
	{
		Steam3Client().SteamUtils()->GetCSERIPPort( &cserIP, &cserPort );
		if ( !cserIP )
			ThreadSleep( 10 );
	}

	// dimhotepus: If no reporting server IP, exit.
	if ( !cserIP )
	{
		Warning("Unable to upload benchmark results %s. Steam reporting server is not available.\n", m_szFilename);
		return;
	}

	netadr_t netadr_CserIP( cserIP, cserPort );
	// upload
	char szFilename[256];
	V_sprintf_safe( szFilename, "%s\\%s", DEFAULT_RESULTS_FOLDER, m_szFilename );

	auto kv = KeyValues::AutoDelete( "benchmark" );
	if ( kv->LoadFromFile( g_pFileSystem, szFilename, "MOD" ) )
	{
		// this sends the data to the Steam CSER
		if ( !UploadData( netadr_CserIP.ToString(), "benchmark", kv ) )
		{
			// dimhotepus: Notify when Steam reporting server fails.
			Warning("Unable to upload benchmark results %s. Steam reporting server %s failed.\n",
				m_szFilename, netadr_CserIP.ToString());
		}
	}
#endif
}

CON_COMMAND_F( bench_start, "Starts gathering of info. Arguments: filename to write results into", FCVAR_CHEAT )
{
	GetBenchResultsMgr()->StartBenchmark( args );
}

CON_COMMAND_F( bench_end, "Ends gathering of info.", FCVAR_CHEAT )
{
	// dimhotepus: Warn if bench is not started.
	if ( GetBenchResultsMgr()->IsBenchmarkRunning() )
	{
		GetBenchResultsMgr()->StopBenchmark();
	}
	else
	{
		Warning("Benchmark is not running. Please, use bench_start <file_name> to start.\n");
	}
}

CON_COMMAND_F( bench_upload, "Uploads most recent benchmark stats to the Valve servers.", FCVAR_CHEAT )
{
	GetBenchResultsMgr()->Upload();
}

