// Copyright Valve Corporation, All rights reserved.

#include "tier0/platform.h"

// dimhotepus: platform.h ends with valve_on.h; turn Valve defines off around
// system and standard headers so STEAM's malloc / free macros can't break them.
#include "tier0/valve_off.h"

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>  // uname for Is64BitOS.
#include <fcntl.h>        // was LINUX-only, open() is used on OSX too.
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

#ifdef OSX
#include <crt_externs.h>  // _NSGetArgc, _NSGetArgv for Plat_GetCommandLine.
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach-o/dyld.h> // _NSGetExecutablePath
#include <sys/sysctl.h>
#endif

#ifndef NO_HOOK_MALLOC
// dimhotepus: Memory logging includes moved here from the middle of the file,
// so they are seen before memdbgon.h (which may #define malloc / free).
#include <execinfo.h>
// dimhotepus: <malloc.h> only on Linux; it doesn't exist on macOS.
#ifdef LINUX
#include <malloc.h>
#endif
#endif

#include "tier0/valve_on.h"

#include "include/SDL3/SDL.h"

#include "posix_file_stream.h"

#include "tier0/vcrmode.h"
#include "tier0/memalloc.h"
#include "tier0/dbg.h"
// dimhotepus: Cross-platform minidump for Plat_ExitProcessWithError, as on Windows.
#include "tier0/scoped_breakpad_handler.h"

#ifndef NO_HOOK_MALLOC
// dimhotepus: Explicit include for CThreadFastMutex / AUTO_LOCK used by the
// memory logging code (previously relied on transitively).
#include "tier0/threadtools.h"
#include "tier1/utlintrusivelist.h"
#endif

#include "tier0/memdbgon.h"

// Benchmark mode uses this heavy-handed method 

// *** WARNING ***. On Linux gettimeofday returns the system's best guess at
// actual wall clock time and this can go backwards. You need to use
// clock_gettime( CLOCK_MONOTONIC ... ) if this isn't what you want.


extern VCRMode_t g_VCRMode;

static bool g_bBenchmarkMode = false;
static double g_FakeBenchmarkTime = 0;
static double g_FakeBenchmarkTimeInc = 1.0 / 66.0;

#ifdef USE_RDTSC_FOR_FLOATTIME

static bool s_bTimeInitted;
static bool s_bUseRDTSC;
static uint64 s_nRDTSCBase;
static float s_flRDTSCToMicroSeconds;
static double s_flRDTSCScale;

#endif // USE_RDTSC_FOR_FLOATTIME

bool Plat_IsInBenchmarkMode()
{
	return g_bBenchmarkMode;
}

void Plat_SetBenchmarkMode( bool bBenchmark )
{
	g_bBenchmarkMode = bBenchmark;
}


enum {
  N_ITERATIONS_OF_RDTSC_TEST_TO_RUN = 5,					// should be odd
  TEST_RDTSC_FLOATTIME = 0
};

size_t ApproximateProcessMemoryUsage( )
{
/*
From https://man7.org/linux/man-pages/man5/proc.5.html:
       /proc/[pid]/statm
              Provides information about memory usage, measured in pages.
              The columns are:

                  size       (1) total program size
                             (same as VmSize in /proc/[pid]/status)
                  resident   (2) resident set size
                             (same as VmRSS in /proc/[pid]/status)
                  share      (3) shared pages (i.e., backed by a file)
                  text       (4) text (code)
                  lib        (5) library (unused in Linux 2.6)
                  data       (6) data + stack
                  dt         (7) dirty pages (unused in Linux 2.6)
*/

	// This returns the resident memory size (RES column in 'top') in bytes.
	size_t nRet = 0;

	constexpr char kStatsFilePath[] = "/proc/self/statm";

	auto [file, rc] = s1::posix::posix_file_stream_factory::open( kStatsFilePath, "r" );
	if ( !rc )
	{
		size_t nSize, nResident, nShare, nText, nLib_Unused, nDataPlusStack, nDt_Unused;
		if ( auto [read, rc2] = file.scan( "%zu %zu %zu %zu %zu %zu %zu",
				&nSize, &nResident, &nShare, &nText, &nLib_Unused, &nDataPlusStack, &nDt_Unused );
			 !rc2 && read >= 2 )
		{
			nRet = 4096 * nResident;
		}
		else
		{
			Warning( "Unable to read resident memory size from %s: missed resident memory value.\n",
				kStatsFilePath );
		}
	}
	else
	{
		Warning( "Unable to read resident memory size from %s: %s.\n",
			kStatsFilePath, rc.message().c_str() );
	}

	return nRet;
}

#ifdef USE_RDTSC_FOR_FLOATTIME

static void InitTimeSystem( void )
{
	s_bTimeInitted = true;

	// now, see if we can use rdtsc instead.
	// If this is one of the chips with a separate constant clock for rdtsc, we can
	auto [file, rc] = s1::posix::posix_file_stream_factory::open("/proc/cpuinfo", "r");
	if ( !rc )
	{
		bool bAnyBadCores = false;
		char lbuf[2048];

		while( std::get<char*>( file.gets( lbuf ) ) )
		{
			if ( memcmp( lbuf, "flags", 4 ) == 0 )
			{
				if ( ! strstr( lbuf, "constant_tsc" ) )
				{
					bAnyBadCores = true;
					break;
				}
			}
		}

		if ( ! bAnyBadCores )
		{
			// this system appears to have the proper cpu setup to use rdtsc from reliable timing. Let's either read the cpu frequency from an
			// environment variable, or time it ourselves
			char const *pEnv = getenv( "RDTSC_FREQUENCY" );
			if ( pEnv )
			{
				// the environment variable is allowed to hold either a benchmark result, or a string such as "disable"
				if ( pEnv && ( ( pEnv[0] > '9' ) || ( pEnv[0] < '0' ) ) )
					return;									// leave rdtsc disabled
				// the variable holds the number of ticks per microsecond
				// dimhotepus: atof -> strtof.
				s_flRDTSCToMicroSeconds = strtof( pEnv, nullptr );
				// sanity check
				if ( s_flRDTSCToMicroSeconds > 1.0 )
				{
					s_bUseRDTSC = true;
					s_flRDTSCScale = 1.0  / ( 1000.0 * 1000.0 * s_flRDTSCToMicroSeconds );
					s_nRDTSCBase = Plat_Rdtsc();
					return;
				}
			}
			else
			{
				printf( "Running a benchmark to measure system clock frequency...\n" );
				// run n iterations and use the median
				double flRDTSCToMicroSeconds[N_ITERATIONS_OF_RDTSC_TEST_TO_RUN];
				for( int i = 0; i < ssize( flRDTSCToMicroSeconds ) ; i++ )
				{
					uint64 stime = Plat_Rdtsc();
					struct timeval stimeval;
					gettimeofday( &stimeval, NULL );
					sleep( 1 );
					uint64 etime = Plat_Rdtsc() - stime;
					struct timeval etimeval;
					gettimeofday( &etimeval, NULL );
					// subtract timevals to get elapsed microseconds
					struct timeval elapsedtimeval;
					timersub( &etimeval, &stimeval, &elapsedtimeval );
					uint64 nUs = 1000000 * elapsedtimeval.tv_sec + elapsedtimeval.tv_usec;
					flRDTSCToMicroSeconds[ i ] = ( etime / nUs );
				}
				std::make_heap( flRDTSCToMicroSeconds, flRDTSCToMicroSeconds + ssize( flRDTSCToMicroSeconds ) - 1 );
				std::sort_heap( flRDTSCToMicroSeconds, flRDTSCToMicroSeconds + ssize( flRDTSCToMicroSeconds ) - 1 );
				s_flRDTSCToMicroSeconds = flRDTSCToMicroSeconds[std::size( flRDTSCToMicroSeconds ) / 2 ];
				s_flRDTSCScale = 1.0  / ( 1000.0 * 1000.0 * s_flRDTSCToMicroSeconds );
				printf( "Finished RDTSC test. To prevent the startup delay from this benchmark, set the environment variable RDTSC_FREQUENCY to %f on this system."
						" This value is dependent upon the CPU clock speed and architecture and should be determined separately for each server. The use of this mechanism"
						" for timing can be disabled by setting RDTSC_FREQUENCY to 'disabled'.\n",
						s_flRDTSCToMicroSeconds );
				s_nRDTSCBase = Plat_Rdtsc();
				s_bUseRDTSC = true;
#if TEST_RDTSC_FLOATTIME
				printf( "RDTSC test results:\n" );
				for( int i = 0; i < ssize( flRDTSCToMicroSeconds ); i++ )
					printf(" [%d] = %f\n", i, flRDTSCToMicroSeconds[i] );
				printf( "scale factor = %f\n", s_flRDTSCScale );
				uint64 srdtsc_time = Plat_Rdtsc();
				for( int i = 0; i < 1000 * 1000 * 10; i++ )
				{
					float p = Plat_FloatTime();
				}
				printf( "slow = %lld\n", Plat_Rdtsc() - srdtsc_time );
				// now, run a benchmark to see how much this optimization buys us
				srdtsc_time = Plat_Rdtsc();
				for( int i = 0; i < 1000 * 1000 * 10; i++ )
				{
					float p = Plat_FloatTime();
				}
				printf( "sfast = %lld\n", Plat_Rdtsc() - srdtsc_time );
#endif
			}
		}
	}
}

static FORCEINLINE void TestTimeSystem( void )
{
#if TEST_RDTSC_FLOATTIME
	// now, test that plat_float time actually works
	for( int t = 0 ; t < 5; t++ )
	{
		float flStartT = Plat_FloatTime();
		struct timeval stime;
		gettimeofday( &stime, NULL );
		sleep( 5 );
		float flElapsedT = Plat_FloatTime() - flStartT;
		struct timeval etime;
		gettimeofday( &etime, NULL );
		struct timeval dtime;
		timersub( &etime, &stime, &dtime );
		printf( " plat_float time says %f elapsed. gettimeofday says %f\n",
				flElapsedT, dtime.tv_sec + dtime.tv_usec / 1000000.0 );
	}
#endif
}

#endif // USE_RDTSC_FOR_FLOATTIME

double Plat_FloatTime()
{
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return g_FakeBenchmarkTime;
	}
	
#ifdef OSX
	// OSX
	static uint64 start_time = 0;
	static mach_timebase_info_data_t    sTimebaseInfo;
	static double conversion = 0.0;
	
	if ( !start_time )
	{
		start_time = mach_absolute_time();
		mach_timebase_info(&sTimebaseInfo);
		conversion = 1e-9 * (double) sTimebaseInfo.numer / (double) sTimebaseInfo.denom;
	}
	
	uint64 now = mach_absolute_time();
	
	return ( now - start_time ) * conversion;
#else
	// Linux
	static struct timespec start_time = { 0, 0 };
	static bool bInitialized = false;	

	if ( !bInitialized )
	{
		bInitialized = true;
		clock_gettime( CLOCK_MONOTONIC, &start_time );
	}

	struct timespec now;
	clock_gettime( CLOCK_MONOTONIC, &now );
	
	return ( now.tv_sec - start_time.tv_sec ) + ( now.tv_nsec * 1e-9 );

#ifdef USE_RDTSC_FOR_FLOATTIME
	if ( ! s_bTimeInitted )
	{
		InitTimeSystem();
		TestTimeSystem();
	}
	if ( s_bUseRDTSC )
	{
		uint64 nTicks = Plat_Rdtsc() - s_nRDTSCBase;
		return ( (double) nTicks) * s_flRDTSCScale;
	}
	else
	{
		struct timeval  tp;
	        gettimeofday( &tp, NULL );

		if (VCRGetMode() == VCR_Disabled)
			return (( tp.tv_sec - s_nSecBase ) + tp.tv_usec / 1000000.0 );
		
		return VCRHook_Sys_FloatTime( ( tp.tv_sec - s_nSecBase ) + tp.tv_usec / 1000000.0 );
	}
#endif // USE_RDTSC_FOR_FLOATTIME

#endif
}

Plat_MessageBoxButton Plat_MessageBox( IN_Z const char      *title,
                                       IN_Z const char      *mainInstruction,
                                       IN_Z const char      *content,
                                       Plat_MessageBoxButton buttons,
                                       Plat_MessageBoxIcon   icon )
{
	SDL_MessageBoxButtonData sdlButtons[5];
	int                      count{ 0 };

	const bool hasCancel{ HasFlag( buttons, Plat_MessageBoxButton::Cancel ) };
	const bool hasNo{ HasFlag( buttons, Plat_MessageBoxButton::No ) };
	bool       returnKeyAssigned{ false };

	const auto add = [&]( Plat_MessageBoxButton button, const char *text )
	{
		if ( !HasFlag( buttons, button ) )
		{
			return;
		}

		SDL_MessageBoxButtonFlags flags{ 0 };
		const bool                affirmative{ button == Plat_MessageBoxButton::Ok ||
			                                   button == Plat_MessageBoxButton::Yes ||
			                                   button == Plat_MessageBoxButton::Retry };
		if ( affirmative && !returnKeyAssigned )
		{
			flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
			returnKeyAssigned = true;
		}
		if ( button == Plat_MessageBoxButton::Cancel ||
		     ( !hasCancel && button == Plat_MessageBoxButton::No ) ||
		     ( !hasCancel && !hasNo && button == Plat_MessageBoxButton::Ok ) )
		{
			flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
		}

		sdlButtons[count++] = { flags, static_cast< int >( to_underlying( button ) ), text };
	};

	add( Plat_MessageBoxButton::Yes, "Yes" );
	add( Plat_MessageBoxButton::No, "No" );
	add( Plat_MessageBoxButton::Ok, "OK" );
	add( Plat_MessageBoxButton::Retry, "Retry" );
	add( Plat_MessageBoxButton::Cancel, "Cancel" );

	if ( count == 0 )
	{
		sdlButtons[count++] = {
			SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,
			static_cast< int >( to_underlying( Plat_MessageBoxButton::Ok ) ), "OK"
		};
	}

	SDL_MessageBoxFlags flags{ SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT };
	switch ( icon )
	{
	case Plat_MessageBoxIcon::Information:
		flags |= SDL_MESSAGEBOX_INFORMATION;
		break;
	case Plat_MessageBoxIcon::Warning:
		flags |= SDL_MESSAGEBOX_WARNING;
		break;
	case Plat_MessageBoxIcon::Error:
		flags |= SDL_MESSAGEBOX_ERROR;
		break;
	case Plat_MessageBoxIcon::None:
		break;
	}

	char       message[2048];
	const bool hasMain{ mainInstruction && *mainInstruction };
	const bool hasContent{ content && *content };
	std::snprintf( message, sizeof( message ), "%s%s%s",
	               hasMain ? mainInstruction : "",
	               hasMain && hasContent ? "\n\n" : "",
	               hasContent ? content : "" );

	const SDL_MessageBoxData data{ flags, nullptr, title ? title : "", message,
		                           count, sdlButtons, nullptr };

	int buttonId{ -1 };
	if ( !SDL_ShowMessageBox( &data, &buttonId ) )
	{
		return Plat_MessageBoxButton::None;
	}

	// -1 means the window was closed without pressing a button.
	return buttonId < 0 ? Plat_MessageBoxButton::Cancel : static_cast< Plat_MessageBoxButton >( buttonId );
#endif
}

unsigned int Plat_MSTime()
{
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return (unsigned int)(g_FakeBenchmarkTime * 1000.0);
	}

#ifdef USE_RDTSC_FOR_FLOATTIME
	if ( ! s_bTimeInitted )
	{
		InitTimeSystem();
		TestTimeSystem();
	}
	if ( s_bUseRDTSC )
	{
		uint64 nTicks = Plat_Rdtsc() - s_nRDTSCBase;
		return 1000.0 * nTicks * s_flRDTSCScale;
	}
	else
#endif // USE_RDTSC_FOR_FLOATTIME
	{
		return ( uint )( Plat_FloatTime() * 1000 );
	}
}

uint64 Plat_USTime()
{
	if ( g_bBenchmarkMode )
	{
		g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
		return (unsigned int)(g_FakeBenchmarkTime * 1000000.0);
	}

#ifdef USE_RDTSC_FOR_FLOATTIME
	if ( ! s_bTimeInitted )
	{
		InitTimeSystem();
		TestTimeSystem();
	}
	if ( s_bUseRDTSC )
	{
		uint64 nTicks = Plat_Rdtsc() - s_nRDTSCBase;
		return 1000000.0 * nTicks * s_flRDTSCScale;
	}
	else
#endif // USE_RDTSC_FOR_FLOATTIME
	{
		return ( uint64 )( Plat_FloatTime() * 1000000 );
	}
}

// Wraps the thread-safe versions of ctime. buf must be at least 26 bytes 
char *Plat_ctime( const time_t *timep, char *buf, size_t bufsize )
{
	return ctime_r( timep, buf );
}

// Wraps the thread-safe versions of gmtime
struct tm *Plat_gmtime( const time_t *timep, struct tm *result )
{
	return gmtime_r( timep, result );
}

time_t Plat_timegm( struct tm *timeptr )
{
	return timegm( timeptr );
}

// Wraps the thread-safe versions of localtime
struct tm *Plat_localtime( const time_t *timep, struct tm *result )
{
	return localtime_r( timep, result );
}

bool vtune( bool resume )
{
  return false;
}


// -------------------------------------------------------------------------------------------------- //
// Memory stuff.
// -------------------------------------------------------------------------------------------------- //

// dimhotepus: Plat_Alloc & co. no longer depend on NO_HOOK_MALLOC.  That
// macro only controls the malloc logging below; with it defined the exported
// allocator functions disappeared and callers failed to link.  Selected like on
// Windows: tier0 allocator unless STEAM / NO_MALLOC_OVERRIDE.

PLATFORM_INTERFACE void Plat_DefaultAllocErrorFn( [[maybe_unused]] unsigned long size )
{
}

using Plat_AllocErrorFn = void (*)(unsigned long);
Plat_AllocErrorFn g_AllocError = Plat_DefaultAllocErrorFn;

// dimhotepus: Under STEAM, valve_on.h #defines malloc / realloc / free as
// function-like macros; "(malloc)( size )" calls the real CRT function.

PLATFORM_INTERFACE MUST_CHECK_RETURN void* Plat_Alloc( unsigned long size )
{
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
	// dimhotepus: MemAlloc_Alloc -> g_pMemAlloc->Alloc, same as the other
	// functions here.
	void *pRet = g_pMemAlloc->Alloc( size );
#else
	void *pRet = (malloc)( size );
#endif
	if ( pRet )
	{
		return pRet;
	}

	// dimhotepus: nullptr for a 0-byte request is not an allocation failure.
	if ( size != 0 )
	{
		g_AllocError( size );
	}
		return nullptr;
	}


PLATFORM_INTERFACE MUST_CHECK_RETURN void* Plat_Realloc( void *ptr, unsigned long size )
{
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
	void *pRet = g_pMemAlloc->Realloc( ptr, size );
#else
	void *pRet = (realloc)( ptr, size );
#endif
	if ( pRet )
	{
		return pRet;
	}

	// dimhotepus: realloc( ptr, 0 ) frees ptr and may return nullptr; that is
	// not an allocation failure.
	if ( size != 0 )
	{
		g_AllocError( size );
	}
		return nullptr;
	}


PLATFORM_INTERFACE void Plat_Free( void *ptr )
{
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
	g_pMemAlloc->Free( ptr );
#else
	(free)( ptr );
#endif
}


PLATFORM_INTERFACE void Plat_SetAllocErrorFn( Plat_AllocErrorFn fn )
{
	g_AllocError = fn;
}

#if defined( OSX )

// From the Apple tech note: https://developer.apple.com/library/archive/qa/qa1361/_index.html
MUST_CHECK_RETURN bool Plat_IsInDebugSession()
{
	// dimhotepus: Check every call instead of caching the first answer
	// forever, so a debugger attached later is detected (as IsDebuggerPresent
	// on Windows).  The old cache was also an unsynchronized static.
	struct kinfo_proc info = {};
	int mib[4]{ CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
	size_t size{ sizeof( info ) };

	// dimhotepus: Check the sysctl result instead of ignoring it.
	if ( sysctl( mib, std::size( mib ), &info, &size, nullptr, 0 ) != 0 )
	{
		return false;
	}

		// We're being debugged if the P_TRACED flag is set.
	return ( info.kp_proc.p_flag & P_TRACED ) != 0;
}

#elif defined( LINUX )

MUST_CHECK_RETURN bool Plat_IsInDebugSession()
{
	// For linux: https://stackoverflow.com/questions/3596781/how-to-detect-if-the-current-process-is-being-run-by-gdb
	// Don't use "if (ptrace(PTRACE_TRACEME, 0, NULL, 0) == -1)" as it means debuggers can't attach.
	// 	Other solutions they mention involve forking. Ugh.
	//
	// Good solution from Pierre-Loup: Check TracerPid in /proc/self/status.
	// from "man proc"
	//     TracerPid: PID of process tracing this process (0 if not being traced).
	//
	// dimhotepus: Only async-signal-safe calls (open / read / close), since the
	// watchdog SIGALRM handler calls this via DebuggerBreakIfDebugging.
	// errno is preserved for the same reason.
	const int savedErrno{ errno };

	// dimhotepus: O_CLOEXEC, and dropped the meaningless S_IRUSR mode.
	const int fd{ open( "/proc/self/status", O_RDONLY | O_CLOEXEC ) };
	if ( fd < 0 )
	{
		errno = savedErrno;
		return false;
	}

	// dimhotepus: Read until EOF / full, retrying on EINTR.  A single read()
	// may return a partial buffer.
	char buf[4096];
	size_t used{ 0 };
	while ( used < sizeof( buf ) - 1 )
	{
		const ssize_t got{ read( fd, buf + used, sizeof( buf ) - 1 - used ) };
		if ( got > 0 )
		{
			used += static_cast<size_t>( got );
		}
		else if ( got < 0 && errno == EINTR )
		{
			continue;
		}
		else
		{
			break;
		}
		}
	close( fd );
	buf[used] = '\0';

	// dimhotepus: Parse right after the "TracerPid:" key.  The old code
	// skipped sizeof( key ) bytes, one past the colon, and relied on the
	// separator being exactly one character; strtol skips any whitespace.
	static const char s_TracerPid[] = "TracerPid:";
	const char *str{ std::strstr( buf, s_TracerPid ) };
	const long tracerpid{ str ? std::strtol( str + sizeof( s_TracerPid ) - 1, nullptr, 10 ) : 0 };

	errno = savedErrno;
	return tracerpid > 0;
}


#endif // defined( LINUX )

MUST_CHECK_RETURN bool Plat_IsUserAnAdmin()
{
	const uid_t uid{ getuid() }, euid{ geteuid() };

	// We might have elevated privileges beyond that of the user who invoked the
	// program, due to suid bit.
	return uid < 0 || euid == 0 || uid != euid;
}

void Plat_DebugString( const char * psz )
{
	// dimhotepus: Null check; fprintf( "%s" ) -> fputs.
	if ( psz )
	{
		std::fputs( psz, stderr );
	}
}

namespace {

// dimhotepus: Command line state behind a mutex.  Lazy loading in
// Plat_GetCommandLine and Plat_SetCommandLine both wrote g_CmdLine
// unsynchronized.
std::mutex g_CmdLineMutex;
bool g_bCmdLineLoaded{ false };
char g_CmdLine[ 2048 ];
#ifdef TCHAR_IS_WCHAR
// dimhotepus: tchar is wchar_t when _UNICODE is set (e.g. by STEAM in
// wchartypes.h); the old code returned char * as const tchar * there, which
// didn't compile.  Keep a wide copy.
wchar_t g_CmdLineW[ 2048 ];

// UTF-8 -> UTF-32 (POSIX wchar_t), invalid bytes become U+FFFD.
void Utf8ToWide( const char *utf8, wchar_t *out, std::size_t capacity ) noexcept
{
	std::size_t used{ 0 };
	const auto *p = reinterpret_cast<const unsigned char *>( utf8 );
	while ( *p && used + 1 < capacity )
	{
		const unsigned c{ *p };
		const unsigned extra{ c < 0x80 ? 0u : ( c >> 5 ) == 6 ? 1u : ( c >> 4 ) == 14 ? 2u :
			( c >> 3 ) == 30 ? 3u : 4u };
		unsigned codePoint{ extra == 0 ? c : c & ( 0x3Fu >> extra ) };
		bool valid{ extra < 4 };
		for ( unsigned i = 1; valid && i <= extra; ++i )
		{
			valid = ( p[i] & 0xC0 ) == 0x80;
			codePoint = ( codePoint << 6 ) | ( p[i] & 0x3F );
		}
		if ( valid )
		{
			out[used++] = static_cast<wchar_t>( codePoint );
			p += extra + 1;
		}
		else
		{
			out[used++] = static_cast<wchar_t>( 0xFFFD );
			++p;
		}
	}
	out[used] = L'\0';
}
#endif

// Copy text into g_CmdLine, truncated at a UTF-8 boundary.  Lock held.
void StoreCommandLine( const char *text ) noexcept
{
	std::snprintf( g_CmdLine, std::size( g_CmdLine ), "%s", text ? text : "" );
	TrimIncompleteUtf8( g_CmdLine );
#ifdef TCHAR_IS_WCHAR
	Utf8ToWide( g_CmdLine, g_CmdLineW, std::size( g_CmdLineW ) );
#endif
	g_bCmdLineLoaded = true;
}

// dimhotepus: Join argv like GetCommandLine on Windows: separated by spaces,
// arguments containing whitespace (or empty) quoted so the command line parser
// splits them back the same way.  The old code turned every NUL in
// /proc/self/cmdline into a space, losing argument boundaries and leaving a
// trailing space.
void AppendArgument( std::string &commandLine, const char *argument )
{
	if ( !commandLine.empty() )
{
		commandLine += ' ';
}

	const bool needsQuotes{ !*argument || std::strpbrk( argument, " \t" ) != nullptr };
	if ( needsQuotes )
{
		commandLine += '"';
	}
	commandLine += argument;
	if ( needsQuotes )
	{
		commandLine += '"';
	}
}

// Build the command line from the process arguments.  Lock held.
void LoadCommandLine()
		{
	std::string commandLine;

#if defined( OSX )
	// dimhotepus: OSX had no implementation: the command line stayed empty
	// unless Plat_SetCommandLine was called.
	const int argc{ *_NSGetArgc() };
	char **argv{ *_NSGetArgv() };
	for ( int i = 0; i < argc && argv[i]; ++i )
	{
		AppendArgument( commandLine, argv[i] );
	}
#elif defined( LINUX )
	// dimhotepus: Read the whole file.  The old code only converted when
	// everything fit in 2047 bytes; for longer command lines it returned just
	// argv[0].
	const int fd{ open( "/proc/self/cmdline", O_RDONLY | O_CLOEXEC ) };
	if ( fd >= 0 )
	{
		std::string raw;
		char chunk[4096];
		for ( ;; )
		{
			const ssize_t got{ read( fd, chunk, sizeof( chunk ) ) };
			if ( got > 0 )
			{
				raw.append( chunk, static_cast<size_t>( got ) );
			}
			else if ( got < 0 && errno == EINTR )
			{
				continue;
			}
			else
			{
				break;
			}
		}
		close( fd );

		// Arguments are NUL-terminated, one after another.
		for ( std::size_t begin = 0; begin < raw.size(); )
		{
			const std::size_t end{ raw.find( '\0', begin ) };
			const std::string argument{ raw.substr( begin,
				end == std::string::npos ? std::string::npos : end - begin ) };
			AppendArgument( commandLine, argument.c_str() );
			if ( end == std::string::npos )
				{
				break;
			}
			begin = end + 1;
		}
	}
#endif

	Assert( !commandLine.empty() );
	StoreCommandLine( commandLine.c_str() );
				}
				
}  // namespace

PLATFORM_INTERFACE void Plat_SetCommandLine( const char *cmdLine )
{
	// dimhotepus: Locked, null-safe, truncates at a UTF-8 boundary.
	std::scoped_lock lock( g_CmdLineMutex );
	StoreCommandLine( cmdLine );
}

PLATFORM_INTERFACE MUST_CHECK_RETURN const tchar *Plat_GetCommandLine()
{
	// dimhotepus: Thread-safe lazy load, on every POSIX platform.
	std::scoped_lock lock( g_CmdLineMutex );
	if ( !g_bCmdLineLoaded )
	{
		LoadCommandLine();
			}

#ifdef TCHAR_IS_WCHAR
	return g_CmdLineW;
#else
	return g_CmdLine;
#endif
		}

PLATFORM_INTERFACE MUST_CHECK_RETURN const char *Plat_GetCommandLineA()
{
	// dimhotepus: Return the narrow buffer directly.  Plat_GetCommandLine()
	// returns const wchar_t * when tchar is wide.
	std::scoped_lock lock( g_CmdLineMutex );
	if ( !g_bCmdLineLoaded )
	{
		LoadCommandLine();
	}

	return g_CmdLine;
}

namespace {

constexpr uint64 cOneMib{ 1024 * 1024 };

// dimhotepus: Bytes -> MiB, clamped to the uint fields of MemoryInformation.
[[nodiscard]] constexpr uint BytesToMib( uint64 bytes ) noexcept
{
	const uint64 mib{ bytes / cOneMib };
	return mib > UINT_MAX ? UINT_MAX : static_cast<uint>( mib );
}

// dimhotepus: Size of the process's user address space, the POSIX counterpart
// of MEMORYSTATUSEX::ullTotalVirtual: RLIMIT_AS if limited, else the usual
// user address space for the architecture.
[[nodiscard]] uint64 AddressSpaceBytes() noexcept
{
	rlimit limit{};
	if ( getrlimit( RLIMIT_AS, &limit ) == 0 && limit.rlim_cur != RLIM_INFINITY )
	{
		return static_cast<uint64>( limit.rlim_cur );
	}

#if defined( __x86_64__ )
	return 1ull << 47;  // 128 TiB, 4-level paging.
#elif defined( __aarch64__ )
	return 1ull << 48;  // 256 TiB, 48-bit VA.
#elif defined( PLATFORM_64BITS )
	return 1ull << 47;
#else
	return 1ull << 32;  // 32-bit process.
#endif
}

#ifdef LINUX
// dimhotepus: Read a small /proc file fully, retrying on EINTR.
[[nodiscard]] bool ReadProcFile( const char *path, std::string &out )
{
	const int fd{ open( path, O_RDONLY | O_CLOEXEC ) };
	if ( fd < 0 )
	{
		return false;
	}

	char chunk[4096];
	for ( ;; )
	{
		const ssize_t got{ read( fd, chunk, sizeof( chunk ) ) };
		if ( got > 0 )
		{
			out.append( chunk, static_cast<size_t>( got ) );
		}
		else if ( got < 0 && errno == EINTR )
		{
			continue;
		}
		else
{
			close( fd );
			return got == 0;
		}
	}
}
#endif

}  // namespace

PLATFORM_INTERFACE MUST_CHECK_RETURN bool GetMemoryInformation( MemoryInformation *pOutMemoryInfo )
{
	if ( !pOutMemoryInfo )
		return false;
	
	// dimhotepus: Check the struct version like Windows does; unknown
	// versions are rejected instead of written to.
	if ( pOutMemoryInfo->m_nStructVersion != 0 )
		return false;

	// dimhotepus: Fill all four fields.  Virtual memory was never set and
	// available physical memory only on OSX.
	const uint64 addressSpace{ AddressSpaceBytes() };

#ifdef OSX
	// dimhotepus: HW_PHYSMEM / HW_USERMEM -> HW_MEMSIZE.  HW_PHYSMEM and
	// HW_USERMEM are 32-bit ints (wrong past 2 GiB); reading them into a uint64
	// left the high half stale, and len wasn't reset between calls.
	uint64_t physicalBytes{ 0 };
	size_t len{ sizeof( physicalBytes ) };
	int mib[]{ CTL_HW, HW_MEMSIZE };
	if ( sysctl( mib, std::size( mib ), &physicalBytes, &len, nullptr, 0 ) != 0 )
	{
		return false;
	}

	// dimhotepus: Available = free + inactive + speculative pages, roughly
	// what Activity Monitor reports as available.
	uint64 availableBytes{ 0 };
	const mach_port_t host{ mach_host_self() };
	vm_size_t pageSize{ 0 };
	vm_statistics64_data_t vmStats{};
	mach_msg_type_number_t count{ HOST_VM_INFO64_COUNT };
	if ( host_page_size( host, &pageSize ) == KERN_SUCCESS &&
		 host_statistics64( host, HOST_VM_INFO64,
			reinterpret_cast<host_info64_t>( &vmStats ), &count ) == KERN_SUCCESS )
	{
		availableBytes = ( static_cast<uint64>( vmStats.free_count ) +
			vmStats.inactive_count + vmStats.speculative_count ) * pageSize;
	}
	mach_port_deallocate( mach_task_self(), host );

	uint64 usedVirtual{ 0 };
	task_vm_info_data_t taskInfo{};
	mach_msg_type_number_t taskCount{ TASK_VM_INFO_COUNT };
	if ( task_info( mach_task_self(), TASK_VM_INFO,
			reinterpret_cast<task_info_t>( &taskInfo ), &taskCount ) == KERN_SUCCESS )
	{
		usedVirtual = taskInfo.virtual_size;
	}
#elif defined(LINUX)
	// dimhotepus: Rewritten parser.  The old one:
	//  * called Sys_Error (not a tier0 function) and so killed the process
	//    when /proc/meminfo couldn't be read;
	//  * wrote buf[br] one past the end when the read filled the buffer;
	//  * set "line = ptr" instead of ptr + 1, so after the first line every
	//    line began with the replaced '\0' and never matched.  MemTotal only
	//    worked because it is the first line.
	std::string meminfo;
	if ( !ReadProcFile( "/proc/meminfo", meminfo ) )
	{
		Warning( "Unable to read /proc/meminfo: %s.\n", std::strerror( errno ) );
		return false;
	}

	uint64 memTotalKib{ 0 }, memAvailableKib{ 0 }, memFreeKib{ 0 }, buffersKib{ 0 }, cachedKib{ 0 };
	bool hasTotal{ false }, hasAvailable{ false };
	for ( std::size_t begin = 0; begin < meminfo.size(); )
	{
		std::size_t end{ meminfo.find( '\n', begin ) };
		if ( end == std::string::npos ) end = meminfo.size();
		const std::string line{ meminfo.substr( begin, end - begin ) };
		begin = end + 1;

		char key[64];
		unsigned long long value{ 0 };
		if ( std::sscanf( line.c_str(), "%63[^:]: %llu", key, &value ) != 2 )
			{
			continue;
			}

		if ( !std::strcmp( key, "MemTotal" ) ) { memTotalKib = value; hasTotal = true; }
		else if ( !std::strcmp( key, "MemAvailable" ) ) { memAvailableKib = value; hasAvailable = true; }
		else if ( !std::strcmp( key, "MemFree" ) ) memFreeKib = value;
		else if ( !std::strcmp( key, "Buffers" ) ) buffersKib = value;
		else if ( !std::strcmp( key, "Cached" ) ) cachedKib = value;
		}

	if ( !hasTotal )
	{
		Warning( "Unable to find MemTotal in /proc/meminfo.\n" );
		return false;
	}

	const uint64 physicalBytes{ memTotalKib * 1024 };
	// MemAvailable exists since Linux 3.14; estimate it on older kernels.
	const uint64 availableBytes{ ( hasAvailable ? memAvailableKib
		: memFreeKib + buffersKib + cachedKib ) * 1024 };

	// Process virtual size, first field of /proc/self/statm, in pages.
	uint64 usedVirtual{ 0 };
	std::string statm;
	if ( ReadProcFile( "/proc/self/statm", statm ) )
	{
		usedVirtual = std::strtoull( statm.c_str(), nullptr, 10 ) *
			static_cast<uint64>( sysconf( _SC_PAGESIZE ) );
	}
#else
	#error "Need to fill out GetMemoryInformation or at least return false for this platform"
#endif

	pOutMemoryInfo->m_nPhysicalRamMbTotal = BytesToMib( physicalBytes );
	pOutMemoryInfo->m_nPhysicalRamMbAvailable = BytesToMib( availableBytes );
	pOutMemoryInfo->m_nVirtualRamMbTotal = BytesToMib( addressSpace );
	pOutMemoryInfo->m_nVirtualRamMbAvailable =
		BytesToMib( addressSpace > usedVirtual ? addressSpace - usedVirtual : 0 );
	return true;
}


PLATFORM_INTERFACE MUST_CHECK_RETURN bool Is64BitOS()
{
#if defined OSX
	return true;
#elif defined( PLATFORM_64BITS )
	// dimhotepus: A 64-bit process implies a 64-bit OS, as on Windows.
	return true;
#else
	// dimhotepus: popen( "uname -m" ) -> uname().  popen started a shell on
	// every call, ignored fgets failure (reading an uninitialized buffer) and
	// only recognized x86_64, so 32-bit processes on 64-bit ARM / other
	// kernels got false.  Cached in a thread-safe function-local static.
	static const bool is64BitOS = []() noexcept
	{
		utsname name{};
		if ( uname( &name ) != 0 )
		{
			return false;
		}

		static constexpr const char *k64BitMachines[]{
			"x86_64", "amd64", "aarch64", "arm64", "aarch64_be", "ppc64", "ppc64le",
			"riscv64", "s390x", "mips64", "loongarch64", "sparc64" };
		for ( const char *machine : k64BitMachines )
		{
			if ( !strcasecmp( name.machine, machine ) )
	{
			return true;
	}
		}
		return false;
	}();

	return is64BitOS;
#endif
}

MUST_CHECK_RETURN bool Plat_GetModuleFilename( char *pOut, int nMaxBytes )
{
	// dimhotepus: Reject null / non-positive sizes.  nMaxBytes - 1 with
	// nMaxBytes == 0 became a huge size_t and readlink overran pOut.
	if ( !pOut || nMaxBytes <= 0 )
	{
	return false;
}

#if defined( LINUX )
	// dimhotepus: Read up to nMaxBytes.  A result shorter than the buffer is
	// complete and leaves room for the terminator; the old code passed
	// nMaxBytes - 1 and treated an exact fit as truncated.
	const ssize_t length{ readlink( "/proc/self/exe", pOut, static_cast<size_t>( nMaxBytes ) ) };
	if ( length <= 0 || length >= nMaxBytes )
	{
		pOut[0] = '\0';
		return false;
	}

	pOut[length] = '\0';
	return true;
#elif defined( OSX )
	uint32_t size{ 0 };
	_NSGetExecutablePath( nullptr, &size );
	std::vector<char> path( size + 1 );
	if ( _NSGetExecutablePath( path.data(), &size ) != 0 )
	{
		pOut[0] = '\0';
		return false;
	}

	// May contain symlinks and "..".
	char resolved[PATH_MAX];
	if ( !realpath( path.data(), resolved ) ||
		 std::strlen( resolved ) >= static_cast<size_t>( nMaxBytes ) )
	{
		pOut[0] = '\0';
		return false;
	}

	std::strcpy( pOut, resolved );
	return true;
#else
	pOut[0] = '\0';
	return false;
#endif
}

PLATFORM_INTERFACE void Plat_ExitProcess( int nCode )
{
	_exit( nCode );
}

// dimhotepus: Was missing on POSIX (declared in platform.h, implemented on
// Windows), so callers failed to link.  Same behavior as Windows.
PLATFORM_INTERFACE void Plat_ExitProcessWithError( int nCode, bool bGenerateMinidump )
{
	//handle default behavior
	if ( bGenerateMinidump )
	{
		//don't generate mini dumps in the debugger
		if ( !Plat_IsInDebugSession() )
		{
			if ( !s1::breakpad::ScopedBreakpadHandler::Instance().WriteMinidump() )
			{
				Plat_DebugString( "Unable to write minidump on exit process by error.\n" );
			}
		}
	}

	//and exit our process
	Plat_ExitProcess( nCode );
}


namespace {

// dimhotepus: Watchdog state shared with the SIGALRM handler is atomic.
// Signal handlers may only touch lock-free atomics.
std::atomic<int> s_WatchdogTime{ 0 };
std::atomic<Plat_WatchDogHandlerFunction_t> s_pWatchDogHandlerFunction{ nullptr };
static_assert( std::atomic<Plat_WatchDogHandlerFunction_t>::is_always_lock_free );
static_assert( std::atomic<int>::is_always_lock_free );

// dimhotepus: The previous SIGALRM action, restored by Plat_EndWatchdogTimer
// instead of forcing SIG_DFL (which clobbered handlers installed by others).
std::mutex s_WatchdogMutex;
struct sigaction s_PreviousAlarmAction;
bool s_bAlarmHandlerInstalled{ false };

// dimhotepus: Two plain statics -> one thread-safe function-local static.
// Uses Plat_GetCommandLineA() instead of reading g_CmdLine directly, which was
// still empty on Linux unless loaded earlier, so -nowatchdog was ignored.
[[nodiscard]] int WatchDogTimerTimeScale()
{
	static const int scale = []()
{
		if ( std::strstr( Plat_GetCommandLineA(), "-nowatchdog" ) )
	{
			return 0;
		}
#ifndef NDEBUG
		return 10;						// debug is slow
#else
		return 1;
#endif
	}();
	return scale;
}

// SIGALRM handler. Used by Watchdog timer code.
void WatchDogHandler( int )
{
	const int savedErrno{ errno };

	// dimhotepus: write() instead of Plat_DebugString( fprintf ): stdio isn't
	// async-signal-safe and can deadlock if the alarm interrupts a stdio call.
	static const char message[] = "WatchDog! Server took too long to process (probably infinite loop).\n";
	[[maybe_unused]] const ssize_t written{ write( STDERR_FILENO, message, sizeof( message ) - 1 ) };

	DebuggerBreakIfDebugging();

	if ( const auto handler = s_pWatchDogHandlerFunction.load() )
	{
		handler();
	}
	else
	{
		// force a crash
		abort();
	}

	errno = savedErrno;
}

}  // namespace

// watchdog timer support
PLATFORM_INTERFACE void Plat_BeginWatchdogTimer( int nSecs )
{
	// dimhotepus: Ignore non-positive times.  A negative value passed through
	// MIN and became a huge unsigned alarm().
	if ( nSecs <= 0 )
	{
		return;
	}

	const int scale{ WatchDogTimerTimeScale() };
	if ( scale == 0 )
	{
		return;
	}

	// dimhotepus: Multiply in 64 bits so huge nSecs can't overflow int.
	// no more than 5 minutes no matter what
	const int secs{ static_cast<int>( std::min<long long>(
		static_cast<long long>( nSecs ) * scale, 5 * 60 ) ) };

	std::scoped_lock lock( s_WatchdogMutex );
	// dimhotepus: signal() -> sigaction(), installed once, previous action saved.
	if ( !s_bAlarmHandlerInstalled )
	{
		struct sigaction action = {};
		action.sa_handler = WatchDogHandler;
		sigemptyset( &action.sa_mask );
		action.sa_flags = SA_RESTART;
		if ( sigaction( SIGALRM, &action, &s_PreviousAlarmAction ) != 0 )
	{
			Warning( "Unable to install watchdog SIGALRM handler: %s.\n", std::strerror( errno ) );
			return;
		}
		s_bAlarmHandlerInstalled = true;
	}

	s_WatchdogTime.store( secs );
	alarm( static_cast<unsigned>( secs ) );
}

PLATFORM_INTERFACE void Plat_EndWatchdogTimer( void )
{
	std::scoped_lock lock( s_WatchdogMutex );
	// dimhotepus: Only touch SIGALRM if the watchdog owns it, and restore the
	// previous action instead of SIG_DFL.
	if ( s_bAlarmHandlerInstalled )
	{
	alarm( 0 );
		sigaction( SIGALRM, &s_PreviousAlarmAction, nullptr );
		s_bAlarmHandlerInstalled = false;
	}
	s_WatchdogTime.store( 0 );
}

PLATFORM_INTERFACE MUST_CHECK_RETURN int Plat_GetWatchdogTime( void )
{
	return s_WatchdogTime.load();
}

PLATFORM_INTERFACE void Plat_SetWatchdogHandlerFunction( Plat_WatchDogHandlerFunction_t function )
{
	s_pWatchDogHandlerFunction.store( function );
}

#ifndef NO_HOOK_MALLOC

// memory logging this functionality is portable code, except for the way in which it hooks
// malloc/free. glibc contains the ability for the app to install hooks into malloc/free.
//
// dimhotepus: glibc removed __malloc_hook / __free_hook / __realloc_hook in
// 2.34 (2021), so this file didn't compile on any current distribution.  Use
// the hooks only where they exist; elsewhere memory logging is unavailable
// and EnableMemoryLogging says so.
#if defined( LINUX ) && defined( __GLIBC__ ) && \
	( __GLIBC__ < 2 || ( __GLIBC__ == 2 && __GLIBC_MINOR__ < 34 ) )
#define SE_HAS_GLIBC_MALLOC_HOOKS 1
#endif
     
#define MEMALLOC_HASHSIZE 8193
// dimhotepus: uint32 -> uintptr_t.  64-bit addresses were cut to 32 bits, so
// different blocks compared equal and frees matched the wrong allocation.
using ptrint_t = uintptr_t;



// dimhotepus: int -> int64 counters.  The byte totals overflowed (UB) past
// 2 GiB in use.
struct CLinuxMemStats
{
	int64 nNumMallocs;										// total every
	int64 nNumFrees;										// total
	int64 nNumMallocsInUse;
	int64 nTotalMallocInUse;

};

#define MAX_STACK_TRACEBACK 20

struct CLinuxMallocContext
{
	CLinuxMallocContext *m_pNext;

	void *pStackTraceBack[MAX_STACK_TRACEBACK];

	// dimhotepus: int -> int64 sizes, see CLinuxMemStats.
	int64 m_nCurrentAllocSize;
	int64 m_nNumAllocsInUse;
	int64 m_nMaximumSize;
	int64 m_TotalNumAllocs;
	int64 m_nLastAllocSize;


	CLinuxMallocContext( )
	{
		// dimhotepus: std::memset through void * (no -Wclass-memaccess).
		std::memset( static_cast<void *>( this ), 0, sizeof( *this ) );
	}

};


static CUtlIntrusiveList<CLinuxMallocContext> s_ContextHash[MEMALLOC_HASHSIZE];

CLinuxMemStats g_LinuxMemStats;


struct RememberedAlloc_t
{
	RememberedAlloc_t *m_pNext, *m_pPrev;	// all addresses that hash to the same value are linked

	CLinuxMallocContext *m_pAllocContext;
	ptrint_t m_nAddress;										// the address of the memory that came from malloc/realloc
	size_t m_nSize;

	void AdjustSize( size_t nsize )
	{
		// dimhotepus: Signed 64-bit delta (was int from size_t subtraction).
		const int64 nDelta{ static_cast<int64>( nsize ) - static_cast<int64>( m_nSize ) };
		m_nSize = nsize;
		m_pAllocContext->m_nCurrentAllocSize += nDelta;
		m_pAllocContext->m_nMaximumSize = MAX( m_pAllocContext->m_nMaximumSize, m_pAllocContext->m_nCurrentAllocSize );
	}

};

static inline int AddressHash( ptrint_t nAdr )
{
	return static_cast<int>( nAdr % MEMALLOC_HASHSIZE );
}

static CUtlIntrusiveDList<RememberedAlloc_t> s_AddressData[MEMALLOC_HASHSIZE];

// dimhotepus: Takes the address as an integer.  The hooks compute it before
// calling realloc / free, so a freed pointer value is never used afterwards
// (-Wuse-after-free).
static struct RememberedAlloc_t *FindAddress( ptrint_t nAdr, int *pHash = nullptr )
{
	int nHash = AddressHash( nAdr );
	if ( pHash )
		*pHash = nHash;
	for( RememberedAlloc_t *i = s_AddressData[nHash].m_pHead; i; i = i->m_pNext )
	{
		if ( i->m_nAddress == nAdr )
			return i;
	}
	return nullptr;
}

// dimhotepus: #ifdef LINUX -> SE_HAS_GLIBC_MALLOC_HOOKS (hooks exist only in
// glibc < 2.34).
#ifdef SE_HAS_GLIBC_MALLOC_HOOKS
static void *MallocHook( size_t, const void * );
static void FreeHook( void*, const void * );
static void *ReallocHook( void *ptr, size_t size, const void *caller );

static void RemoveHooks( void )
{
	__malloc_hook = NULL;
	__free_hook = NULL;
	__realloc_hook = NULL;
}


static void InstallHooks( void )
{
	__malloc_hook = MallocHook;
	__free_hook = FreeHook;
	__realloc_hook = ReallocHook;

}
#else

// dimhotepus: No malloc hooks on this platform / C library: no-ops (was
// OSX-only; other platforms hit #error).
static void RemoveHooks( void )
{
}


static void InstallHooks( void )
{	
}

#endif


static void AddMemoryAllocation( void *pResult, size_t size )
{
	if ( pResult )
	{
		g_LinuxMemStats.nNumMallocs++;
		g_LinuxMemStats.nNumMallocsInUse++;
		// dimhotepus: Explicit size_t -> int64 and pointer -> uintptr_t casts
		// (C-style ( ptrint_t ) cast didn't compile on 64-bit with uint32).
		g_LinuxMemStats.nTotalMallocInUse += static_cast<int64>( size );

		auto *pNew = new RememberedAlloc_t;
		pNew->m_nAddress = reinterpret_cast<ptrint_t>( pResult );
		pNew->m_nSize = size;
		s_AddressData[AddressHash( pNew->m_nAddress )].AddToHead( pNew );


		// now, find the stack traceback context for this call
		void *pTraceBack[MAX_STACK_TRACEBACK];
		int nNumGot = backtrace( pTraceBack, std::size( pTraceBack ) );
		for( int n = MAX( 0, nNumGot - 1 ); n < MAX_STACK_TRACEBACK; n++ )
			pTraceBack[n] = nullptr;

		uint32 nHash = 0;
		for(auto & i : pTraceBack)
		{
			// dimhotepus: Explicit pointer -> integer -> uint32 conversion.
			nHash = ( nHash * 3 ) + static_cast<uint32>( reinterpret_cast<ptrint_t>( i ) );
		}
		nHash %= MEMALLOC_HASHSIZE;

		CLinuxMallocContext *pFoundCtx = nullptr;
		// see if we have this context
		for( CLinuxMallocContext *i = s_ContextHash[nHash].m_pHead; i ; i = i->m_pNext )
		{
			if ( memcmp( pTraceBack, i->pStackTraceBack, sizeof( pTraceBack ) ) == 0 )
			{
				pFoundCtx = i;
				break;
			}
		}
		if ( ! pFoundCtx )
		{
			pFoundCtx = new CLinuxMallocContext;
			memcpy( pFoundCtx->pStackTraceBack, pTraceBack, sizeof( pTraceBack ) );
			s_ContextHash[nHash].AddToHead( pFoundCtx );
		}
		pNew->m_pAllocContext = pFoundCtx;
		pFoundCtx->m_nCurrentAllocSize += static_cast<int64>( size );
		pFoundCtx->m_nNumAllocsInUse++;
		pFoundCtx->m_nMaximumSize = MAX( pFoundCtx->m_nCurrentAllocSize, pFoundCtx->m_nMaximumSize );
		pFoundCtx->m_TotalNumAllocs++;
	}
}

// dimhotepus: Shared bookkeeping for a block that went away (free or a moving
// realloc).  The realloc path used to leak the node and left the global
// in-use counters too high.
static void ForgetAllocation( RememberedAlloc_t *pBlock, int nHash )
{
	pBlock->m_pAllocContext->m_nCurrentAllocSize -= static_cast<int64>( pBlock->m_nSize );
	pBlock->m_pAllocContext->m_nNumAllocsInUse--;
	g_LinuxMemStats.nTotalMallocInUse -= static_cast<int64>( pBlock->m_nSize );
	g_LinuxMemStats.nNumFrees++;
	g_LinuxMemStats.nNumMallocsInUse--;
	s_AddressData[nHash].RemoveNode( pBlock );
	delete pBlock;
}


static CThreadFastMutex s_MemoryMutex;

// dimhotepus: Hooks call the real functions as (realloc) / (malloc) / (free)
// so memdbgon.h / valve_on.h macros can't redirect them.
[[maybe_unused]] static void *ReallocHook( void *ptr, size_t size, [[maybe_unused]] const void *caller )
{
	AUTO_LOCK( s_MemoryMutex );
	RemoveHooks();
	const ptrint_t nOldAddress{ reinterpret_cast<ptrint_t>( ptr ) };
	void *nResult = (realloc)( ptr, size );

	int nHash{ 0 };
	RememberedAlloc_t *pBlock{ nOldAddress ? FindAddress( nOldAddress, &nHash ) : nullptr };

	if ( !nResult && size != 0 )
		{
		// dimhotepus: realloc failed; ptr is still valid and still tracked.
		// Old code dropped its bookkeeping.
	}
	else if ( pBlock && reinterpret_cast<ptrint_t>( nResult ) == nOldAddress )
			{
				// it successfully alloced, just need to update size info, etc
		// dimhotepus: Update the global total before AdjustSize.  The old code
		// did it after, when m_nSize already equaled size, so the delta was
		// always 0.
		g_LinuxMemStats.nTotalMallocInUse += static_cast<int64>( size ) - static_cast<int64>( pBlock->m_nSize );
				pBlock->AdjustSize( size );
			}
			else
			{
		// Moved, freed by realloc( ptr, 0 ), or not tracked before.
		if ( pBlock )
		{
			ForgetAllocation( pBlock, nHash );
		}
			AddMemoryAllocation( nResult, size );
	}

	InstallHooks();
	return nResult;
}


[[maybe_unused]] static void *MallocHook(size_t size, [[maybe_unused]] const void *caller)
{
	// turn off hooking so we won't recurse
	AUTO_LOCK( s_MemoryMutex );
	RemoveHooks();


	void *pResult = (malloc)( size );

	// now, add this memory chunk to our list
	AddMemoryAllocation( pResult, size );

	InstallHooks();

	return pResult;
}
     
[[maybe_unused]] static void FreeHook(void *ptr, [[maybe_unused]] const void *caller )
{
	AUTO_LOCK( s_MemoryMutex );
	RemoveHooks();

	// dimhotepus: Address captured before free, looked up as an integer
	// (see FindAddress); unused caller marked [[maybe_unused]].
	const ptrint_t nAddress{ reinterpret_cast<ptrint_t>( ptr ) };

	// call real free
	(free)( ptr );

	// look in our list
	if ( nAddress )
	{
		int nHash;
		RememberedAlloc_t *pFound = FindAddress( nAddress, &nHash );
		if ( !pFound )
		{
			//printf(" free of unallocated adr %p (maybe)\n", ptr );
		}
		else
		{
			// dimhotepus: Shared bookkeeping, see ForgetAllocation.
			ForgetAllocation( pFound, nHash );
		}
	}
	InstallHooks();
}
     
void EnableMemoryLogging( bool bOnOff )
{
	if ( bOnOff )
	{
#ifndef SE_HAS_GLIBC_MALLOC_HOOKS
		// dimhotepus: Tell the caller instead of silently logging nothing.
		Warning( "Memory logging is not supported on this platform (no malloc hooks).\n" );
#endif
		InstallHooks();
	}

	else
		RemoveHooks();
}


static inline bool SortLessFunc( CLinuxMallocContext * const &left, CLinuxMallocContext  * const &right )
{
	return left->m_nCurrentAllocSize > right->m_nCurrentAllocSize;
}

// dimhotepus: Shared printing for DumpMemoryLog / DumpChangedMemory.
static void PrintContextStack( CLinuxMallocContext *p )
{
	char **strings = backtrace_symbols( p->pStackTraceBack, MAX_STACK_TRACEBACK );
	RunCodeAtScopeExit( (free)( strings ) );

	Msg( "  stack\n" );
	for ( int n = 0; n < MAX_STACK_TRACEBACK; n++ )
	{
		if ( p->pStackTraceBack[n] )
		{
			Msg( "  %p %s\n", p->pStackTraceBack[n], strings ? strings[n] : "?" );
		}
	}
}

void DumpMemoryLog( int nThresh )
{
	AUTO_LOCK( s_MemoryMutex );
	// dimhotepus: EndWatchdogTimer -> Plat_EndWatchdogTimer.
	Plat_EndWatchdogTimer();
	RemoveHooks();
	std::vector<CLinuxMallocContext *> memList;
	
	for( int i =0 ; i < MEMALLOC_HASHSIZE; i++ )
	{
		for( CLinuxMallocContext *p = s_ContextHash[i].m_pHead; p; p=p->m_pNext )
		{
			if ( p->m_nCurrentAllocSize >= nThresh )
			{
				memList.push_back( p );
			}
		}
	}

	std::sort( memList.begin(), memList.end(), SortLessFunc );
	
	for( auto *p : memList )
	{
		Msg( "Context cursize=%lld nallocs=%lld maxsize=%lld total_allocs=%lld\n",
			static_cast<long long>( p->m_nCurrentAllocSize ), static_cast<long long>( p->m_nNumAllocsInUse ),
			static_cast<long long>( p->m_nMaximumSize ), static_cast<long long>( p->m_TotalNumAllocs ) );
		PrintContextStack( p );
	}
	Msg("End of memory list\n" );
	InstallHooks();
}

void DumpChangedMemory( int nThresh )
{
	AUTO_LOCK( s_MemoryMutex );
	// dimhotepus: EndWatchdogTimer -> Plat_EndWatchdogTimer.
	Plat_EndWatchdogTimer();
	RemoveHooks();
	std::vector<CLinuxMallocContext *> memList;
	
	for( int i =0 ; i < MEMALLOC_HASHSIZE; i++ )
	{
		for( CLinuxMallocContext *p = s_ContextHash[i].m_pHead; p; p=p->m_pNext )
		{
			if ( p->m_nCurrentAllocSize - p->m_nLastAllocSize > nThresh )
			{
				memList.push_back( p );
			}
		}
	}

	std::sort( memList.begin(), memList.end(), SortLessFunc );
	for( auto *p : memList )
	{
		Msg( "Context cursize=%lld lastsize=%lld nallocs=%lld maxsize=%lld total_allocs=%lld\n",
			static_cast<long long>( p->m_nCurrentAllocSize ), static_cast<long long>( p->m_nLastAllocSize ),
			static_cast<long long>( p->m_nNumAllocsInUse ), static_cast<long long>( p->m_nMaximumSize ),
			static_cast<long long>( p->m_TotalNumAllocs ) );
		PrintContextStack( p );
	}
	Msg("End of memory list\n" );
	InstallHooks();
}

void SetMemoryMark( )
{
	AUTO_LOCK( s_MemoryMutex );
	for( int i =0 ; i < MEMALLOC_HASHSIZE; i++ )
	{
		for( CLinuxMallocContext *p = s_ContextHash[i].m_pHead; p; p=p->m_pNext )
		{
			p->m_nLastAllocSize = p->m_nCurrentAllocSize;
		}
	}
}


void DumpMemorySummary( )
{
	Msg( "Total memory in use = %lld, NumMallocs=%lld, Num Frees=%lld approx process usage=%zu\n",
		static_cast<long long>( g_LinuxMemStats.nTotalMallocInUse ),
		static_cast<long long>( g_LinuxMemStats.nNumMallocs ),
		static_cast<long long>( g_LinuxMemStats.nNumFrees ),
		ApproximateProcessMemoryUsage() );
}

#endif // !NO_HOOK_MALLOC

// Turn off memdbg macros (turned on up top) since this is included like a header
#include "tier0/memdbgoff.h"