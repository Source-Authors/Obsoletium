// Copyright Valve Corporation, All rights reserved.

#ifndef SE_LAUNCHER_SCOPED_APP_RELAUNCH_H
#define SE_LAUNCHER_SCOPED_APP_RELAUNCH_H

#ifdef _WIN32
#include <sal.h>
#include <specstrings.h> // __out_data_source

#include <array> // std::size
#elif defined( LINUX ) || defined( OSX )
#include <sys/stat.h>
#include <sys/types.h>

#include "posix_file_stream.h"
#endif

#if defined( _WIN64 )
using ULONG_PTR = unsigned __int64;
#else
using ULONG_PTR = unsigned long;
#endif

FORWARD_DECLARE_HANDLE( HINSTANCE );
FORWARD_DECLARE_HANDLE( HKEY );
FORWARD_DECLARE_HANDLE( HWND );

using REGSAM = unsigned long;

extern "C"
{
__declspec( dllimport ) long __stdcall RegOpenKeyExW(
_In_ HKEY hKey, _In_opt_ const wchar_t *lpSubKey,
_In_opt_ unsigned long ulOptions, _In_ REGSAM samDesired,
_Out_ HKEY *phkResult );
__declspec( dllimport ) long __stdcall RegCloseKey( _In_ HKEY hKey );

__declspec( dllimport ) long __stdcall RegQueryValueExW(
_In_ HKEY hKey, _In_opt_ const wchar_t *lpValueName,
_Reserved_ unsigned long *lpReserved, _Out_opt_ unsigned long *lpType,
_Out_writes_bytes_to_opt_( *lpcbData, *lpcbData )
__out_data_source( REGISTRY ) unsigned char *lpData,
_When_( lpData == NULL, _Out_opt_ )
_When_( lpData != NULL, _Inout_opt_ ) unsigned long *lpcbData );
__declspec( dllimport ) long __stdcall RegDeleteValueW(
_In_ HKEY hKey, _In_opt_ const wchar_t *lpValueName );

__declspec( dllimport ) HINSTANCE __stdcall ShellExecuteW(
_In_opt_ HWND hwnd, _In_opt_ const wchar_t *lpOperation,
_In_ const wchar_t *lpFile, _In_opt_ const wchar_t *lpParameters,
_In_opt_ const wchar_t *lpDirectory, _In_ int nShowCmd );

} // extern "C"

namespace s1::launcher
{

// Scoped app relaunch.
class ScopedAppRelaunch
{
public:
	ScopedAppRelaunch() noexcept
	{
#ifndef WIN32
		struct stat st;
		if ( !stat( kRelaunchFilePath, &st ) )
		{
			if ( unlink( kRelaunchFilePath ) )
			{
				Warning( "Unable to remove relaunch file '%s': %s.\n", kRelaunchFilePath,
				         std::generic_category().message( errno ).c_str() );
			}
		}
#endif
	}

	ScopedAppRelaunch( const ScopedAppRelaunch & ) = delete;
	ScopedAppRelaunch( ScopedAppRelaunch && ) = delete;
	ScopedAppRelaunch &operator=( const ScopedAppRelaunch & ) = delete;
	ScopedAppRelaunch &operator=( ScopedAppRelaunch && ) = delete;

	~ScopedAppRelaunch() noexcept
	{
#ifndef WIN32
		struct stat st;
		if ( stat( kRelaunchFilePath, &st ) )
		{
			return;
		}

		auto [fp, rc] =
		s1::posix::posix_file_stream_factory::open( kRelaunchFilePath, "r" );
		if ( !rc )
		{
			char   cmd[256];
			size_t read;
			std::tie( read, rc ) = fp.read( cmd );

			if ( !rc && read > 0 )
			{
				char open_line[MAX_PATH];

#if defined( LINUX )
				V_sprintf_safe( open_line, "xdg-open \"%s\"", cmd );
#else
				V_sprintf_safe( open_line, "open \"%s\"", cmd );
#endif

				system( open_line );
			}
		}
#else
		// Now that the mutex has been released, check
		// HKEY_CURRENT_USER\Software\Valve\Source\Relaunch URL.  If there is a URL
		// here, exec it.  This supports the capability of immediately re-launching
		// the the game via Steam in a different audio language.
		if ( HKEY key;
		     ::RegOpenKeyExW( kHKEY_CURRENT_USER, L"Software\\Valve\\Source", 0,
		                      kKEY_READ | kKEY_WRITE, &key ) == kERROR_SUCCESS )
		{
			RunCodeAtScopeExit( ::RegCloseKey( key ) );

			wchar_t value[MAX_PATH];
			// in bytes + leave room for L'\0'
			unsigned long value_type, value_size = sizeof( value ) - sizeof( wchar_t );

			if ( RegQueryValueExW( key, L"Relaunch URL", nullptr, &value_type,
			                       reinterpret_cast< unsigned char * >( value ),
			                       &value_size ) == kERROR_SUCCESS &&
			     value_type == kREG_SZ )
			{
				RunCodeAtScopeExit( RegDeleteValueW( key, L"Relaunch URL" ) );

				// Ensure always zero-terminate.
				value[value_size / sizeof( wchar_t )] = L'\0';

				// dimhotepus: Only steam:// protocol allowed.
				if ( StringHasPrefix( value, L"steam://" ) )
				{
					::ShellExecuteW( nullptr, L"open", value, nullptr, nullptr, kSW_SHOW );
				}
			}
		}
#endif
	}

private:
#ifndef WIN32
	static constexpr char kRelaunchFilePath[18] = { "/tmp/hl2_relaunch" };
#else
	static constexpr HKEY kHKEY_CURRENT_USER{
		( (HKEY)(ULONG_PTR)( (long)0x80000001 ) )
	};
	static constexpr long kERROR_SUCCESS{ 0L };
	static constexpr int  kSW_SHOW{ 5 };

	static constexpr unsigned long kREG_SZ{ 1ul }; // Unicode nul terminated string
	static constexpr long          kKEY_READ{ 131097L };
	static constexpr unsigned long kKEY_WRITE{ 131078L };
#endif
};

} // namespace s1::launcher

#endif // SE_LAUNCHER_SCOPED_APP_RELAUNCH_H
