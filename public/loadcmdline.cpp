//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: loads additional command line options from a config file
//
// $NoKeywords: $
//=============================================================================//

#include "KeyValues.h"
#include "tier1/strtools.h"
#include "FileSystem_Tools.h"
#include "tier1/utlstring.h"

// So we know whether or not we own argv's memory
static bool sFoundConfigArgs = false;

//-----------------------------------------------------------------------------
// Purpose: Parses arguments and adds them to argv and argc
//-----------------------------------------------------------------------------
static void AddArguments( int &argc, char **&argv, const char *str )
{
	char  **args	 = 0;
	char   *argList	 = 0;
	int		argCt	 = argc;

	argList = V_strdup( str );

	// Parse the arguments out of the string
	char *token = strtok( argList, " " );
	while( token )
	{
		++argCt;
		token = strtok( NULL, " " );
	}

	// Make sure someting was actually found in the file
	if( argCt > argc )
	{
		sFoundConfigArgs = true;

		// Allocate a new array for argv
		args = new char*[ argCt ];

		// Copy original arguments, up to the last one
		int i;
		for( i = 0; i < argc - 1; ++i )
		{
			args[ i ] = V_strdup( argv[ i ] );
		}

		// copy new arguments
		Q_strcpy( argList, str );
		token = strtok( argList, " " );
		for( ; i < argCt - 1; ++i )
		{
			args[ i ] = V_strdup( token );
			token = strtok( NULL, " " );
		}

		// Copy the last original argument
		args[ i ] = V_strdup( argv[ argc - 1 ] );

		argc = argCt;
		argv = args;
	}

	delete [] argList;
}

//-----------------------------------------------------------------------------
// Purpose: Loads additional commandline arguments from a config file for an app.
//			Filesystem must be initialized before calling this function.
// keyname: Name of the block containing the key/args pairs (ie map or model name)
// appname: Keyname for the commandline arguments to be loaded - typically the exe name.
//-----------------------------------------------------------------------------
void LoadCmdLineFromFile( int &argc, char **&argv, const char *keyname, const char *appname )
{
	sFoundConfigArgs = false;

	Assert( g_pFileSystem );
	if( !g_pFileSystem )
		return;

	char filename[MAX_PATH];
	// dimhotepus: Game dir already ends with /, no need to add 2 one.
	V_sprintf_safe( filename, "%scfg" CORRECT_PATH_SEPARATOR_S "commandline.cfg", gamedir );
	V_FixSlashes(filename);
	
	// Load the cfg file, and find the keyname
	auto kv = KeyValues::AutoDelete( "CommandLine" );
	if ( kv->LoadFromFile( g_pFileSystem, filename ) )
	{
		// Load the commandline arguments for this app
		KeyValues *appKey = kv->FindKey( keyname );
		if ( appKey )
		{
			const char *str	= appKey->GetString( appname );
			Msg( "Command Line for app %s found: %s\n", appname, str );

			AddArguments( argc, argv, str );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Cleans up any memory allocated for the new argv.  Pass in the app's
// argc and argv - this is safe even if no extra arguments were loaded.
//-----------------------------------------------------------------------------
void DeleteCmdLine( int argc, char **argv )
{
	if( !sFoundConfigArgs )
		return;

	for( int i = 0; i < argc; ++i )
	{
		delete [] argv[i];
	}
	delete [] argv;
}
