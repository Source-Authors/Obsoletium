//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//


#if defined(_WIN32) && !defined(_X360)
#include "winlite.h"
#include <system_error>
#endif
#if defined(LINUX)
#include <unistd.h>
#include <fcntl.h>
#endif

#if defined( USE_SDL )
#include "include/SDL3/SDL.h"
#endif

#include "quakedef.h"
#include "igame.h"
#include "errno.h"
#include "host.h"
#include "profiling.h"
#include "server.h"
#include "vengineserver_impl.h"
#include "filesystem_engine.h"
#include "sys.h"
#include "sys_dll.h"
#include "ivideomode.h"
#include "host_cmd.h"
#include "crtmemdebug.h"
#include "sv_log.h"
#include "sv_main.h"
#include "traceinit.h"
#include "dt_test.h"
#include "keys.h"
#include "gl_matsysiface.h"
#include "tier0/vcrmode.h"
#include "tier0/icommandline.h"
#include "cmd.h"
#include <ihltvdirector.h>
#if defined( REPLAY_ENABLED )
#include "replay/ireplaysystem.h"
#endif
#include "MapReslistGenerator.h"
#include "DevShotGenerator.h"
#include "cdll_engine_int.h"
#include "dt_send.h"
#include "idedicatedexports.h"
#include "eifacev21.h"
#include "cl_steamauth.h"
#include "tier0/etwprof.h"
#include "tier2/tier2.h"

// dimhotepus: NO_STEAM
#ifdef NO_STEAM
#include "tier0/minidump.h"
#endif

#include "vgui_baseui_interface.h"
#include "tier0/systeminformation.h"
#ifdef _WIN32
#include <io.h>
#endif
#include "toolframework/itoolframework.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ONE_HUNDRED_TWENTY_EIGHT_MB	(128 * 1024 * 1024)

ConVar mem_min_heapsize( "mem_min_heapsize",
#ifdef PLATFORM_64BITS	
	"96"
#else
	"48"
#endif
	, FCVAR_INTERNAL_USE, "Minimum amount of memory to dedicate to engine hunk and datacache (in MiB)" );
ConVar mem_max_heapsize( "mem_max_heapsize",
#ifdef PLATFORM_64BITS	
	"512"
#else
	"256"
#endif
	, FCVAR_INTERNAL_USE, "Maximum amount of memory to dedicate to engine hunk and datacache (in MiB)" );
ConVar mem_max_heapsize_dedicated( "mem_max_heapsize_dedicated", "64", FCVAR_INTERNAL_USE, "Maximum amount of memory to dedicate to engine hunk and datacache, for dedicated server (in MiB)" );

#define MINIMUM_WIN_MEMORY			(size_t)(mem_min_heapsize.GetInt()*1024*1024)
#define MAXIMUM_WIN_MEMORY			max( (size_t)(mem_max_heapsize.GetInt()*1024*1024), MINIMUM_WIN_MEMORY )
#define MAXIMUM_DEDICATED_MEMORY	(size_t)(mem_max_heapsize_dedicated.GetInt()*1024*1024)


char *CheckParm(const char *psz, char **ppszValue = NULL);
void SeedRandomNumberGenerator( bool random_invariant );
void Con_ColorPrintf( const Color& clr, PRINTF_FORMAT_STRING const char *fmt, ... ) FMTFUNCTION( 2, 3 );

void COM_ShutdownFileSystem( void );
void COM_InitFilesystem( const char *pFullModPath );

modinfo_t			gmodinfo;

#ifdef PLATFORM_WINDOWS
HWND				*pmainwindow = NULL;
#endif

char				gszDisconnectReason[256];
char				gszExtendedDisconnectReason[256];
bool				gfExtendedError = false;
uint8				g_eSteamLoginFailure = 0;
bool				g_bV3SteamInterface = false;
CreateInterfaceFn	g_AppSystemFactory = NULL;

static bool			s_bIsDedicated = false;
ConVar *sv_noclipduringpause = NULL;

// Special mode where the client uses a console window and has no graphics. Useful for stress-testing a server
// without having to round up 32 people.
bool g_bTextMode = false;

// Set to true when we exit from an error.
bool g_bInErrorExit = false;

static FileFindHandle_t	g_hfind = FILESYSTEM_INVALID_FIND_HANDLE;

// The extension DLL directory--one entry per loaded DLL
CSysModule *g_GameDLL = NULL;

// Prototype of an global method function
typedef void (DLLEXPORT * PFN_GlobalMethod)( edict_t *pEntity );

IServerGameDLL	*serverGameDLL = NULL;
int g_iServerGameDLLVersion = 0;
IServerGameEnts *serverGameEnts = NULL;

IServerGameClients *serverGameClients = NULL;
int g_iServerGameClientsVersion = 0;	// This matches the number at the end of the interface name (so for "ServerGameClients004", this would be 4).

IHLTVDirector	*serverGameDirector = NULL;

IServerGameTags *serverGameTags = NULL;

void Sys_InitArgv( char *lpCmdLine );
void Sys_ShutdownArgv( void );

//-----------------------------------------------------------------------------
// Purpose: Compare file times
// Input  : ft1 - 
//			ft2 - 
// Output : int
//-----------------------------------------------------------------------------
int Sys_CompareFileTime( long ft1, long ft2 )
{
	if ( ft1 < ft2 )
	{
		return -1;
	}
	else if ( ft1 > ft2 )
	{
		return 1;
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Is slash?
//-----------------------------------------------------------------------------
inline bool IsSlash( char c )
{
	return ( c == '\\') || ( c == '/' );
}


//-----------------------------------------------------------------------------
// Purpose: Create specified directory
// Input  : *path - 
// Output : void Sys_mkdir
//-----------------------------------------------------------------------------
void Sys_mkdir( const char *path )
{
	char testpath[ MAX_OSPATH ];

	// Remove any terminal backslash or /
	V_strcpy_safe( testpath, path );
	intp nLen = Q_strlen( testpath );
	if ( (nLen > 0) && IsSlash( testpath[ nLen - 1 ] ) )
	{
		testpath[ nLen - 1 ] = 0;
	}

	// Look for URL
	const char *pPathID = "MOD";
	if ( IsSlash( testpath[0] ) && IsSlash( testpath[1] ) )
	{
		pPathID = NULL;
	}

	if ( g_pFileSystem->FileExists( testpath, pPathID ) )
	{
		// if there is a file of the same name as the directory we want to make, just kill it
		if ( !g_pFileSystem->IsDirectory( testpath, pPathID ) )
		{
			g_pFileSystem->RemoveFile( testpath, pPathID );
		}
	}

	g_pFileSystem->CreateDirHierarchy( path, pPathID );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *path - 
//			*basename - 
// Output : char *Sys_FindFirst
//-----------------------------------------------------------------------------
const char *Sys_FindFirst(const char *path, char *basename, int namelength )
{
	if (g_hfind != FILESYSTEM_INVALID_FIND_HANDLE)
	{
		Sys_Error ("Sys_FindFirst without close");
		g_pFileSystem->FindClose(g_hfind);		
	}

	const char* psz = g_pFileSystem->FindFirst(path, &g_hfind);
	if (basename && psz)
	{
		Q_FileBase(psz, basename, namelength );
	}

	return psz;
}

//-----------------------------------------------------------------------------
// Purpose: Sys_FindFirst with a path ID filter.
//-----------------------------------------------------------------------------
const char *Sys_FindFirstEx( const char *pWildcard, const char *pPathID, char *basename, int namelength )
{
	if (g_hfind != FILESYSTEM_INVALID_FIND_HANDLE)
	{
		Sys_Error ("Sys_FindFirst without close");
		g_pFileSystem->FindClose(g_hfind);		
	}

	const char* psz = g_pFileSystem->FindFirstEx( pWildcard, pPathID, &g_hfind);
	if (basename && psz)
	{
		Q_FileBase(psz, basename, namelength );
	}

	return psz;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *basename - 
// Output : char *Sys_FindNext
//-----------------------------------------------------------------------------
const char* Sys_FindNext(char *basename, int namelength)
{
	const char *psz = g_pFileSystem->FindNext(g_hfind);
	if ( basename && psz )
	{
		Q_FileBase(psz, basename, namelength );
	}

	return psz;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void Sys_FindClose
//-----------------------------------------------------------------------------

void Sys_FindClose(void)
{
	if ( FILESYSTEM_INVALID_FIND_HANDLE != g_hfind )
	{
		g_pFileSystem->FindClose(g_hfind);
		g_hfind = FILESYSTEM_INVALID_FIND_HANDLE;
	}
}


//-----------------------------------------------------------------------------
// Purpose: OS Specific initializations
//-----------------------------------------------------------------------------
void Sys_Init( void )
{
	// Set default FPU control word to truncate (chop) mode for optimized _ftol()
	// This does not "stick", the mode is restored somewhere down the line.
//	Sys_TruncateFPU();	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Sys_Shutdown( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: Print to system console
// Input  : *fmt - 
//			... - 
// Output : void Sys_Printf
//-----------------------------------------------------------------------------
void Sys_Printf(const char *fmt, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr,fmt);
	Q_vsnprintf (text, sizeof( text ), fmt, argptr);
	va_end (argptr);
		
	if ( developer.GetInt() )
	{
		Plat_DebugString( text );
#ifdef _WIN32
		ThreadSleep( 0 );
#endif
	}

	if ( s_bIsDedicated )
	{
		fprintf( stderr, "%s", text );
	}
}


bool Sys_MessageBox(const char *title, const char *info, bool bShowOkAndCancel)
{
#ifdef _WIN32

	if ( IDOK == ::MessageBox( NULL, title, info, MB_ICONEXCLAMATION | ( bShowOkAndCancel ? MB_OKCANCEL : MB_OK ) ) )
	{
		return true;
	}
	return false;

#elif defined( USE_SDL )

	int buttonid = 0;
	SDL_MessageBoxData messageboxdata = { 0 };
	SDL_MessageBoxButtonData buttondata[] =
	{
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,	1,	"OK"		},
		{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,	0,	"Cancel"	},
	};

	messageboxdata.window = GetAssertDialogParent();
	messageboxdata.title = title;
	messageboxdata.message = info;
	messageboxdata.numbuttons = bShowOkAndCancel ? 2 : 1;
	messageboxdata.buttons = buttondata;

	SDL_ShowMessageBox( &messageboxdata, &buttonid );
	return ( buttonid == 1 );

#elif defined( POSIX )

	Warning( "%s\n", info );
	return true;

#else
#error "implement me"
#endif
}

bool g_bUpdateMinidumpComment = true;
void BuildMinidumpComment( char const *pchSysErrorText, bool bRealCrash );

void Sys_Error_Internal( bool bMinidump, const char *error, va_list argsList )
{
	char		text[1024];
	static      bool bReentry = false; // Don't meltdown

	Q_vsnprintf( text, sizeof( text ), error, argsList );

	if ( bReentry )
	{
		fprintf( stderr, "%s\n", text );
		return;
	}

	bReentry = true;

	if ( s_bIsDedicated )
	{
		fprintf( stderr, "%s\n", text );
	}
	else
	{
		Sys_Printf( "%s\n", text );
	}

	// Write the error to the log and ensure the log contents get written to disk
	g_Log.Printf( "Engine error: %s\n", text );
	g_Log.Flush();

	g_bInErrorExit = true;

#if !defined( SWDS )
	if ( videomode )
		videomode->Shutdown();
#endif

	if (!CommandLine()->FindParm( "-makereslists" ) &&
		!CommandLine()->FindParm( "-nomessagebox" ) &&
		!CommandLine()->FindParm( "-nocrashdialog" ) )
	{
#ifdef _WIN32
		::MessageBox( NULL, text, "Engine - Error", MB_OK | MB_TOPMOST | MB_ICONERROR );
#elif defined( USE_SDL )
		Sys_MessageBox( "Engine Error", text, false );
#endif
	}

	DebuggerBreakIfDebugging();

	BuildMinidumpComment( text, true );
	g_bUpdateMinidumpComment = false;

	if ( bMinidump && !Plat_IsInDebugSession() && !CommandLine()->FindParm( "-nominidumps") )
	{
#if defined( WIN32 )
		// MiniDumpWrite() has problems capturing the calling thread's context 
		// unless it is called with an exception context.  So fake an exception.
		__try
		{
			RaiseException
				(
				0,							// dwExceptionCode
				EXCEPTION_NONCONTINUABLE,	// dwExceptionFlags
				0,							// nNumberOfArguments,
				NULL						// const ULONG_PTR* lpArguments
				);

			// Never get here (non-continuable exception)
		}
		// Write the minidump from inside the filter (GetExceptionInformation() is only 
		// valid in the filter)
		__except ( 
#ifndef NO_STEAM
			SteamAPI_WriteMiniDump( 0, GetExceptionInformation(), build_number() )
#else
			// dimhotepus: Write dump via tier0 if no Steam.
			WriteMiniDumpUsingExceptionInfo( GetExceptionCode(), GetExceptionInformation(), 0x00000002 /* MiniDumpWithFullMemory */, "engine_error" )
#endif
			, EXCEPTION_EXECUTE_HANDLER )
		{

			// We always get here because the above filter evaluates to EXCEPTION_EXECUTE_HANDLER
		}
#elif defined( OSX )
		// Doing this doesn't quite work the way we want because there is no "crashing" thread
		// and we see "No thread was identified as the cause of the crash; No signature could be created because we do not know which thread crashed" on the back end
		//SteamAPI_WriteMiniDump( 0, NULL, build_number() );
		// dimhotepus: Write to stderr instead ouf stdout
		fprintf( stderr, "\n ##### Sys_Error: %s", text );
		fflush( stdout );

		int *p = 0;
#ifdef PLATFORM_64BITS
		*p = 0xdeadbeefdeadbeef;
#else
		*p = 0xdeadbeef;
#endif
#elif defined( LINUX )
		// Doing this doesn't quite work the way we want because there is no "crashing" thread
		// and we see "No thread was identified as the cause of the crash; No signature could be created because we do not know which thread crashed" on the back end
		//SteamAPI_WriteMiniDump( 0, NULL, build_number() );
		int *p = 0;
#ifdef PLATFORM_64BITS
		*p = 0xdeadbeefdeadbeef;
#else
		*p = 0xdeadbeef;
#endif
#else
#warning "need minidump impl on sys_error"
#endif
	}

	host_initialized = false;

#if defined(_WIN32)
	// We don't want global destructors in our process OR in any DLL to get executed.
	// _exit() avoids calling global destructors in our module, but not in other DLLs.
	TerminateProcess( GetCurrentProcess(), 100 );
#else
	_exit( 100 );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Exit engine with error
// Input  : *error - 
//			... - 
// Output : void Sys_Error
//-----------------------------------------------------------------------------
void Sys_Error( PRINTF_FORMAT_STRING const char *error, ...) FMTFUNCTION( 1, 2 )
{
	va_list		argptr;

	va_start( argptr, error );
	Sys_Error_Internal( true, error, argptr );
	va_end( argptr );
}


//-----------------------------------------------------------------------------
// Purpose: Exit engine with error
// Input  : *error - 
//			... - 
// Output : void Sys_Error
//-----------------------------------------------------------------------------
void Sys_Exit( PRINTF_FORMAT_STRING const char *error, ...) FMTFUNCTION( 1, 2 )
{
	va_list		argptr;

	va_start( argptr, error );
	Sys_Error_Internal( false, error, argptr );
	va_end( argptr );

}


bool IsInErrorExit()
{
	return g_bInErrorExit;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msec - 
// Output : void Sys_Sleep
//-----------------------------------------------------------------------------
void Sys_Sleep( int msec )
{
	ThreadSleep ( msec );
}

#if defined(_WIN32) && !defined( _X360 )
int prevCRTMemDebugState = 0;

BOOL WINAPI DllMain(HANDLE, ULONG ulInit, LPVOID)
{
	if (ulInit == DLL_PROCESS_ATTACH)
	{
		prevCRTMemDebugState = InitCRTMemDebug();
	} 
	else if (ulInit == DLL_PROCESS_DETACH)
	{
		ShutdownCRTMemDebug(prevCRTMemDebugState);
	}

	return TRUE;
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Allocate memory for engine hunk
// Input  : *parms - 
//-----------------------------------------------------------------------------
void Sys_InitMemory( void )
{
	// Allow overrides
	if ( CommandLine()->FindParm( "-minmemory" ) )
	{
		host_parms.memsize = MINIMUM_WIN_MEMORY;
		return;
	}

	host_parms.memsize = 0;

	MemoryInformation info;
	if (GetMemoryInformation(&info))
	{
#ifdef PLATFORM_64BITS
		host_parms.memsize = static_cast<uintp>(info.m_nPhysicalRamMbTotal) * 1024 * 1024;
#else
		host_parms.memsize = info.m_nPhysicalRamMbTotal > 4095
			? 0xFFFFFFFFUL
			: info.m_nPhysicalRamMbTotal * 1024 * 1024;
#endif
	}

	if ( host_parms.memsize == 0 )
	{
		host_parms.memsize = MAXIMUM_WIN_MEMORY;
	}

	if ( host_parms.memsize < ONE_HUNDRED_TWENTY_EIGHT_MB )
	{
		Sys_Error( "Available memory less than 128MiB!!! %zu\n", host_parms.memsize );
	}

	// take one quarter the physical memory
	if ( host_parms.memsize <= 512u*1024u*1024u)
	{
		host_parms.memsize >>= 2;
		// Apply cap of 64MB for 512MB systems
		// this keeps the code the same as HL2 gold
		// but allows us to use more memory on 1GB+ systems
		if (host_parms.memsize > MAXIMUM_DEDICATED_MEMORY)
		{
			host_parms.memsize = MAXIMUM_DEDICATED_MEMORY;
		}
	}
	else
	{
		// just take one quarter, no cap
		host_parms.memsize >>= 2;
	}

	host_parms.memsize = std::clamp(host_parms.memsize, MINIMUM_WIN_MEMORY, MAXIMUM_WIN_MEMORY);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parms - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void Sys_ShutdownMemory( void )
{
	host_parms.memsize = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Sys_InitAuthentication( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Sys_ShutdownAuthentication( void )
{
}

//-----------------------------------------------------------------------------
// Debug library spew output
//-----------------------------------------------------------------------------
CThreadLocalInt<> g_bInSpew;

#include "tier1/fmtstr.h"

static ConVar sys_minidumpspewlines( "sys_minidumpspewlines", "500", 0, "Lines of crash dump console spew to keep." );

static CUtlLinkedList< CUtlString > g_SpewHistory;
static int g_nSpewLines = 1;
static CThreadFastMutex g_SpewMutex;

static void AddSpewRecord( char const *pMsg )
{
	AUTO_LOCK( g_SpewMutex );

	static bool s_bReentrancyGuard = false;
	if ( s_bReentrancyGuard )
		return;

	s_bReentrancyGuard = true;

	if ( g_SpewHistory.Count() > sys_minidumpspewlines.GetInt() )
	{
		g_SpewHistory.Remove( g_SpewHistory.Head() );
	}

	int i = g_SpewHistory.AddToTail();
	g_SpewHistory[ i ].Format( "%d(%f):  %s", g_nSpewLines++, Plat_FloatTime(), pMsg );

	s_bReentrancyGuard = false;
}

void GetSpew( char *buf, size_t buflen )
{
	AUTO_LOCK( g_SpewMutex );

	// Walk list backward
	char *pcur = buf;
	intp remainder = (intp)buflen - 1;

	// Walk backward(
	for ( auto i = g_SpewHistory.Tail(); i != g_SpewHistory.InvalidIndex(); i = g_SpewHistory.Previous( i ) )
	{
		const CUtlString &rec = g_SpewHistory[ i ];
		intp len = rec.Length();
		intp tocopy = MIN( len, remainder );

		if ( tocopy <= 0 )
			break;
		
		Q_memcpy( pcur, rec.String(), tocopy );
		remainder -= tocopy;
		pcur += tocopy;

		if ( remainder <= 0 )
			break;
	}
	*pcur = 0;
}

ConVar spew_consolelog_to_debugstring( "spew_consolelog_to_debugstring", "0", 0, "Send console log to PLAT_DebugString()" );

SpewRetval_t Sys_SpewFunc( SpewType_t spewType, const char *pMsg )
{
	bool suppress = g_bInSpew;

	g_bInSpew = true;

	AddSpewRecord( pMsg );

	// Text output shows up on dedicated server profiles, both as consuming CPU
	// time and causing IPC delays. Sending the messages to ETW will help us
	// understand why, and save us time when server operators are triggering
	// excessive spew. Having the output in traces is also generically useful
	// for understanding slowdowns.
	ETWMark1I( pMsg, spewType );
	
	constexpr char engineGroup[] = "engine";
	const char* group = GetSpewOutputGroup();

	group = group && group[0] ? group : engineGroup;

	if ( !suppress )
	{
		// If this is a dedicated server, then we have taken over its spew function, but we still
		// want its vgui console to show the spew, so pass it into the dedicated server.
		if ( dedicated )
			dedicated->Sys_Printf( (char*)pMsg );

		if( spew_consolelog_to_debugstring.GetBool() )
		{
			Plat_DebugString( pMsg );
		}

		if ( g_bTextMode )
		{
			if (spewType == SPEW_MESSAGE || spewType == SPEW_LOG)
			{
				printf( "[%s] %s", group, pMsg );
			}
			else
			{
				fprintf( stderr, "[%s] %s", group, pMsg );
			}
		}

		if ((spewType != SPEW_LOG) || (sv.GetMaxClients() == 1))
		{
			Color color;
			switch ( spewType )
			{
#ifndef SWDS
			case SPEW_WARNING:
				{
					color.SetColor( 255, 90, 90, 255 );
				}
				break;
			case SPEW_ASSERT:
				{
					color.SetColor( 255, 20, 20, 255 );
				}
				break;
			case SPEW_ERROR:
				{
					color.SetColor( 20, 70, 255, 255 );
				}
				break;
#endif
			default:
				{
					color = *GetSpewOutputColor();
				}
				break;
			}
			Con_ColorPrintf( color, "[%s] %s", group, pMsg );

		}
		else
		{
			g_Log.Printf( "[%s] %s", group, pMsg );
		}
	}

	g_bInSpew = false;

	if (spewType == SPEW_ERROR)
	{
		Sys_Error( "[%s] %s", group, pMsg );
		return SPEW_ABORT;
	}
	if (spewType == SPEW_ASSERT)
	{
		if ( CommandLine()->FindParm( "-noassert" ) == 0 )
			return SPEW_DEBUGGER;
		else
			return SPEW_CONTINUE;
	}
	return SPEW_CONTINUE;
}

void DeveloperChangeCallback( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	// Set the "developer" spew group to the value...
	ConVarRef var( pConVar );
	int val = var.GetInt();
	SpewActivate( "developer", val );

	// Activate console spew (spew value 2 == developer console spew)
	SpewActivate( "console", val ? 2 : 1 );
}

//-----------------------------------------------------------------------------
// Purpose: factory comglomerator, gets the client, server, and gameui dlls together
//-----------------------------------------------------------------------------
void *GameFactory( const char *pName, int *pReturnCode )
{
	// first ask the app factory
	void *pRetVal = g_AppSystemFactory( pName, pReturnCode );
	if (pRetVal)
		return pRetVal;

#ifndef SWDS
	// now ask the client dll
	if (ClientDLL_GetFactory())
	{
		pRetVal = ClientDLL_GetFactory()( pName, pReturnCode );
		if (pRetVal)
			return pRetVal;
	}

	// gameui.dll
	if (EngineVGui()->GetGameUIFactory())
	{
		pRetVal = EngineVGui()->GetGameUIFactory()( pName, pReturnCode );
		if (pRetVal)
			return pRetVal;
	}
#endif	
	// server dll factory access would go here when needed

	return NULL;
}

// factory instance
CreateInterfaceFn g_GameSystemFactory = GameFactory;


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *lpOrgCmdLine - 
//			launcherFactory - 
//			*pwnd - 
//			bIsDedicated - 
// Output : int
//-----------------------------------------------------------------------------
int Sys_InitGame( CreateInterfaceFn appSystemFactory, const char* pBaseDir, void *pwnd, int bIsDedicated )
{
#ifdef BENCHMARK
	if ( bIsDedicated )
	{
		Error( "Dedicated server isn't supported by this benchmark!" );
	}
#endif

	extern void InitMathlib();
	InitMathlib();
	
	FileSystem_SetWhitelistSpewFlags();

	// Activate console spew
	// Must happen before developer.InstallChangeCallback because that callback may reset it 
	SpewActivate( "console", 1 );

	// Install debug spew output....
	developer.InstallChangeCallback( DeveloperChangeCallback );

	SpewOutputFunc( Sys_SpewFunc );
	
	// Assume failure
	host_initialized = false;

#ifdef PLATFORM_WINDOWS
	// Grab main window pointer
	pmainwindow = (HWND *)pwnd;
#endif

	// Remember that this is a dedicated server
	s_bIsDedicated = bIsDedicated ? true : false;

	memset( &gmodinfo, 0, sizeof( modinfo_t ) );

	static char s_pBaseDir[256];
	Q_strncpy( s_pBaseDir, pBaseDir, sizeof( s_pBaseDir ) );
	Q_strlower( s_pBaseDir );
	Q_FixSlashes( s_pBaseDir );
	host_parms.basedir = s_pBaseDir;

	if ( CommandLine()->FindParm ( "-pidfile" ) )
	{	
		FileHandle_t pidFile = g_pFileSystem->Open( CommandLine()->ParmValue ( "-pidfile", "srcds.pid" ), "w+" );
		if ( pidFile )
		{
			g_pFileSystem->FPrintf( pidFile, "%i\n", getpid() );
			g_pFileSystem->Close(pidFile);
		}
		else
		{
			Warning("Unable to open pidfile (%s)\n", CommandLine()->CheckParm ( "-pidfile" ));
		}
	}

	// Initialize clock
	TRACEINIT( Sys_Init(), Sys_Shutdown() );

#if defined(_DEBUG)
	if( !CommandLine()->FindParm( "-nodttest" ) && !CommandLine()->FindParm( "-dti" ) )
	{
		RunDataTableTest();	
	}
#endif

	// NOTE: Can't use COM_CheckParm here because it hasn't been set up yet.
	SeedRandomNumberGenerator( CommandLine()->FindParm( "-random_invariant" ) != 0 );

	TRACEINIT( Sys_InitMemory(), Sys_ShutdownMemory() );

	TRACEINIT( Host_Init( s_bIsDedicated ), Host_Shutdown() );

	if ( !host_initialized )
	{
		return 0;
	}

	TRACEINIT( Sys_InitAuthentication(), Sys_ShutdownAuthentication() );

	MapReslistGenerator_BuildMapList();

	BuildMinidumpComment( NULL, false );
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Sys_ShutdownGame( void )
{
	TRACESHUTDOWN( Sys_ShutdownAuthentication() );

	TRACESHUTDOWN( Host_Shutdown() );

	TRACESHUTDOWN( Sys_ShutdownMemory() );

	// TRACESHUTDOWN( Sys_ShutdownArgv() );

	TRACESHUTDOWN( Sys_Shutdown() );

	// Remove debug spew output....
	developer.InstallChangeCallback( 0 );
	SpewOutputFunc( nullptr );

	// dimhotepus: Free spew memory, etc.
	SpewDeactivate();
}

//
// Try to load a single DLL.  If it conforms to spec, keep it loaded, and add relevant
// info to the DLL directory.  If not, ignore it entirely.
//

CreateInterfaceFn g_ServerFactory;


static bool LoadThisDll( const char *szDllFilename, bool bIsServerOnly )
{
	CSysModule *pDLL = NULL;

	// check signature, don't let users with modified binaries connect to secure servers, they will get VAC banned
	if ( !Host_AllowLoadModule( szDllFilename, "GAMEBIN", true, bIsServerOnly ) )
	{
		// not supposed to load this but we will anyway
		Host_DisallowSecureServers();
		Host_AllowLoadModule( szDllFilename, "GAMEBIN", true, bIsServerOnly );
	}
	// Load DLL, ignore if cannot
	// ensures that the game.dll is running under Steam
	// this will have to be undone when we want mods to be able to run
	if ((pDLL = g_pFileSystem->LoadModule(szDllFilename, "GAMEBIN", false)) == NULL)
	{
		ConMsg("Failed to load %s\n", szDllFilename);
		goto IgnoreThisDLL;
	}

	// Load interface factory and any interfaces exported by the game .dll
	g_iServerGameDLLVersion = 0;
	g_ServerFactory = Sys_GetFactory( pDLL );
	if ( g_ServerFactory )
	{
		// Figure out latest version we understand
		g_iServerGameDLLVersion = INTERFACEVERSION_SERVERGAMEDLL_INT;

		// Scan for most recent version the game DLL understands.
		for (;;)
		{
			char archVersion[64];
			V_sprintf_safe( archVersion, "ServerGameDLL%03d", g_iServerGameDLLVersion );
			serverGameDLL = (IServerGameDLL*)g_ServerFactory(archVersion, NULL);
			if ( serverGameDLL )
				break;
			--g_iServerGameDLLVersion;
			if ( g_iServerGameDLLVersion < 4 )
			{
				g_iServerGameDLLVersion = 0;
				Msg( "Could not get IServerGameDLL interface from library %s", szDllFilename );
				goto IgnoreThisDLL;
			}
		}

		serverGameEnts = (IServerGameEnts*)g_ServerFactory(INTERFACEVERSION_SERVERGAMEENTS, NULL);
		if ( !serverGameEnts )
		{
			ConMsg( "Could not get IServerGameEnts interface from library %s", szDllFilename );
			goto IgnoreThisDLL;
		}
		
		serverGameClients = (IServerGameClients*)g_ServerFactory(INTERFACEVERSION_SERVERGAMECLIENTS, NULL);
		if ( serverGameClients )
		{
			g_iServerGameClientsVersion = 4;
		}
		else
		{
			// Try the previous version.
			const char *pINTERFACEVERSION_SERVERGAMECLIENTS_V3 = "ServerGameClients003";
			serverGameClients = (IServerGameClients*)g_ServerFactory(pINTERFACEVERSION_SERVERGAMECLIENTS_V3, NULL);
			if ( serverGameClients )
			{
				g_iServerGameClientsVersion = 3;
			}
			else
			{
				ConMsg( "Could not get IServerGameClients interface from library %s", szDllFilename );
				goto IgnoreThisDLL;
			}
		}
		serverGameDirector = (IHLTVDirector*)g_ServerFactory(INTERFACEVERSION_HLTVDIRECTOR, NULL);
		if ( !serverGameDirector )
		{
			ConMsg( "Could not get IHLTVDirector interface from library %s", szDllFilename );
			// this is not a critical 
		}

		serverGameTags = (IServerGameTags*)g_ServerFactory(INTERFACEVERSION_SERVERGAMETAGS, NULL);
		// Possible that this is NULL - optional interface
	}
	else
	{
		ConMsg( "Could not find factory interface in library %s", szDllFilename );
		goto IgnoreThisDLL;
	}

	g_GameDLL = pDLL;
	return true;

IgnoreThisDLL:
	if (pDLL != NULL)
	{
		g_pFileSystem->UnloadModule(pDLL);
		serverGameDLL = NULL;
		serverGameEnts = NULL;
		serverGameClients = NULL;
	}
	return false;
}

//
// Scan DLL directory, load all DLLs that conform to spec.
//
void LoadEntityDLLs( const char *szBaseDir, bool bIsServerOnly )
{
	memset( &gmodinfo, 0, sizeof( modinfo_t ) );
	gmodinfo.version = 1;
	gmodinfo.svonly  = true;

	// Run through all DLLs found in the extension DLL directory
	g_GameDLL = NULL;
	sv_noclipduringpause = NULL;

	{
		// Listing file for this game.
		KeyValues::AutoDelete modinfo = KeyValues::AutoDelete("modinfo");
		MEM_ALLOC_CREDIT();
		if (modinfo->LoadFromFile(g_pFileSystem, "gameinfo.txt"))
		{
			Q_strncpy( gmodinfo.szInfo, modinfo->GetString("url_info"), sizeof( gmodinfo.szInfo ) );
			Q_strncpy( gmodinfo.szDL, modinfo->GetString("url_dl"), sizeof( gmodinfo.szDL ) );

			gmodinfo.version = modinfo->GetInt("version");
			gmodinfo.size = modinfo->GetInt("size");
			gmodinfo.svonly = modinfo->GetInt("svonly") ? true : false;
			gmodinfo.cldll = modinfo->GetInt("cldll") ? true : false;

			Q_strncpy( gmodinfo.szHLVersion, modinfo->GetString("hlversion"), sizeof( gmodinfo.szHLVersion ) );
		}
	}
	
	// Load the game .dll
	LoadThisDll( "server" DLL_EXT_STRING, bIsServerOnly );

	if ( serverGameDLL )
	{
		Msg("server%s loaded for \"%s\"\n", DLL_EXT_STRING, (char *)serverGameDLL->GetGameDescription());
	}
	else
	{
		// dimhotepus: Show explicit error if no server DLL. Same as for client.dll
		Error("Could not load server" DLL_EXT_STRING " library. Try restarting. If that doesn't work, verify the cache.");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Retrieves a string value from the registry
//-----------------------------------------------------------------------------
#if defined(_WIN32)
void Sys_GetRegKeyValueUnderRoot( HKEY rootKey, const char *pszSubKey, const char *pszElement, OUT_Z_CAP(nReturnLength) char *pszReturnString, int nReturnLength, const char *pszDefaultValue )
{
	HKEY hKey;              // Handle of opened/created key
	char szBuff[128];       // Temp. buffer
	ULONG dwDisposition;    // Type of key opening event
	DWORD dwType;           // Type of key
	DWORD dwSize;           // Size of element data

	// Copying a string to itself is both unnecessary and illegal.
	// Address sanitizer prohibits this so we have to fix this in order
	// to continue testing with it.
	if ( pszReturnString != pszDefaultValue )
	{
		// Assume the worst
		Q_strncpy(pszReturnString, pszDefaultValue, nReturnLength );
	}

	// Create it if it doesn't exist.  (Create opens the key otherwise)
	LONG lResult = VCRHook_RegCreateKeyEx(
		rootKey,	// handle of open key 
		pszSubKey,			// address of name of subkey to open 
		0ul,					// DWORD ulOptions,	  // reserved 
		nullptr,			// Type of value
		REG_OPTION_NON_VOLATILE, // Store permanently in reg.
		KEY_ALL_ACCESS,		// REGSAM samDesired, // security access mask 
		NULL,
		&hKey,				// Key we are creating
		&dwDisposition);    // Type of creation
	if (lResult != ERROR_SUCCESS)  // Failure
		return;

	// First time, just set to Valve default
	if (dwDisposition == REG_CREATED_NEW_KEY)
	{
		// Just Set the Values according to the defaults
		lResult = VCRHook_RegSetValueEx( hKey, pszElement, 0, REG_SZ, (CONST BYTE *)pszDefaultValue, Q_strlen(pszDefaultValue) + 1 ); 
	}
	else
	{
		// We opened the existing key. Now go ahead and find out how big the key is.
		dwSize = nReturnLength;
		lResult = VCRHook_RegQueryValueEx( hKey, pszElement, 0, &dwType, (unsigned char *)szBuff, &dwSize );

		// Success?
		if (lResult == ERROR_SUCCESS)
		{
			// Only copy strings, and only copy as much data as requested.
			if (dwType == REG_SZ)
			{
				Q_strncpy(pszReturnString, szBuff, nReturnLength);
				pszReturnString[nReturnLength - 1] = '\0';
			}
		}
		else
		// Didn't find it, so write out new value
		{
			// Just Set the Values according to the defaults
			lResult = VCRHook_RegSetValueEx( hKey, pszElement, 0, REG_SZ, (CONST BYTE *)pszDefaultValue, Q_strlen(pszDefaultValue) + 1 ); 
		}
	};

	// Always close this key before exiting.
	VCRHook_RegCloseKey(hKey);
}


//-----------------------------------------------------------------------------
// Purpose: Retrieves a DWORD value from the registry
//-----------------------------------------------------------------------------
void Sys_GetRegKeyValueUnderRootInt( HKEY rootKey, const char *pszSubKey, const char *pszElement, long *plReturnValue, const long lDefaultValue )
{
	HKEY hKey;              // Handle of opened/created key
	ULONG dwDisposition;    // Type of key opening event
	DWORD dwType;           // Type of key
	DWORD dwSize;           // Size of element data

	// Assume the worst
	// Set the return value to the default
	*plReturnValue = lDefaultValue; 

	// Create it if it doesn't exist.  (Create opens the key otherwise)
	LONG lResult = VCRHook_RegCreateKeyEx(
		rootKey,	// handle of open key 
		pszSubKey,			// address of name of subkey to open 
		0ul,					// DWORD ulOptions,	  // reserved 
		nullptr,			// Type of value
		REG_OPTION_NON_VOLATILE, // Store permanently in reg.
		KEY_ALL_ACCESS,		// REGSAM samDesired, // security access mask 
		nullptr,
		&hKey,				// Key we are creating
		&dwDisposition);    // Type of creation
	if (lResult != ERROR_SUCCESS)  // Failure
		return;

	// First time, just set to Valve default
	if (dwDisposition == REG_CREATED_NEW_KEY)
	{
		// Just Set the Values according to the defaults
		lResult = VCRHook_RegSetValueEx( hKey, pszElement, 0, REG_DWORD, (CONST BYTE *)&lDefaultValue, sizeof( DWORD ) ); 
	}
	else
	{
		// We opened the existing key. Now go ahead and find out how big the key is.
		dwSize = sizeof( DWORD );
		lResult = VCRHook_RegQueryValueEx( hKey, pszElement, 0, &dwType, (unsigned char *)plReturnValue, &dwSize );

		// Success?
		if (lResult != ERROR_SUCCESS)
			// Didn't find it, so write out new value
		{
			// Just Set the Values according to the defaults
			lResult = VCRHook_RegSetValueEx( hKey, pszElement, 0, REG_DWORD, (LPBYTE)&lDefaultValue, sizeof( DWORD ) ); 
		}
	};

	// Always close this key before exiting.
	VCRHook_RegCloseKey(hKey);
}


void Sys_SetRegKeyValueUnderRoot( HKEY rootKey, const char *pszSubKey, const char *pszElement, const char *pszValue )
{
	HKEY hKey;              // Handle of opened/created key
	ULONG dwDisposition;    // Type of key opening event

	// Create it if it doesn't exist.  (Create opens the key otherwise)
	LONG lResult = VCRHook_RegCreateKeyEx(
		rootKey,			// handle of open key 
		pszSubKey,			// address of name of subkey to open 
		0ul,				// DWORD ulOptions,	
		nullptr,			// Type of value
		REG_OPTION_NON_VOLATILE, // Store permanently in reg.
		KEY_ALL_ACCESS,		// REGSAM samDesired, // security access mask 
		nullptr,
		&hKey,				// Key we are creating
		&dwDisposition);    // Type of creation
	if (lResult != ERROR_SUCCESS)  // Failure
		return;

	// First time, just set to Valve default
	if (dwDisposition == REG_CREATED_NEW_KEY)
	{
		// Just Set the Values according to the defaults
		lResult = VCRHook_RegSetValueEx( hKey, pszElement, 0, REG_SZ, (CONST BYTE *)pszValue, Q_strlen(pszValue) + 1 ); 
	}
	else
	{
		// Didn't find it, so write out new value
		// Just Set the Values according to the defaults
		lResult = VCRHook_RegSetValueEx( hKey, pszElement, 0, REG_SZ, (CONST BYTE *)pszValue, Q_strlen(pszValue) + 1 ); 
	}

	// Always close this key before exiting.
	VCRHook_RegCloseKey(hKey);
}
#endif

void Sys_GetRegKeyValue( const char *pszSubKey, const char *pszElement, OUT_Z_CAP(nReturnLength) char *pszReturnString, int nReturnLength, const char *pszDefaultValue )
{
#if defined(_WIN32)
	Sys_GetRegKeyValueUnderRoot( HKEY_CURRENT_USER, pszSubKey, pszElement, pszReturnString, nReturnLength, pszDefaultValue );
#else
	//hushed AssertMsg( false, "Impl me" );
	// Copying a string to itself is both unnecessary and illegal.
	if ( pszReturnString != pszDefaultValue )
	{
		Q_strncpy( pszReturnString, pszDefaultValue, nReturnLength );
	}
#endif
}

void Sys_GetRegKeyValueInt( const char *pszSubKey, const char *pszElement, long *plReturnValue, long lDefaultValue)
{
#if defined(_WIN32)
	Sys_GetRegKeyValueUnderRootInt( HKEY_CURRENT_USER, pszSubKey, pszElement, plReturnValue, lDefaultValue );
#else
	//hushed AssertMsg( false, "Impl me" );
	*plReturnValue = lDefaultValue;
#endif
}

void Sys_SetRegKeyValue( const char *pszSubKey, const char *pszElement,	const char *pszValue )
{
#if defined(_WIN32)
	Sys_SetRegKeyValueUnderRoot( HKEY_CURRENT_USER, pszSubKey, pszElement, pszValue );
#else
	//hushed AssertMsg( false, "Impl me" );
#endif
}

#define SOURCE_ENGINE_APP_CLASS "Valve.Source"

void Sys_CreateFileAssociations( int count, FileAssociationInfo *list )
{
#if defined(_WIN32)
	char appname[ 512 ];

	GetModuleFileName( 0, appname, sizeof( appname ) );
	Q_FixSlashes( appname );
	Q_strlower( appname );

	char quoted_appname_with_arg[ 512 ];
	V_sprintf_safe(quoted_appname_with_arg, "\"%s\" \"%%1\"", appname );
	char base_exe_name[ 256 ];
	Q_FileBase( appname, base_exe_name, sizeof( base_exe_name) );
	Q_DefaultExtension( base_exe_name, ".exe", sizeof( base_exe_name ) );

	// HKEY_CLASSES_ROOT/Valve.Source/shell/open/command == "u:\tf2\hl2.exe" "%1" quoted
	Sys_SetRegKeyValueUnderRoot( HKEY_CLASSES_ROOT, va( "%s\\shell\\open\\command", SOURCE_ENGINE_APP_CLASS ), "", quoted_appname_with_arg );
	// HKEY_CLASSES_ROOT/Applications/hl2.exe/shell/open/command == "u:\tf2\hl2.exe" "%1" quoted
	Sys_SetRegKeyValueUnderRoot( HKEY_CLASSES_ROOT, va( "Applications\\%s\\shell\\open\\command", base_exe_name ), "", quoted_appname_with_arg );

	char binding[32];
	for ( int i = 0; i < count ; i++ )
	{
		FileAssociationInfo *fa = &list[ i ];

		binding[0] = '\0';

		// Create file association for our .exe
		// HKEY_CLASSES_ROOT/.dem == "Valve.Source"
		Sys_GetRegKeyValueUnderRoot( HKEY_CLASSES_ROOT, fa->extension, "", binding, sizeof(binding), "" );
		if ( Q_isempty( binding ) )
		{
			Sys_SetRegKeyValueUnderRoot( HKEY_CLASSES_ROOT, fa->extension, "", SOURCE_ENGINE_APP_CLASS );
		}
	}
#endif
}

void Sys_TestSendKey( const char *pKey )
{
#if defined(_WIN32) && !defined(USE_SDL)
	int key = pKey[0];
	if ( pKey[0] == '\\' && pKey[1] == 'r' )
	{
		key = VK_RETURN;
	}

	HWND hWnd = static_cast<HWND>(game->GetMainWindow());
	PostMessageA( hWnd, WM_KEYDOWN, key, 0 );
	PostMessageA( hWnd, WM_KEYUP, key, 0 );
#endif
}

void Sys_OutputDebugString(const char *msg)
{
	Plat_DebugString( msg );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void UnloadEntityDLLs( void )
{
	if ( !g_GameDLL )
		return;

	// Unlink the cvars associated with game DLL
	FileSystem_UnloadModule( g_GameDLL );
	g_GameDLL = NULL;
	serverGameDLL = NULL;
	serverGameEnts = NULL;
	serverGameClients = NULL;
	sv_noclipduringpause = NULL;
}

CON_COMMAND( star_cpu, "Dump CPU stats" )
{
	const CPUInformation& pi{*GetCPUInformation()};

	// Compute Frequency in GHz:
	constexpr char frequence_denomination[]{"GHz"};
	const float frequency{pi.m_Speed / 1000'000'000.0f};

	char features[256];
	features[0] = '\0';

	if( pi.m_bSSE )
	{
		Q_strncat(features,
			MathLib_SSEEnabled() ? "SSE " : "(SSE) ",
			sizeof( features ) );
	}

	if( pi.m_bSSE2 )
	{
		Q_strncat(features,
			MathLib_SSE2Enabled() ? "SSE2 " : "(SSE2) ",
			sizeof( features ) );
	}

	if (pi.m_bSSE3)
	{
		Q_strncat( features,
#ifdef _XM_SSE3_INTRINSICS_
			"SSE3 ",
#else
			"(SSE3) ",
#endif
			sizeof(features) );
	}

	if (pi.m_bSSSE3)
	{
		Q_strncat( features,
			"(SSSE3) ",
			sizeof(features) );
	}

	if (pi.m_bSSE4a)
	{
		Q_strncat( features,
			"(SSE4a) ",
			sizeof(features) );
	}

	if (pi.m_bSSE41)
	{
		Q_strncat( features,
#ifdef _XM_SSE4_INTRINSICS_
			"SSE4.1 ",
#else
			"(SSE4.1) ",
#endif
			sizeof(features) );
	}

	if (pi.m_bSSE42)
	{
		Q_strncat( features,
#ifdef _XM_SSE4_INTRINSICS_
			"SSE4.2 ",
#else
			"(SSE4.2) ",
#endif
			sizeof(features) );
	}

	if( pi.m_bMMX )
	{
		Q_strncat(features,
			MathLib_SSEEnabled() ? "MMX " : "(MMX) ",
			sizeof( features ) );
	}

	if( pi.m_b3DNow )
	{
		Q_strncat(features,
			MathLib_SSEEnabled() ? "3DNow " : "(3DNow) ",
			sizeof( features ) );
	}

	if( pi.m_bRDTSC )
		Q_strncat(features, "RDTSC ", sizeof( features ) );
	if( pi.m_bCMOV )
		Q_strncat(features, "CMOV ", sizeof( features ) );
	if( pi.m_bFCMOV )
		Q_strncat(features, "FCMOV ", sizeof( features ) );

	// Remove the trailing space.  There will always be one.
	features[Q_strlen(features)-1] = '\0';

	// Dump CPU information:
	if( pi.m_nLogicalProcessors != 1 )
	{
		char buffer[256] = "";
		if( pi.m_nPhysicalProcessors != pi.m_nLogicalProcessors )
		{
			Q_snprintf(buffer,
				sizeof( buffer ),
				" (%hhu physical)",
				pi.m_nPhysicalProcessors );
		}

		ConDMsg( "hardware: CPU %s, %hhu logical%s cores, Frequency: %.01f %s,  Features: %s\n",
			pi.m_szProcessorBrand,
			pi.m_nLogicalProcessors,
			buffer,
			frequency,
			frequence_denomination,
			features
		);
		return;
	}

	ConDMsg( "hardware: CPU %s, 1 logical (1 physical) core, Frequency: %.01f %s,  Features: %s\n",
		pi.m_szProcessorBrand,
		frequency,
		frequence_denomination,
		features
	);
}

CON_COMMAND( star_memory, "Dump RAM stats" )
{
#ifdef LINUX
	struct mallinfo memstats{ mallinfo( ) };
	ConDMsg( "hardware: RAM sbrk size: %s, Used: %s, #mallocs = %d\n",
		Q_pretifymem(memstats.arena, 2, true),
		Q_pretifymem(memstats.uordblks, 2, true),
		memstats.hblks );
#elif OSX
	struct mstats memstats{ mstats( ) };
	ConDMsg( "hardware: RAM available %s, Used: %s, #mallocs = %lu\n",
		Q_pretifymem(memstats.bytes_free, 2, true),
		Q_pretifymem(memstats.bytes_used, 2, true),
		memstats.chunks_used );
#else
	MEMORYSTATUSEX stat = { sizeof(stat), 0, 0, 0, 0, 0, 0, 0, 0 };
	if ( GlobalMemoryStatusEx( &stat ) )
	{
		ConDMsg( "hardware: RAM available: %s, Used: %s, Free: %s\n",
			Q_pretifymem(stat.ullTotalPhys, 2, true),
			Q_pretifymem(stat.ullTotalPhys - stat.ullAvailPhys, 2, true),
			Q_pretifymem(stat.ullAvailPhys, 2, true) );
	}
	else
	{
		Warning( "hardware: Unable to get RAM available: %s ",
			std::system_category().message( (int) ::GetLastError() ).c_str() );
	}

	ConDMsg( "hardware: Host RAM available: %s\n",
			Q_pretifymem(host_parms.memsize, 2, true) );
#endif
}

CON_COMMAND( star_gpu, "Dump GPU stats" )
{
	MaterialAdapterInfo_t info;
	materials->GetDisplayAdapterInfo( materials->GetCurrentAdapter(), info );
	char gpuInfo[ 2048 ];

	const char *dxlevel{ "N/A" };
	const char *gpuRamSize{ nullptr };
	if ( g_pMaterialSystemHardwareConfig )
	{
		dxlevel = COM_DXLevelToString( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() ) ;
		gpuRamSize = Q_pretifymem(g_pMaterialSystemHardwareConfig->TextureMemorySize(), 2, true);
	}

	Q_snprintf( gpuInfo, sizeof( gpuInfo ),
		"GPU %s, VRAM %s, DirectX level '%s', Viewport %d x %d",
		info.m_pDriverName,
		gpuRamSize,
		dxlevel ? dxlevel : "N/A",
		videomode->GetModeWidth(), videomode->GetModeHeight());

	ConDMsg( "hardware: %s\n", gpuInfo );
}

void DisplaySystemVersion(char *osversion, int maxlen);

CON_COMMAND( star_os, "Dump operating system stats" )
{
	char osversion[ 256 ];
	DisplaySystemVersion( osversion, sizeof( osversion ) );

	ConDMsg( "os: %s\n", osversion );
}