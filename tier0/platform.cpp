// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"

#include "tier0/platform.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>  // std::_Exit
#include <ctime>
#include <system_error>

#include "winlite.h"
#include <dwmapi.h>
#include "windows/com_error_category.h"

#include "tier0/vcrmode.h"
#include "tier0/scoped_breakpad_handler.h"

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
#include "tier0/memalloc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#endif

extern VCRMode_t g_VCRMode;
static LARGE_INTEGER g_PerformanceFrequency;
static double g_PerformanceCounterToS;
static double g_PerformanceCounterToMS;
static double g_PerformanceCounterToUS;
static LARGE_INTEGER g_ClockStart;
// dimhotepus: bool -> std::atomic_bool.  Read by the Asserts on any thread
// while InitTime writes it, which was a data race.
static std::atomic_bool s_bTimeInitted = false;

// Benchmark mode uses this heavy-handed method 
static std::atomic_bool g_bBenchmarkMode = false;
// dimhotepus: double accumulator -> atomic tick counter.  Timer functions are
// called from many threads.
static std::atomic<uint64> g_FakeBenchmarkTicks = 0;
constexpr double g_FakeBenchmarkTimeInc = 1.0 / 66.0;

namespace {

// dimhotepus: Advance benchmark time by one tick and return it in seconds.
[[nodiscard]] double NextFakeBenchmarkTime() noexcept
{
	const uint64 ticks{ g_FakeBenchmarkTicks.fetch_add( 1, std::memory_order_relaxed ) + 1 };
	return static_cast<double>( ticks ) * g_FakeBenchmarkTimeInc;
}

// dimhotepus: Convert non-negative double to uint32 with wraparound (like
// GetTickCount).  static_cast<uint32>( double ) is undefined behavior once the
// value exceeds UINT32_MAX, which Plat_MSTime hits after ~49.7 days.
[[nodiscard]] constexpr uint32 WrapToUint32( double value ) noexcept
{
	return static_cast<uint32>( static_cast<uint64>( value ) );
}

}  // namespace

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
		const double frequencyD{ static_cast<double>( frequency ) };
		g_PerformanceCounterToS = 1.0 / frequencyD;
		g_PerformanceCounterToMS = 1e3 / frequencyD;
		g_PerformanceCounterToUS = 1e6 / frequencyD;
	}

	QueryPerformanceCounter(&g_ClockStart);

	// dimhotepus: Race, set to initted only when time set.
	// dimhotepus: Release, so a thread seeing true also sees the values above.
	s_bTimeInitted.store( true, std::memory_order_release );
}

MUST_CHECK_RETURN bool Plat_IsInBenchmarkMode()
{
	return g_bBenchmarkMode.load(std::memory_order_relaxed);
}

void Plat_SetBenchmarkMode( bool bBenchmark )
{
	g_bBenchmarkMode.store(bBenchmark, std::memory_order_relaxed);
}

static long long Plat_CycleTime()
{
	Assert( s_bTimeInitted.load( std::memory_order_acquire ) );

	LARGE_INTEGER now;
	QueryPerformanceCounter( &now );

	return now.QuadPart - g_ClockStart.QuadPart;
}

MUST_CHECK_RETURN double Plat_FloatTime()
{
	if ( !g_bBenchmarkMode.load(std::memory_order_relaxed) )
	{
		Assert( s_bTimeInitted.load( std::memory_order_acquire ) );

		double seconds = static_cast<double>( Plat_CycleTime() ) * g_PerformanceCounterToS;
		return seconds;
	}

	// dimhotepus: Thread-safe fake time.
	return NextFakeBenchmarkTime();
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

// Converts UTF-8 to UTF-16, truncating (at a code point boundary) instead of
// failing when the text is too long for the buffer.
template <std::size_t size>
wchar_t* Utf8ToWide( IN_Z const char* utf8, OUT_Z_ARRAY wchar_t ( &buffer )[size] ) noexcept
{
	// dimhotepus: The failure path copies L"N/A" (4).
	static_assert( size >= 4 && size <= INT_MAX );
 
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

// dimhotepus: nullptr for an empty string, so TaskDialog / MessageBoxW show
// their default caption instead of a blank title bar.
[[nodiscard]] constexpr const wchar_t* NullIfEmpty( const wchar_t* text ) noexcept
{
	return text && *text ? text : nullptr;
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

// dimhotepus: Type-checked GetProcAddress.  Casting through void (*)() is the
// form compilers accept without cast-function-type warnings.
template <typename TFunction>
[[nodiscard]] TFunction GetProcAddressAs( HMODULE module, const char* name ) noexcept
{
	static_assert( std::is_pointer_v<TFunction> &&
		std::is_function_v<std::remove_pointer_t<TFunction>> );

	return module
		? reinterpret_cast<TFunction>( reinterpret_cast<void ( * )()>( ::GetProcAddress( module, name ) ) )
		: nullptr;
}

}  // namespace

Plat_MessageBoxButton Plat_MessageBox( IN_Z const char* title,
									   IN_Z const char* mainInstruction,
									   IN_Z const char* content,
									   Plat_MessageBoxButton buttons,
									   Plat_MessageBoxIcon icon )
{
	wchar_t wideTitle[512], wideMainInstruction[512], wideContent[512];
	int pressed{ 0 };

	const HRESULT hr{ ::TaskDialog(
		nullptr,
		nullptr,
		NullIfEmpty( Utf8ToWide( title, wideTitle ) ),
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
 
	pressed = ::MessageBoxW( nullptr, combined, NullIfEmpty( wideTitle ),
		MB_SYSTEMMODAL | MB_SETFOREGROUND |
		GetMessageBoxIcon( icon ) | GetMessageBoxButtons( buttons ) );
	return GetPressedButton( pressed );
}

MUST_CHECK_RETURN uint32 Plat_MSTime()
{
	if ( !g_bBenchmarkMode.load(std::memory_order_relaxed) )
	{
		Assert( s_bTimeInitted.load( std::memory_order_acquire ) );

		double ms = static_cast<double>( Plat_CycleTime() ) * g_PerformanceCounterToMS;
		// dimhotepus: Wrap instead of UB after ~49.7 days.
		return WrapToUint32( ms );
	}

	// dimhotepus: Thread-safe fake time, wrap instead of UB.
	return WrapToUint32( NextFakeBenchmarkTime() * 1000.0 );
}

MUST_CHECK_RETURN uint64 Plat_USTime()
{
	if ( !g_bBenchmarkMode.load(std::memory_order_relaxed) )
	{
		Assert( s_bTimeInitted.load( std::memory_order_acquire ) );

		double us = static_cast<double>( Plat_CycleTime() ) * g_PerformanceCounterToUS;
		return static_cast<uint64>( us );
	}

	// dimhotepus: Thread-safe fake time.
	return static_cast<uint64>( NextFakeBenchmarkTime() * 1e6 );
}

void GetCurrentDate( int *pDay, int *pMonth, int *pYear )
{
	// dimhotepus: localtime -> localtime_s.
	const time_t now{ time( nullptr ) };

	struct tm local = {};
	if ( localtime_s( &local, &now ) != 0 )
	{
		AssertMsg( false, "localtime_s failed for time %lld.", static_cast<long long>( now ) );
		*pDay = 0;
		*pMonth = 0;
		*pYear = 0;
		return;
	}

	*pDay = local.tm_mday;
	*pMonth = local.tm_mon + 1;
	*pYear = local.tm_year + 1900;
}


// Wraps the thread-safe versions of ctime. buf must be at least 26 bytes 
// dimhotepus: Check for any error, not only EINVAL.  ctime_s returns ERANGE
// when buf is shorter than 26 bytes, and the old code returned buf anyway.
char *Plat_ctime( const time_t *timep, char *buf, size_t bufsize )
{
	return ctime_s( buf, bufsize, timep ) == 0 ? buf : nullptr;
}

MUST_CHECK_RETURN bool Plat_GetModuleFilename( char *pOut, int nMaxBytes )
{
	// dimhotepus: Reject null / non-positive sizes.  A negative nMaxBytes was
	// cast to a huge DWORD and let GetModuleFileName overrun pOut.
	if ( !pOut || nMaxBytes <= 0 )
	{
		return false;
	}

	// dimhotepus: GetModuleFileName -> GetModuleFileNameA.
	const DWORD size{ static_cast<DWORD>( nMaxBytes ) };
	const DWORD length{ ::GetModuleFileNameA( nullptr, pOut, size ) };

	if ( length == 0 )
	{
		// dimhotepus: Don't leave pOut unterminated on failure.
		pOut[0] = '\0';
		return false;
	}

	// A truncated path is reported as success with length == size.
	// dimhotepus: length > 0 is already handled above; compare against the
	// validated DWORD size.
	return length < size;
}

void Plat_ExitProcess( int nCode )
{
	// We don't want global destructors in our process OR in any DLL to get executed.
	// _exit() avoids calling global destructors in our module, but not in other DLLs.
	// dimhotepus: Drop UB on gc.
	// const char *pchCmdLineA = Plat_GetCommandLineA();
	// if ( nCode || ( strstr( pchCmdLineA, "gc.exe" ) && strstr( pchCmdLineA, "gc.dll" ) && strstr( pchCmdLineA, "-gc" ) ) )
	// {
	// 	int *x = nullptr; *x = 1; // cause a hard crash, GC is not allowed to exit voluntarily from gc.dll //-V522
	// }
	::TerminateProcess( ::GetCurrentProcess(), static_cast<UINT>( nCode ) );

	// dimhotepus: Plat_ExitProcess is [[noreturn]], so returning when
	// TerminateProcess fails is undefined behavior.  Fall back to _Exit, which
	// at least skips our module's destructors and atexit handlers.
	std::_Exit( nCode );
}

void Plat_ExitProcessWithError( int nCode, bool bGenerateMinidump )
{
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

// Wraps the thread-safe versions of gmtime
// dimhotepus: Check for any error, not only EINVAL.
struct tm *Plat_gmtime( const time_t *timep, struct tm *result )
{
	return gmtime_s( result, timep ) == 0 ? result : nullptr;
}


time_t Plat_timegm( struct tm *timeptr )
{
	return _mkgmtime( timeptr );
}


// Wraps the thread-safe versions of localtime
// dimhotepus: Check for any error, not only EINVAL.
struct tm *Plat_localtime( const time_t *timep, struct tm *result )
{
	return localtime_s( result, timep ) == 0 ? result : nullptr;
}


MUST_CHECK_RETURN bool vtune( bool resume )
{
	// dimhotepus: Two static bools -> one function-local static initialized by
	// a lambda.  C++11 guarantees that runs exactly once even when called from
	// several threads; the old "if ( !bInitialized )" check was a data race.
	// The DLL is intentionally never freed: the pointers are used for the
	// process lifetime.
	struct VTuneApi
	{
		void ( __cdecl *resume )();
		void ( __cdecl *pause )();
	};
	static const VTuneApi api = []() noexcept
	{
		const HMODULE vtuneDll{ ::LoadLibraryA( "vtuneapi.dll" ) };
		return VTuneApi{
			GetProcAddressAs<void ( __cdecl * )()>( vtuneDll, "VTResume" ),
			GetProcAddressAs<void ( __cdecl * )()>( vtuneDll, "VTPause" ) };
	}();

	// Call the appropriate function, as indicated by the argument:
	// dimhotepus: VTResume / VTPause statics -> api.resume / api.pause.
	if( resume && api.resume )
	{
		api.resume();
		return true;
	}

	if( !resume && api.pause )
	{
		api.pause();
		return true;
	}

	return false;
}

MUST_CHECK_RETURN bool Plat_IsInDebugSession()
{
	return IsDebuggerPresent() != 0;
}

// dimhotepus: Additional bug info.
// See https://docs.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-checktokenmembership
MUST_CHECK_RETURN static BOOL IsUserAdmin()
{
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

// dimhotepus: Additional bug info.
MUST_CHECK_RETURN bool Plat_IsUserAnAdmin()
{
  return ::IsUserAdmin() ? true : false;
}

// dimhotepus: Forward declarations kept for binary / source compatibility.
PLATFORM_INTERFACE MUST_CHECK_RETURN void *Plat_Alloc( unsigned long size );
PLATFORM_INTERFACE void Plat_Free( void *ptr );

#if defined(WDK_NTDDI_VERSION) && defined(NTDDI_WIN11_ZN) && \
	(WDK_NTDDI_VERSION >= NTDDI_WIN11_ZN)
#define SE_PLATFORM_HAS_DWM_SYSTEM_BACKDROP 1
#endif

#ifdef SE_PLATFORM_HAS_DWM_SYSTEM_BACKDROP

[[nodiscard]] static bool ShouldAppUseLightTheme() {
	DWORD value{0}, valueSize{sizeof(value)};
	const LSTATUS rc{::RegGetValueW(HKEY_CURRENT_USER,
		L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
		L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &valueSize)};

	// Light mode by default if not supported.
	return rc != ERROR_SUCCESS || value != 0;
}

template <std::size_t size>
[[nodiscard]] static const char *GetWindowTitleForLog(HWND window, char (&buffer)[size]) {
	static_assert(size > 1 && size <= INT_MAX);

	buffer[0] = '\0';
	return ::GetWindowTextA(window, buffer, static_cast<int>(size)) > 0
		? buffer : "N/A";
}

#endif  // SE_PLATFORM_HAS_DWM_SYSTEM_BACKDROP

// dimhotepus: Apply system Mica materials to window.
// See https://learn.microsoft.com/en-us/windows/apps/design/style/mica
bool Plat_ApplySystemTitleBarTheme([[maybe_unused]] void *window,
                                   [[maybe_unused]] SystemBackdropType backDropType) {
#ifdef SE_PLATFORM_HAS_DWM_SYSTEM_BACKDROP
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

	const HWND hwnd{static_cast<HWND>(window)};
	bool ok = true;

	// Detect current app window text is light or dark and apply Dark mode.
	const BOOL enableDarkMode{ShouldAppUseLightTheme() ? FALSE : TRUE};
	HRESULT hr{::DwmSetWindowAttribute(hwnd,
		DWMWA_USE_IMMERSIVE_DARK_MODE, &enableDarkMode, sizeof(enableDarkMode))};
	if (FAILED(hr)) {
		// dimhotepus: truncated if longer, never fails.
		char windowText[256];
		Warning(
			"Unable to apply user Color Mode preferences to window 0x%p (%s): %s\n",
			window, GetWindowTitleForLog(hwnd, windowText),
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
		hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
		&dwm_backdrop_type, sizeof(dwm_backdrop_type));
	if (FAILED(hr)) {
		// dimhotepus: truncated if longer, never fails.
		char windowText[256];
		Warning(
			"Unable to apply backdrop material effect to window 0x%p (%s): %s\n",
			window, GetWindowTitleForLog(hwnd, windowText),
			s1::win::com::com_error_category().message(hr).c_str());

		ok = false;
	}

	return ok;
#else
	return false;
#endif
}


void Plat_DebugString( const char * psz )
{
	::OutputDebugStringA( psz );
}


MUST_CHECK_RETURN const tchar *Plat_GetCommandLine()
{
#ifdef TCHAR_IS_WCHAR
	return GetCommandLineW();
#else
	// dimhotepus: GetCommandLine -> GetCommandLineA.  The TCHAR macro returns
	// LPWSTR when UNICODE is defined, which doesn't convert to const char *.
	return GetCommandLineA();
#endif
}

MUST_CHECK_RETURN bool GetMemoryInformation( MemoryInformation *pOutMemoryInfo )
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


MUST_CHECK_RETURN const char *Plat_GetCommandLineA()
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
MUST_CHECK_RETURN int Plat_GetWatchdogTime( void )
{
	return 0;
}
void Plat_SetWatchdogHandlerFunction( [[maybe_unused]] Plat_WatchDogHandlerFunction_t function )
{
}

MUST_CHECK_RETURN bool Is64BitOS()
{
	// dimhotepus: Cleanup x86-64 process detection.
#ifdef _WIN64
  return true;
#else
	// dimhotepus: Prefer IsWow64Process2 (Windows 10 1511+).  IsWow64Process
	// reports FALSE for x86 processes on Windows on ARM64, so the old code
	// called a 64-bit ARM OS 32-bit.
	static const bool is64BitOS = []() noexcept
	{
		using IsWow64Process2Fn = BOOL ( WINAPI * )( HANDLE, USHORT *, USHORT * );

		const auto isWow64Process2 = GetProcAddressAs<IsWow64Process2Fn>(
			::GetModuleHandleW( L"kernel32.dll" ), "IsWow64Process2" );
		if ( isWow64Process2 )
		{
			USHORT processMachine{ IMAGE_FILE_MACHINE_UNKNOWN };
			USHORT nativeMachine{ IMAGE_FILE_MACHINE_UNKNOWN };
			if ( isWow64Process2( ::GetCurrentProcess(), &processMachine, &nativeMachine ) )
			{
				return nativeMachine == IMAGE_FILE_MACHINE_AMD64 ||
					   nativeMachine == IMAGE_FILE_MACHINE_ARM64 ||
					   nativeMachine == IMAGE_FILE_MACHINE_IA64;
			}
		}

		BOOL isWow64{ FALSE };
		return ::IsWow64Process( ::GetCurrentProcess(), &isWow64 ) && isWow64;
	}();

	return is64BitOS;
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

static std::atomic<Plat_AllocErrorFn> g_AllocError = Plat_DefaultAllocErrorFn;
#endif

// dimhotepus: Under STEAM, valve_on.h (re-enabled at the end of platform.h)
// #defines malloc / realloc / free as function-like macros that expand to
// HEY_DONT_USE_* errors.  Parenthesizing the name, "(malloc)( size )",
// suppresses function-like macro expansion and calls the real CRT function.

PLATFORM_INTERFACE MUST_CHECK_RETURN void *Plat_Alloc( unsigned long size )
{
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
	void *pRet = g_pMemAlloc->Alloc( size );
#else
	// dimhotepus: (malloc) bypasses valve_on.h's malloc() macro under STEAM.
	void *pRet = (malloc)( size );
#endif

	if ( pRet )
	{
		return pRet;
	}

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
	// dimhotepus: nullptr for a 0-byte request is not an allocation failure.
	if ( size != 0 )
	{
		const auto allocErrorFn = g_AllocError.load( std::memory_order_acquire );
		if ( allocErrorFn )
		{
			allocErrorFn( size );
		}
	}
#endif
	return nullptr;
}

PLATFORM_INTERFACE MUST_CHECK_RETURN void *Plat_Realloc( void *ptr, unsigned long size )
{
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
	void *pRet = g_pMemAlloc->Realloc( ptr, size );
#else
	// dimhotepus: (realloc) bypasses valve_on.h's realloc() macro under STEAM.
	void *pRet = (realloc)( ptr, size );
#endif

	if ( pRet )
	{
		return pRet;
	}

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
	// dimhotepus: realloc( ptr, 0 ) frees ptr and may return nullptr; that is
	// not an allocation failure.
	if ( size != 0 )
	{
		const auto allocErrorFn = g_AllocError.load( std::memory_order_acquire );
		if ( allocErrorFn )
		{
			allocErrorFn( size );
		}
	}
#endif
	return nullptr;
}

PLATFORM_INTERFACE void Plat_Free( void *ptr )
{
#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
	g_pMemAlloc->Free( ptr );
#else
	// dimhotepus: (free) bypasses valve_on.h's free() macro under STEAM.
	(free)( ptr );
#endif
}

#if !defined(STEAM) && !defined(NO_MALLOC_OVERRIDE)
PLATFORM_INTERFACE void Plat_SetAllocErrorFn( Plat_AllocErrorFn fn )
{
	g_AllocError.store( fn, std::memory_order_relaxed );
}
#endif