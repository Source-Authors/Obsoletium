//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: perform initialization needed in most command line programs
//
//===================================================================================-=//

#include <tier2/tier2.h>
#include <tier0/platform.h>
#include <mathlib/mathlib.h>
#include <tier0/icommandline.h>
#include "tier0/memalloc.h"
#include "tier0/progressbar.h"
#include "tier1/strtools.h"

namespace {

void PrintFReportHandler(char const *job_name, int total_units_to_do, int n_units_completed)
{
	static bool work_in_progress=false;
	static char LastJobName[1024];
	if ( Q_strncmp( LastJobName, job_name, sizeof( LastJobName ) ) )
	{
		if ( work_in_progress )
			Msg("..done\n");
		V_strcpy_safe( LastJobName, job_name );
	}
 	if ( (total_units_to_do > 0 ) && (total_units_to_do >= n_units_completed) )
	{
		int percent_done=(100*n_units_completed)/total_units_to_do;
		Msg("\r%s : %d%%",LastJobName, percent_done );
		work_in_progress = true;
	}
	else
	{
		Msg("%s\n",LastJobName);
		work_in_progress = false;
	}
}

ProgressReportHandler_t g_progress_report = nullptr;
SpewOutputFunc_t g_spew_output = nullptr;

}

void InitCommandLineProgram( int argc, char **argv )
{
	MathLib_Init( 1,1,1,0,false,true,true,true);

	CommandLine()->CreateCmdLine( argc, argv );

	InitDefaultFileSystem();

	g_progress_report = InstallProgressReportHandler( PrintFReportHandler );

	// By default, command line programs should not use the new assert dialog,
	// and any asserts should be fatal, unless we are being debugged
	if ( !Plat_IsInDebugSession() )
		g_spew_output = SpewOutputFunc2( DefaultSpewFuncAbortOnAsserts );
}

void ShutdownCommandLineProgram()
{
	if ( !Plat_IsInDebugSession() )
	{
		g_spew_output = SpewOutputFunc2( g_spew_output );
		Assert( g_spew_output == DefaultSpewFuncAbortOnAsserts );
	}

	g_progress_report = InstallProgressReportHandler( g_progress_report );
	Assert( g_progress_report == PrintFReportHandler );

	ShutdownDefaultFileSystem();
}
