// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"

#include <ctime>

#if defined(_WIN32)
#include <cerrno>

#include "winlite.h"
#include <dwmapi.h>

// dimhotepus: COM errors.
#include "windows/com_error_category.h"
#endif

#if defined(POSIX)
#include <unistd.h>
#endif

#include <system_error>

#include "tier0/platform.h"
#include "tier0/minidump.h"
#include "tier0/vcrmode.h"

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
#include "tier0/memalloc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#endif

//our global error callback function. Note that this is not initialized, but static space guarantees this is NULL at app start.
//If you initialize, it will set to zero again when the CPP runs its static initializers, which could stomp the value if another
//CPP sets this value while initializing its static space
static ExitProcessWithErrorCBFn g_pfnExitProcessWithErrorCB; //= NULL

extern VCRMode_t g_VCRMode;
static LARGE_INTEGER g_PerformanceFrequency;
static double g_PerformanceCounterToS;
static double g_PerformanceCounterToMS;
static double g_PerformanceCounterToUS;
static LARGE_INTEGER g_ClockStart;
static bool s_bTimeInitted;

// Benchmark mode uses this heavy-handed method 
static bool g_bBenchmarkMode = false;
static double g_FakeBenchmarkTime = 0;
constexpr double g_FakeBenchmarkTimeInc = 1.0 / 66.0;

void InitTime()
{
	QueryPerformanceFrequency(&g_PerformanceFrequency);

	// Common case, frequency is 10000000.
	const long long frequency{ g_PerformanceFrequency.QuadPart };
	if ( frequency == 10000000 )
	{
		g_PerformanceCounterToS = 1.0e-7;
		g_PerformanceCounterToMS = 1.0e-4;
		g_PerformanceCounterToUS = 0.1;
	}
	else
	{
		g_PerformanceCounterToS = 1.0 / frequency;
		g_PerformanceCounterToMS = 1e3 / frequency;
		g_PerformanceCounterToUS = 1e6 / frequency;
	}

	QueryPerformanceCounter(&g_ClockStart);

	// dimhotepus: Race, set to initted only when time set.
	s_bTimeInitted = true;
}

bool Plat_IsInBenchmarkMode()
{
	return g_bBenchmarkMode;
}

void Plat_SetBenchmarkMode( bool bBenchmark )
{
	g_bBenchmarkMode = bBenchmark;
}

static long long Plat_CycleTime()
{
	Assert( s_bTimeInitted );

	LARGE_INTEGER now;
	QueryPerformanceCounter( &now );

	return now.QuadPart - g_ClockStart.QuadPart;
}

double Plat_FloatTime()
{
	if ( !g_bBenchmarkMode )
	{
		Assert( s_bTimeInitted );

		double seconds = static_cast<double>( Plat_CycleTime() ) * g_PerformanceCounterToS;
		return seconds;
	}

	g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
	return g_FakeBenchmarkTime;
}

uint32 Plat_MSTime()
{
	if ( !g_bBenchmarkMode )
	{
		Assert( s_bTimeInitted );

		double ms = static_cast<double>( Plat_CycleTime() ) * g_PerformanceCounterToMS;
		return static_cast<uint32>( ms );
	}

	g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
	return (uint32)(g_FakeBenchmarkTime * 1000.0);
}

uint64 Plat_USTime()
{
	if ( !g_bBenchmarkMode )
	{
		Assert ( s_bTimeInitted );

		double us = static_cast<double>( Plat_CycleTime() ) * g_PerformanceCounterToUS;
		return static_cast<uint64>( us );
	}

	g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
	return (uint64)(g_FakeBenchmarkTime * 1e6);
}

void GetCurrentDate( int *pDay, int *pMonth, int *pYear )
{
	struct tm *pNewTime;
	time_t long_time;

	time( &long_time );                /* Get time as long integer. */
	pNewTime = localtime( &long_time ); /* Convert to local time. */

	*pDay = pNewTime->tm_mday;
	*pMonth = pNewTime->tm_mon + 1;
	*pYear = pNewTime->tm_year + 1900;
}


// Wraps the thread-safe versions of ctime. buf must be at least 26 bytes 
char *Plat_ctime( const time_t *timep, char *buf, size_t bufsize )
{
	if ( EINVAL == ctime_s( buf, bufsize, timep ) )
		return nullptr;
	else
		return buf;
}

void Plat_GetModuleFilename( char *pOut, int nMaxBytes )
{
#ifdef PLATFORM_WINDOWS_PC
	SetLastError( ERROR_SUCCESS ); // clear the error code
	GetModuleFileName( nullptr, pOut, nMaxBytes );
	if ( GetLastError() != ERROR_SUCCESS )
		Error( "Plat_GetModuleFilename: Unable to read exe file name: %s.", 
			std::system_category().message(GetLastError()).c_str());
#else
	if ( -1 == readlink( "/proc/self/exe", pOut, nMaxBytes ) )
		Error( "Plat_GetModuleFilename: Unable to read exe file name: %s.", 
			std::system_category().message(errno).c_str() );
#endif
}

void Plat_ExitProcess( int nCode )
{
#if defined( _WIN32 )
	// We don't want global destructors in our process OR in any DLL to get executed.
	// _exit() avoids calling global destructors in our module, but not in other DLLs.
	// dimhotepus: Drop UB on gc.
	// const char *pchCmdLineA = Plat_GetCommandLineA();
	// if ( nCode || ( strstr( pchCmdLineA, "gc.exe" ) && strstr( pchCmdLineA, "gc.dll" ) && strstr( pchCmdLineA, "-gc" ) ) )
	// {
	// 	int *x = nullptr; *x = 1; // cause a hard crash, GC is not allowed to exit voluntarily from gc.dll //-V522
	// }
	TerminateProcess( GetCurrentProcess(), nCode );
#else	
	_exit( nCode );
#endif
}

void Plat_ExitProcessWithError( int nCode, bool bGenerateMinidump )
{
	//try to delegate out if they have registered a callback
	const auto cb = g_pfnExitProcessWithErrorCB;
	if( cb && cb( nCode ) )
		return;

	//handle default behavior
	if( bGenerateMinidump )
	{
		//don't generate mini dumps in the debugger
		if( !Plat_IsInDebugSession() )
		{
			WriteMiniDump();
		}
	}

	//and exit our process
	Plat_ExitProcess( nCode );
}

void Plat_SetExitProcessWithErrorCB( ExitProcessWithErrorCBFn pfnCB )
{
	g_pfnExitProcessWithErrorCB = pfnCB;	
}

// Wraps the thread-safe versions of gmtime
struct tm *Plat_gmtime( const time_t *timep, struct tm *result )
{
	if ( EINVAL == gmtime_s( result, timep ) )
		return nullptr;
	else
		return result;
}


time_t Plat_timegm( struct tm *timeptr )
{
	return _mkgmtime( timeptr );
}


// Wraps the thread-safe versions of localtime
struct tm *Plat_localtime( const time_t *timep, struct tm *result )
{
	if ( EINVAL == localtime_s( result, timep ) )
		return nullptr;
	else
		return result;
}


bool vtune( bool resume )
{
#ifndef _X360
	static bool bInitialized = false;
	static void (__cdecl *VTResume)(void) = nullptr;
	static void (__cdecl *VTPause) (void) = nullptr;

	// Grab the Pause and Resume function pointers from the VTune DLL the first time through:
	if( !bInitialized )
	{
		bInitialized = true;

		HINSTANCE pVTuneDLL = LoadLibrary( "vtuneapi.dll" );

		if( pVTuneDLL )
		{
			VTResume = (void(__cdecl *)())GetProcAddress( pVTuneDLL, "VTResume" );
			VTPause  = (void(__cdecl *)())GetProcAddress( pVTuneDLL, "VTPause" );
		}
	}

	// Call the appropriate function, as indicated by the argument:
	if( resume && VTResume )
	{
		VTResume();
		return true;

	} 
	else if( !resume && VTPause )
	{
		VTPause();
		return true;
	}
#endif
	return false;
}

bool Plat_IsInDebugSession()
{
#if defined( _WIN32 ) && !defined( _X360 )
	return (IsDebuggerPresent() != 0);
#elif defined( _WIN32 ) && defined( _X360 )
	return (XBX_IsDebuggerPresent() != 0);
#elif defined( LINUX )
	#error This code is implemented in platform_posix.cpp
#else
	return false;
#endif
}

// dimhotepus: Additional bug info.
#if defined(_WIN32) && !defined(_X360)
// See
// https://docs.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-checktokenmembership
static BOOL IsUserAdmin() {
  SID_IDENTIFIER_AUTHORITY ntAuthority{SECURITY_NT_AUTHORITY};
  PSID administratorsGroup;
  BOOL ok = AllocateAndInitializeSid(
      &ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0,
      0, 0, 0, 0, 0, &administratorsGroup);

  if (ok) {
    if (!CheckTokenMembership(nullptr, administratorsGroup, &ok)) {
      ok = FALSE;
    }

    FreeSid(administratorsGroup);
  }

  return ok;
}
#endif

// dimhotepus: Additional bug info.
bool Plat_IsUserAnAdmin() {
#if defined(_WIN32) && !defined(_X360)
  return ::IsUserAdmin() ? true : false;
#else
  const uid_t uid{getuid()}, euid{geteuid()};

  // We might have elevated privileges beyond that of the user who invoked the
  // program, due to suid bit.
  return uid < 0 || euid == 0 || uid != euid;
#endif
}

[[nodiscard]] static bool ShouldAppUseLightTheme() {
	HKEY personalizeKey;
	// Light mode by default if not supported.
	DWORD value = 1;
	LSTATUS ok{::RegOpenKeyExW(HKEY_CURRENT_USER,
		L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
		0, KEY_READ, &personalizeKey)};
	if (personalizeKey) {
		DWORD valueType;
		DWORD valueSize = sizeof(value);

		ok = ::RegQueryValueExW(personalizeKey,
			L"AppsUseLightTheme",
			nullptr,
			&valueType,
			reinterpret_cast<byte*>(&value),
			&valueSize);

		::RegCloseKey(personalizeKey);

		// Light mode by default if not supported.
		value = ok == ERROR_SUCCESS && valueType == REG_DWORD ? value : 1;
	}

	return value > 0;
}

// dimhotepus: Apply system Mica materials to window.
// See https://learn.microsoft.com/en-us/windows/apps/design/style/mica
bool Plat_ApplySystemTitleBarTheme(void *window,
                                   SystemBackdropType backDropType) {
	static_assert(to_underlying(SystemBackdropType::Auto) == DWMSBT_AUTO);
	static_assert(to_underlying(SystemBackdropType::None) == DWMSBT_NONE);
	static_assert(to_underlying(SystemBackdropType::MainWindow) ==
	              DWMSBT_MAINWINDOW);
	static_assert(to_underlying(SystemBackdropType::TransientWindow) ==
	              DWMSBT_TRANSIENTWINDOW);
	static_assert(to_underlying(SystemBackdropType::TabbedWindow) ==
	              DWMSBT_TABBEDWINDOW);
	
	bool ok = true;
	
	// dimhotepus: Since Windows 11 21H1 (22000)
	// DWM has DWMWA_USE_IMMERSIVE_DARK_MODE & DWMWA_SYSTEMBACKDROP_TYPE.
#if defined(WDK_NTDDI_VERSION) && (WDK_NTDDI_VERSION >= NTDDI_WIN11_ZN)
	// Detect current app  window text is light or dark and apply Dark mode.
	const BOOL enableDarkMode{ShouldAppUseLightTheme() ? FALSE : TRUE};
	HRESULT hr{::DwmSetWindowAttribute(static_cast<HWND>(window),
		DWMWA_USE_IMMERSIVE_DARK_MODE, &enableDarkMode, sizeof(enableDarkMode))};
	if (FAILED(hr)) {
		const int windowTextSize =
			GetWindowTextLengthA(static_cast<HWND>(window)) + 1;
		char *windowText = stackallocT(char, windowTextSize);
		GetWindowTextA(static_cast<HWND>(window), windowText, windowTextSize);

		Warning(
			"Unable to apply user Color Mode preferences to window 0x%p (%s): "
			"%s.\n",
			windowText, window,
			se::win::com::com_error_category().message(hr).c_str());

		ok = false;
	}

	const DWM_SYSTEMBACKDROP_TYPE dwm_backdrop_type =
		static_cast<DWM_SYSTEMBACKDROP_TYPE>(to_underlying(backDropType));
	hr = ::DwmSetWindowAttribute(
		static_cast<HWND>(window), DWMWA_SYSTEMBACKDROP_TYPE,
		&dwm_backdrop_type, sizeof(dwm_backdrop_type));
	if (FAILED(hr)) {
		const int windowTextSize =
			GetWindowTextLengthA(static_cast<HWND>(window)) + 1;
		char *windowText = stackallocT(char, windowTextSize);
		GetWindowTextA(static_cast<HWND>(window), windowText, windowTextSize);

		Warning(
			"Unable to apply backdrop material effect to window 0x%p (%s): %s.\n",
			windowText, window,
			se::win::com::com_error_category().message(hr).c_str());

		ok = false;
	}
#else
	ok = false;
#endif

	return ok;
}


void Plat_DebugString( const char * psz )
{
#if defined( _WIN32 ) && !defined( _X360 )
	::OutputDebugStringA( psz );
#elif defined( _WIN32 ) && defined( _X360 )
	XBX_OutputDebugString( psz );
#endif
}


const tchar *Plat_GetCommandLine()
{
#ifdef TCHAR_IS_WCHAR
	return GetCommandLineW();
#else
	return GetCommandLine();
#endif
}

bool GetMemoryInformation( MemoryInformation *pOutMemoryInfo )
{
	if ( !pOutMemoryInfo ) 
		return false;

	MEMORYSTATUSEX memStat = { sizeof(memStat), 0, 0, 0, 0, 0, 0, 0, 0 };
	if ( !GlobalMemoryStatusEx( &memStat ) ) 
		return false;

	constexpr uint32_t cOneMib{1024 * 1024};

	switch ( pOutMemoryInfo->m_nStructVersion )
	{
	case 0:
		( *pOutMemoryInfo ).m_nPhysicalRamMbTotal     = static_cast<uint>(memStat.ullTotalPhys / cOneMib);
		( *pOutMemoryInfo ).m_nPhysicalRamMbAvailable = static_cast<uint>(memStat.ullAvailPhys / cOneMib);
		( *pOutMemoryInfo ).m_nVirtualRamMbTotal      = static_cast<uint>(memStat.ullTotalVirtual / cOneMib);
		( *pOutMemoryInfo ).m_nVirtualRamMbAvailable  = static_cast<uint>(memStat.ullAvailVirtual / cOneMib);
		break;

	default:
		return false;
	}

	return true;
}


const char *Plat_GetCommandLineA()
{
	return GetCommandLineA();
}

//--------------------------------------------------------------------------------------------------
// Watchdog timer
//--------------------------------------------------------------------------------------------------
void Plat_BeginWatchdogTimer( [[maybe_unused]] int nSecs )
{
}
void Plat_EndWatchdogTimer( void )
{
}
int Plat_GetWatchdogTime( void )
{
	return 0;
}
void Plat_SetWatchdogHandlerFunction( [[maybe_unused]] Plat_WatchDogHandlerFunction_t function )
{
}

bool Is64BitOS()
{
	// dimhotepus: Cleanup x86-64 process detection.
#ifdef _WIN64
  return true;
#else
	static BOOL bIs64bit = FALSE;
	static bool bInitialized = false;
	if ( bInitialized )
		return bIs64bit != FALSE;
	else
	{
		bInitialized = true;
		// NOTE: If the process is a 32-bit application running under 64-bit Windows 10 on ARM, the value is set to FALSE
		return ::IsWow64Process(::GetCurrentProcess(), &bIs64bit) && bIs64bit;
	}
#endif
}


// -------------------------------------------------------------------------------------------------- //
// Memory stuff. 
//
// DEPRECATED. Still here to support binary back compatability of tier0.dll
//
// -------------------------------------------------------------------------------------------------- //
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)

using Plat_AllocErrorFn = void (*)(unsigned long);

void Plat_DefaultAllocErrorFn( [[maybe_unused]] unsigned long size )
{
}

Plat_AllocErrorFn g_AllocError = Plat_DefaultAllocErrorFn;
#endif

PLATFORM_INTERFACE void* Plat_Alloc( unsigned long size )
{
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
	void *pRet = g_pMemAlloc->Alloc( size );
#else
	void *pRet = malloc( size );
#endif

	if ( pRet )
	{
		return pRet;
	}

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
	g_AllocError( size );
#endif
	return nullptr;
}

PLATFORM_INTERFACE void* Plat_Realloc( void *ptr, unsigned long size )
{
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
	void *pRet = g_pMemAlloc->Realloc( ptr, size );
#else
	void *pRet = realloc( ptr, size );
#endif

	if ( pRet )
	{
		return pRet;
	}

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
	g_AllocError( size );
#endif
	return nullptr;
}

PLATFORM_INTERFACE void Plat_Free( void *ptr )
{
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
	g_pMemAlloc->Free( ptr );
#else
	free( ptr );
#endif
}

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
PLATFORM_INTERFACE void Plat_SetAllocErrorFn( Plat_AllocErrorFn fn )
{
	g_AllocError = fn;
}
#endif
