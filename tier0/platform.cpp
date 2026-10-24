// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"

#include <ctime>

#if defined(_WIN32)
#include <cerrno>

#include "winlite.h"
#include <dwmapi.h>

// dimhotepus: COM errors.
#include "windows/com_error_category.h"
#else
#include "include/SDL3/SDL.h"
#endif

#if defined(POSIX)
#include <unistd.h>  // chdir, readlink
#ifdef __APPLE__
#include <mach-o/dyld.h>  // _NSGetExecutablePath
#endif
#endif

#include <system_error>

#include "tier0/platform.h"
#include "tier0/vcrmode.h"
#include "tier0/scoped_breakpad_handler.h"

#include <atomic>

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
static std::atomic_bool g_bBenchmarkMode = false;
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
	return g_bBenchmarkMode.load(std::memory_order::memory_order_relaxed);
}

void Plat_SetBenchmarkMode( bool bBenchmark )
{
	g_bBenchmarkMode.store(bBenchmark, std::memory_order::memory_order_relaxed);
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
	if ( !g_bBenchmarkMode.load(std::memory_order::memory_order_relaxed) )
	{
		Assert( s_bTimeInitted );

		double seconds = static_cast<double>( Plat_CycleTime() ) * g_PerformanceCounterToS;
		return seconds;
	}

	g_FakeBenchmarkTime += g_FakeBenchmarkTimeInc;
	return g_FakeBenchmarkTime;
}

namespace {
	
template <typename TEnum>
using IsEnum = std::enable_if_t<std::is_enum_v<TEnum>, bool>;

template<typename TEnum>
[[nodiscard]] constexpr IsEnum<TEnum> HasFlag( TEnum value, TEnum flag )
{
	// value == flag to handle 0 == 0.
	return value == flag || ( value & flag ) == flag;
}

}  // namespace

#ifdef _WIN32

namespace {

// Converts UTF-8 to UTF-16, truncating (at a code point boundary) instead of
// failing when the text is too long for the buffer.
template <std::size_t size>
wchar_t* Utf8ToWide( IN_Z const char* utf8, OUT_Z_ARRAY wchar_t ( &buffer )[size] ) noexcept
{
	static_assert( size > 1 && size <= INT_MAX );
 
	buffer[0] = L'\0';
	if ( !utf8 || !*utf8 )
	{
		return buffer;
	}
 
	// Every UTF-8 byte produces at most one UTF-16 code unit (4-byte sequences
	// produce 2 units), so feeding at most size - 1 bytes always fits.
	std::size_t length = std::strlen( utf8 );
	if ( length > size - 1 )
	{
		length = size - 1;
		// Do not split a multi-byte sequence: back off while the first
		// excluded byte is a continuation byte (10xxxxxx).
		while ( length > 0 && ( static_cast<unsigned char>( utf8[length] ) & 0xC0 ) == 0x80 )
		{
			--length;
		}
	}
 
	const int written{ ::MultiByteToWideChar( CP_UTF8, 0, utf8, static_cast<int>( length ),
		buffer, static_cast<int>( size - 1 ) ) };
	if ( written <= 0 )
	{
		wcscpy_s( buffer, L"N/A" );
		return buffer;
	}
 
	// With an explicit input length, no terminator is written for us.
	buffer[written] = L'\0';
	return buffer;
}

[[nodiscard]] constexpr TASKDIALOG_COMMON_BUTTON_FLAGS GetTaskDialogButtons(
	Plat_MessageBoxButton buttons ) noexcept
{
	TASKDIALOG_COMMON_BUTTON_FLAGS flags = 0;
 
	if ( HasFlag( buttons, Plat_MessageBoxButton::Ok ) )     flags |= TDCBF_OK_BUTTON;
	if ( HasFlag( buttons, Plat_MessageBoxButton::Yes ) )    flags |= TDCBF_YES_BUTTON;
	if ( HasFlag( buttons, Plat_MessageBoxButton::No ) )     flags |= TDCBF_NO_BUTTON;
	if ( HasFlag( buttons, Plat_MessageBoxButton::Cancel ) ) flags |= TDCBF_CANCEL_BUTTON;
	if ( HasFlag( buttons, Plat_MessageBoxButton::Retry ) )  flags |= TDCBF_RETRY_BUTTON;
 
	return flags;
}

// MessageBoxW only supports fixed combinations; pick the closest superset.
[[nodiscard]] constexpr UINT GetMessageBoxButtons( Plat_MessageBoxButton buttons ) noexcept
{
	const bool cancel{ HasFlag( buttons, Plat_MessageBoxButton::Cancel ) };
 
	if ( HasFlag( buttons, Plat_MessageBoxButton::Yes ) ||
		 HasFlag( buttons, Plat_MessageBoxButton::No ) )
	{
		return cancel ? MB_YESNOCANCEL : MB_YESNO;
	}
 
	if ( HasFlag( buttons, Plat_MessageBoxButton::Retry ) )
	{
		return MB_RETRYCANCEL;
	}
 
	return cancel ? MB_OKCANCEL : MB_OK;
}

[[nodiscard]] Plat_MessageBoxButton GetPressedButton( int button ) noexcept
{
	switch ( button )
	{
		case 0:        return Plat_MessageBoxButton::None;
		case IDOK:     return Plat_MessageBoxButton::Ok;
		case IDYES:    return Plat_MessageBoxButton::Yes;
		case IDNO:     return Plat_MessageBoxButton::No;
		case IDCANCEL: return Plat_MessageBoxButton::Cancel;
		case IDRETRY:  return Plat_MessageBoxButton::Retry;
		default:
			AssertMsg( false, "Unknown message box button pressed 0x%x.", button );
			return Plat_MessageBoxButton::None;
	}
}
 
[[nodiscard]] PCWSTR GetTaskDialogIcon( Plat_MessageBoxIcon icon ) noexcept
{
	switch ( icon )
	{
		case Plat_MessageBoxIcon::None:        return nullptr;
		case Plat_MessageBoxIcon::Information: return TD_INFORMATION_ICON;
		case Plat_MessageBoxIcon::Warning:     return TD_WARNING_ICON;
		case Plat_MessageBoxIcon::Error:       return TD_ERROR_ICON;
	}
 
	AssertMsg( false, "Unknown message box icon 0x%x.", static_cast<unsigned>( icon ) );
	return nullptr;
}

[[nodiscard]] UINT GetMessageBoxIcon( Plat_MessageBoxIcon icon ) noexcept
{
	switch ( icon )
	{
		case Plat_MessageBoxIcon::None:        return 0;
		case Plat_MessageBoxIcon::Information: return MB_ICONINFORMATION;
		case Plat_MessageBoxIcon::Warning:     return MB_ICONWARNING;
		case Plat_MessageBoxIcon::Error:       return MB_ICONERROR;
	}
 
	AssertMsg( false, "Unknown message box icon 0x%x.", static_cast<unsigned>( icon ) );
	return 0;
}

}  // namespace
#endif  // _WIN32

Plat_MessageBoxButton Plat_MessageBox( IN_Z const char* title,
									   IN_Z const char* mainInstruction,
									   IN_Z const char* content,
									   Plat_MessageBoxButton buttons,
									   Plat_MessageBoxIcon icon )
{
#ifdef _WIN32
	wchar_t wideTitle[512], wideMainInstruction[512], wideContent[512];
	int pressed{ 0 };

	const HRESULT hr{ ::TaskDialog(
		nullptr,
		nullptr,
		Utf8ToWide( title, wideTitle ),
		Utf8ToWide( mainInstruction, wideMainInstruction ),
		Utf8ToWide( content, wideContent ),
		GetTaskDialogButtons( buttons ),
		GetTaskDialogIcon( icon ),
		&pressed ) };
	if ( SUCCEEDED( hr ) )
	{
		return GetPressedButton( pressed );
	}
 
	// Fallback: TaskDialog failed (e.g. out of memory).
	wchar_t combined[std::size( wideMainInstruction ) + 2 + std::size( wideContent )];
	combined[0] = L'\0';
 
	if ( wideMainInstruction[0] != L'\0' )
	{
		wcscpy_s( combined, wideMainInstruction );
		if ( wideContent[0] != L'\0' )
		{
			wcscat_s( combined, L"\n\n" );
		}
	}
	wcscat_s( combined, wideContent );
 
	pressed = ::MessageBoxW( nullptr, combined, wideTitle,
		MB_SYSTEMMODAL | MB_SETFOREGROUND |
		GetMessageBoxIcon( icon ) | GetMessageBoxButtons( buttons ) );
	return GetPressedButton( pressed );
#else
	SDL_MessageBoxButtonData sdlButtons[5];
	int count{ 0 };
 
	const bool hasCancel{ HasFlag( buttons, Plat_MessageBoxButton::Cancel ) };
	const bool hasNo{ HasFlag( buttons, Plat_MessageBoxButton::No ) };
	bool returnKeyAssigned{ false };
 
	const auto add = [&]( Plat_MessageBoxButton button, const char* text )
	{
		if ( !HasFlag( buttons, button ) )
		{
			return;
		}
 
		SDL_MessageBoxButtonFlags flags{ 0 };
		const bool affirmative{ button == Plat_MessageBoxButton::Ok ||
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
 
		sdlButtons[count++] = { flags, static_cast<int>( to_underlying( button ) ), text };
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
			static_cast<int>( to_underlying( Plat_MessageBoxButton::Ok ) ), "OK" };
	}
 
	SDL_MessageBoxFlags flags{ SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT };
	switch ( icon )
	{
		case Plat_MessageBoxIcon::Information: flags |= SDL_MESSAGEBOX_INFORMATION; break;
		case Plat_MessageBoxIcon::Warning:     flags |= SDL_MESSAGEBOX_WARNING; break;
		case Plat_MessageBoxIcon::Error:       flags |= SDL_MESSAGEBOX_ERROR; break;
		case Plat_MessageBoxIcon::None:        break;
	}
 
	char message[2048];
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
	return buttonId < 0
		? Plat_MessageBoxButton::Cancel
		: static_cast<Plat_MessageBoxButton>( buttonId );
#endif
}

uint32 Plat_MSTime()
{
	if ( !g_bBenchmarkMode.load(std::memory_order::memory_order_relaxed) )
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
	if ( !g_bBenchmarkMode.load(std::memory_order::memory_order_relaxed) )
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

bool Plat_GetModuleFilename( char *pOut, int nMaxBytes )
{
#ifdef _WIN32
	HMODULE module{nullptr};
	if (!::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			nullptr, &module)) {
		return false;
	}

	// A truncated path is reported as success with length == size.
	const DWORD length{
		::GetModuleFileName(module, pOut, static_cast<DWORD>(nMaxBytes))};
	return length > 0 && length < static_cast<DWORD>(nMaxBytes);
#elif defined(_LINUX)
	// readlink does not null-terminate; length == size - 1 may be truncated.
	const ssize_t length{readlink("/proc/self/exe", pOut, nMaxBytes - 1)};
	if (length <= 0 || length >= nMaxBytes - 1) {
		return false;
	}

	pOut[length] = '\0';
	return true;
#elif defined(OSX)
	char path[PATH_MAX];
	uint32_t size{sizeof(path)};
	if (_NSGetExecutablePath(path, &size) != 0) {
		return false;
	}

	// May contain symlinks and "..".
	char resolved[PATH_MAX];
	if (!realpath(path, resolved) || strlen(resolved) >= nMaxBytes) {
		return false;
	}

	strcpy(pOut, resolved);
	return true;
#else
	pOut[0] = '\0';
	return false;
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
			// dimhotepus: Use cross-platform minidump.
			if ( !s1::breakpad::ScopedBreakpadHandler::Instance().WriteMinidump() )
			{
				Plat_DebugString( "Unable to write minidump on exit process by error.\n" );
			}
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
#if defined( _WIN32 )
	return (IsDebuggerPresent() != 0);
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
    RunCodeAtScopeExit(FreeSid(administratorsGroup));

    if (!CheckTokenMembership(nullptr, administratorsGroup, &ok)) {
      ok = FALSE;
    }
  }

  return ok;
}
#endif

// dimhotepus: Additional bug info.
bool Plat_IsUserAnAdmin() {
#if defined(_WIN32)
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
	if (::RegOpenKeyExW(HKEY_CURRENT_USER,
		L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
		0, KEY_READ, &personalizeKey) == ERROR_SUCCESS) {
		RunCodeAtScopeExit(::RegCloseKey(personalizeKey));

		DWORD valueType, valueSize = sizeof(value);

		const LSTATUS ok{::RegQueryValueExW(personalizeKey,
			L"AppsUseLightTheme",
			nullptr,
			&valueType,
			reinterpret_cast<byte*>(&value),
			&valueSize)};

		// Light mode by default if not supported.
		value = ok == ERROR_SUCCESS && valueType == REG_DWORD ? value : 1;
	}

	return value > 0;
}

PLATFORM_INTERFACE void* Plat_Alloc( unsigned long size );
PLATFORM_INTERFACE void Plat_Free( void *ptr );

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
	
	// Restore last error code. We can fail a lot, not needed to pollute global one.
	const auto lastErrorCode = GetLastError();
	RunCodeAtScopeExit(SetLastError(lastErrorCode));

	bool ok = true;
	
	// dimhotepus: Since Windows 11 21H1 (22000) SDK
	// DWM has DWMWA_USE_IMMERSIVE_DARK_MODE & DWMWA_SYSTEMBACKDROP_TYPE.
#if defined(WDK_NTDDI_VERSION) && (WDK_NTDDI_VERSION >= NTDDI_WIN11_ZN)
	// Detect current app window text is light or dark and apply Dark mode.
	const BOOL enableDarkMode{ShouldAppUseLightTheme() ? FALSE : TRUE};
	HRESULT hr{::DwmSetWindowAttribute(static_cast<HWND>(window),
		DWMWA_USE_IMMERSIVE_DARK_MODE, &enableDarkMode, sizeof(enableDarkMode))};
	if (FAILED(hr)) {
		const int windowTextSize =
			GetWindowTextLengthA(static_cast<HWND>(window)) + 1;
		char *windowText = static_cast<char*>(Plat_Alloc(windowTextSize));
		RunCodeAtScopeExit(Plat_Free(windowText));

		if (windowText)
		{
			GetWindowTextA(static_cast<HWND>(window), windowText, windowTextSize);
		}

		Warning(
			"Unable to apply user Color Mode preferences to window 0x%p (%s): %s\n",
			window, windowText ? windowText : "N/A",
			s1::win::com::com_error_category().message(hr).c_str());

		ok = false;

		// Invalid handle means no sense to continue.
		if (hr == E_HANDLE)
		{
			return ok;
		}
	}

	const DWM_SYSTEMBACKDROP_TYPE dwm_backdrop_type =
		static_cast<DWM_SYSTEMBACKDROP_TYPE>(to_underlying(backDropType));
	hr = ::DwmSetWindowAttribute(
		static_cast<HWND>(window), DWMWA_SYSTEMBACKDROP_TYPE,
		&dwm_backdrop_type, sizeof(dwm_backdrop_type));
	if (FAILED(hr)) {
		const int windowTextSize =
			GetWindowTextLengthA(static_cast<HWND>(window)) + 1;
		char *windowText = static_cast<char*>(Plat_Alloc(windowTextSize));
		RunCodeAtScopeExit(Plat_Free(windowText));

		if (windowText)
		{
			GetWindowTextA(static_cast<HWND>(window), windowText, windowTextSize);
		}

		Warning(
			"Unable to apply backdrop material effect to window 0x%p (%s): %s\n",
			window, windowText ? windowText : "N/A",
			s1::win::com::com_error_category().message(hr).c_str());

		ok = false;
	}
#else
	ok = false;
#endif

	return ok;
}


void Plat_DebugString( const char * psz )
{
#if defined( _WIN32 )
	::OutputDebugStringA( psz );
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
