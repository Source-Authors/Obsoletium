// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"

#include "tier0/valve_off.h"

#if defined( _WIN32 )
#include "winlite.h"
#include "windows/dpi_wnd_behavior.h"

// dimhotepus: Launcher icon id.
#include "../launcher_main/resource.h"
// dimhotepus: Remove launcher definition clashing with tier0 one.
#undef SE_PRODUCT_FILE_DESCRIPTION_STRING
#elif defined( POSIX )
#include <dlfcn.h>
#include <cstdlib>
#endif

#include <atomic>
#include <new>

#include "tier0/valve_on.h"

#include "resource.h"
#include "tier0/icommandline.h"
#include "tier0/threadtools.h"

#if defined( USE_SDL )
// We lazily load the SDL shared object, and only reference functions if it's
// available, so this can be included on the dedicated server too.
#include "include/SDL3/SDL.h"

using SDL_ShowMessageBoxFn = int ( SDLCALL * )( const SDL_MessageBoxData *messageboxdata, int *buttonid );
#endif

struct CDialogInitInfo
{
	CDialogInitInfo( const tchar *pFilename, const tchar *pExpression, int line )
		: m_Filename{ pFilename }, m_Expression{ pExpression }, m_iLine{ line }
	{
	}

	// dimhotepus: Use fixed buffers with copied data as we access from multiple threads.
	tstring      m_Filename;
	int          m_iLine;
	tstring      m_Expression;
};

struct CAssertDisable
{
	CAssertDisable( const tstring &filename,
		int line_min,
		int line_max,
		unsigned ignore_times,
		CAssertDisable *pNext ) :
		m_LineMin{ line_min },
		m_LineMax{ line_max },
		m_nIgnoreTimes{ ignore_times },
		m_pNext{ pNext },
		m_Filename{ filename }
	{
	}

	static constexpr inline int kIgnore = -1;

	const tstring m_Filename;
	
	// If these are not kIgnore, then this CAssertDisable only disables asserts
	// on lines between these values (inclusive).
	const int m_LineMin;
	const int m_LineMax;
	
	// Decremented each time we hit this assert and ignore it, until it's 0.
	// Then the CAssertDisable is removed.
	// If this is 0, then we always ignore this assert.
	unsigned m_nIgnoreTimes;

	CAssertDisable *m_pNext;
};

// dimhotepus: Make all this things thread-safe as may be used from different threads.
static std::atomic_bool g_bAssertsEnabled = true;

// -------------------------------------------------------------------------------- //
// Internal functions.
// -------------------------------------------------------------------------------- //

#if defined( _WIN32 ) && !defined(STATIC_TIER0)
extern "C" BOOL APIENTRY MemDbgDllMain( HMODULE hDll, DWORD dwReason, void* );

extern void InitTime();

BOOL WINAPI DllMain( HINSTANCE module, DWORD fdwReason, LPVOID lpvReserved )
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		InitTime();

		// dimhotepus: Do not notify on thread creation for performance.
		DisableThreadLibraryCalls(module);
	}

#ifndef NDEBUG
	MemDbgDllMain( module, fdwReason, lpvReserved );
#endif

	return TRUE;
}
#endif

[[nodiscard]] static bool IsDebugBreakEnabled()
{
	static bool bResult = CommandLine()->HasParm( _T("-debugbreak") ) ||
		CommandLine()->HasParm( _T("-raiseonassert") ) ||
		getenv( "RAISE_ON_ASSERT" );
	return bResult;
}

[[nodiscard]] static bool AreAssertsDisabled()
{
	static bool bResult = CommandLine()->HasParm( _T("-noassert") );
	return bResult;
}

[[nodiscard]] static bool IsConsoleMode()
{
	// No graphics and sound in launcher.
	static bool bResult = CommandLine()->HasParm( _T("-textmode") ) ||
		// Console mode for dedicated server.
		CommandLine()->HasParm( _T("-console") );
	return bResult;
}

[[nodiscard]] static bool AreAssertsEnabledInFileLine( CAssertDisable *&pAssertDisables, const tchar *pFilename, int iLine )
{
	CAssertDisable **pPrev{ &pAssertDisables };
	for ( CAssertDisable *it{ pAssertDisables }, *pNext = nullptr; it; it = pNext )
	{
		pNext = it->m_pNext;

		if ( _tcsicmp( pFilename, it->m_Filename.c_str() ) == 0 )
		{
			bool bAssertsEnabled = true;

			// dimhotepus: Use else to check line case.
			if ( it->m_LineMin == CAssertDisable::kIgnore && it->m_LineMax == CAssertDisable::kIgnore )
			{
				// Are asserts disabled in the whole file?
				bAssertsEnabled = false;
			}
			else if ( iLine >= it->m_LineMin && iLine <= it->m_LineMax )
			{
				// Are asserts disabled on the specified line?
				bAssertsEnabled = false;
			}

			if ( !bAssertsEnabled )
			{
				// If this assert is only disabled for the next N times, then countdown..
				if ( it->m_nIgnoreTimes > 0 )
				{
					--it->m_nIgnoreTimes;
					if ( it->m_nIgnoreTimes == 0 )
					{
						// Remove this one from the list.
						*pPrev = pNext;
						delete it;
						// dimhotepus: This assert should be ignored this time, but not next one.
						return false;
					}
				}
				
				return false;
			}
		}

		pPrev = &it->m_pNext;
	}

	return true;
}

static CAssertDisable *CreateNewAssertDisable( CAssertDisable *&pAssertDisables,
	const tstring &filename,
	int lineMin,
	int lineMax )
{
	// dimhotepus: Use passed file name to create assert disable.
	auto *pDisable = new ( std::nothrow ) CAssertDisable{
		filename, lineMin, lineMax, 0, pAssertDisables };
	if ( !pDisable ) return nullptr;

	pAssertDisables = pDisable;

	return pDisable;
}

static void IgnoreAssertsInCurrentFile( CAssertDisable *&pAssertDisables, const CDialogInitInfo &info )
{
	CreateNewAssertDisable( pAssertDisables, info.m_Filename, CAssertDisable::kIgnore, CAssertDisable::kIgnore );
}

static CAssertDisable *IgnoreAssertsNearby( CAssertDisable *&pAssertDisables, const CDialogInitInfo &info, int nRange )
{
	return CreateNewAssertDisable(
		pAssertDisables, info.m_Filename, info.m_iLine - nRange, info.m_iLine + nRange );
}

#if defined( _WIN32 )
struct AssertDialogData
{
	unsigned                       &last_ignore_num_times;
	CDialogInitInfo                 init_info;
	CAssertDisable                *&assert_disables;
	bool                            &should_break;
	std::atomic_bool                &are_asserts_enabled;
	s1::win::ui::CDpiWindowBehavior dpi_window_behavior{ false };
};

static INT_PTR CALLBACK AssertDialogProc(
  HWND hDlg,                      // handle to dialog box
  UINT uMsg,                      // message
  WPARAM wParam,                  // first message parameter
  LPARAM lParam                   // second message parameter
)
{
	AssertDialogData *pDialogData = nullptr;

	switch( uMsg )
	{
		case WM_INITDIALOG:
		{
			pDialogData = reinterpret_cast<AssertDialogData *>( lParam );
			assert( pDialogData );

			// Store it for later use
			SetWindowLongPtr( hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( pDialogData ) );

			SetDlgItemText( hDlg, IDC_ASSERT_MSG_CTRL, pDialogData->init_info.m_Expression.c_str() );
			SetDlgItemText( hDlg, IDC_FILENAME_CONTROL, pDialogData->init_info.m_Filename.c_str() );
			SetDlgItemInt( hDlg, IDC_LINE_CONTROL, pDialogData->init_info.m_iLine, FALSE );
			SetDlgItemInt( hDlg, IDC_IGNORE_NUMLINES, 5, FALSE );
			SetDlgItemInt( hDlg, IDC_IGNORE_NUMTIMES, pDialogData->last_ignore_num_times, FALSE );

			// dimhotepus: Honor per monitor V2 DPI.
			// It is DialogBoxParam (uses CreateWindowEx), so need scaling.
			pDialogData->dpi_window_behavior.OnCreateWindow( hDlg );

			// dimhotepus: Add launcher icon for Assert dialog.
			HANDLE hExeIcon = LoadImageW(
				GetModuleHandleW( nullptr ),
				MAKEINTRESOURCEW( SE_IDI_APP_MAIN ),
				IMAGE_ICON, 0, 0, LR_SHARED );
			SendMessage( hDlg, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>( hExeIcon ) );
		
			// Center the dialog.
			RECT rcDlg, rcDesktop;
			GetWindowRect( hDlg, &rcDlg );
			GetWindowRect( GetDesktopWindow(), &rcDesktop );
			SetWindowPos( 
				hDlg, 
				HWND_TOP, 
				((rcDesktop.right-rcDesktop.left) - (rcDlg.right-rcDlg.left)) / 2,
				((rcDesktop.bottom-rcDesktop.top) - (rcDlg.bottom-rcDlg.top)) / 2,
				0,
				0,
				SWP_NOSIZE | SWP_NOACTIVATE );
		}
		return TRUE;

		case WM_COMMAND:
		{
			pDialogData = reinterpret_cast<AssertDialogData *>( GetWindowLongPtr( hDlg, GWLP_USERDATA ) );
			assert( pDialogData );

			switch( LOWORD( wParam ) )
			{
				case IDC_IGNORE_FILE:
				{
					IgnoreAssertsInCurrentFile( pDialogData->assert_disables, pDialogData->init_info );
					EndDialog( hDlg, 0 );
					return TRUE;
				}

				// Ignore this assert N times.
				case IDC_IGNORE_THIS:
				{
					BOOL bTranslated = false;
					UINT value = GetDlgItemInt( hDlg, IDC_IGNORE_NUMTIMES, &bTranslated, FALSE );
					if ( bTranslated && value > 1 )
					{
						CAssertDisable *pDisable = IgnoreAssertsNearby(
							pDialogData->assert_disables, pDialogData->init_info, 0 );
						if ( pDisable )
						{
							pDisable->m_nIgnoreTimes = value - 1;
						}
						pDialogData->last_ignore_num_times = value;
					}

					EndDialog( hDlg, 0 );
					return TRUE;
				}

				// Always ignore this assert.
				case IDC_IGNORE_ALWAYS:
				{
					IgnoreAssertsNearby( pDialogData->assert_disables, pDialogData->init_info, 0 );
					EndDialog( hDlg, 0 );
					return TRUE;
				}
				
				case IDC_IGNORE_NEARBY:
				{
					BOOL bTranslated = false;
					UINT value = GetDlgItemInt( hDlg, IDC_IGNORE_NUMLINES, &bTranslated, FALSE );
					// dimhotepus: Check nearby value is in int range.
					if ( !bTranslated || value < 1 || value > static_cast<unsigned>( std::numeric_limits<int>::max() ) )
						return TRUE;

					IgnoreAssertsNearby( pDialogData->assert_disables, pDialogData->init_info, value );
					EndDialog( hDlg, 0 );
					return TRUE;
				}

				case IDC_IGNORE_ALL:
				{
					pDialogData->are_asserts_enabled.store(false, std::memory_order_release);
					EndDialog( hDlg, 0 );
					return TRUE;
				}

				case IDC_BREAK:
				{
					pDialogData->should_break = true;
					EndDialog( hDlg, 0 );
					return TRUE;
				}

				// dimhotepus: Explicitly handle missed commands.
				case IDC_IGNORE_NUMLINES:
				case IDC_IGNORE_NUMTIMES:
					return TRUE;

				// dimhotepus: Explicitly handle ESC.
				case IDC_ESCAPE_DIALOG:
				{
					// Ignore this assert.
					EndDialog( hDlg, 0 );
					return TRUE;
				}

				default:
				{
					// dimhotepus: Cant use Assert here as recursive assert causes crash.
					DebuggerBreakIfDebugging();
					return FALSE;
				}
			}
		}

		case WM_DPICHANGED:
		{
			pDialogData = reinterpret_cast<AssertDialogData *>( GetWindowLongPtr( hDlg, GWLP_USERDATA ) );
			assert( pDialogData );

			return !pDialogData->dpi_window_behavior.OnWindowDpiChanged(wParam, lParam)
				? TRUE : FALSE;
		}

		case WM_SETTINGCHANGE:
		{
			// dimhotepus: Handle Dark / Light mode change.
			const tchar *area = reinterpret_cast< const tchar * >( lParam );
			if ( area && _tcscmp( area, _T("ImmersiveColorSet") ) == 0 )
			{
				// dimhotepus: Reapply Dark or Light mode if any and Mica styles to window title bar.
				Plat_ApplySystemTitleBarTheme( hDlg, SystemBackdropType::TransientWindow );
				return TRUE;
			}
		}
		break;

		case WM_DESTROY:
		{
			pDialogData = reinterpret_cast<AssertDialogData *>( GetWindowLongPtr( hDlg, GWLP_USERDATA ) );
			assert( pDialogData );

			pDialogData->dpi_window_behavior.OnDestroyWindow();
			return TRUE;
		}

		default:
			// dimhotepus: Add default case.
			return FALSE;
	}

	return FALSE;
}

struct EnumerateWindowData
{
	EnumerateWindowData( DWORD process_id, HWND &best_parent_window ) :
		process_id{process_id},
		best_parent_window{best_parent_window}
	{
	}

	DWORD process_id;
	HWND  &best_parent_window;
};

static BOOL CALLBACK ParentWindowEnumProc(
	HWND hWnd,      // handle to parent window
	LPARAM lParam   // application-defined value
)
{
	if ( ::IsWindowVisible( hWnd ) )
	{
		DWORD process_id{0};
		const DWORD thread_id{ ::GetWindowThreadProcessId( hWnd, &process_id ) };
		if ( thread_id == 0 )
		{
			return TRUE;
		}

		auto *window_data = reinterpret_cast< EnumerateWindowData * >( lParam );
		assert( window_data );

		if ( process_id == window_data->process_id )
		{
			window_data->best_parent_window = hWnd;
			// Don't iterate any more.
			return FALSE;
		}
	}

	return TRUE;
}

[[nodiscard]] static HWND FindLikelyParentWindow()
{
	HWND                best_parent_window{ nullptr };
	EnumerateWindowData window_data{ ::GetCurrentProcessId(), best_parent_window };

	// Enumerate top-level windows and take the first visible one with our processID.
	::EnumWindows( ParentWindowEnumProc, reinterpret_cast<LPARAM>( &window_data ) );

	return best_parent_window;
}
#endif // defined( _WIN32 )

// -------------------------------------------------------------------------------- //
// Interface functions.
// -------------------------------------------------------------------------------- //

// provides access to the global that turns asserts on and off
DBG_INTERFACE bool AreAllAssertsDisabled()
{
	return !g_bAssertsEnabled.load( std::memory_order_acquire );
}

DBG_INTERFACE void SetAllAssertsDisabled( bool bAssertsDisabled )
{
	g_bAssertsEnabled.store(!bAssertsDisabled, std::memory_order_release);
}

#if defined( USE_SDL )
// dimhotepus: Make thread-safe.
std::atomic<SDL_Window *> g_SDLWindow = nullptr;

DBG_INTERFACE void SetAssertDialogParent( struct SDL_Window *window )
{
	g_SDLWindow.store( window, std::memory_order_release );
}

DBG_INTERFACE SDL_Window* GetAssertDialogParent()
{
	return g_SDLWindow.load( std::memory_order_acquire );
}
#endif

DBG_INTERFACE bool ShouldUseNewAssertDialog()
{
	static bool bMPIWorker = CommandLine()->HasParm( _T("-mpi_worker") );
	if ( bMPIWorker )
	{
		return false;
	}

#ifdef DBGFLAG_ASSERTDLG
	return true;		// always show an assert dialog
#else
	return Plat_IsInDebugSession();		// only show an assert dialog if the process is being debugged
#endif // DBGFLAG_ASSERTDLG
}

#if defined( POSIX )

#include <execinfo.h>

static void SpewBacktrace()
{
	void *buffer[ 16 ];
	int nptrs = backtrace( buffer, std::size( buffer ) );
	if ( nptrs )
	{
		char **strings = backtrace_symbols( buffer, nptrs );
		RunCodeAtScopeExitOpt( strings, free( strings ) );

		if ( strings )
		{
			for ( int i = 0; i < nptrs; i++)
			{
				const char *module = strrchr( strings[ i ], '/' );
				module = module ? ( module + 1 ) : strings[ i ];

				fprintf( stderr, "  %s\n", module );
			}
		}
	}
}

#endif

DBG_INTERFACE bool DoNewAssertDialog( const tchar *pFilename, int line, const tchar *pExpression )
{
	LOCAL_THREAD_LOCK();

	if ( AreAssertsDisabled() )
		return false;

	// Have ALL Asserts been disabled?
	if ( !g_bAssertsEnabled.load(std::memory_order_acquire) )
		return false;

	// Linked list of disabled asserts.
	static CAssertDisable *assert_disables{ nullptr };

	// Has this specific Assert been disabled?
	if ( !AreAssertsEnabledInFileLine( assert_disables, pFilename, line ) )
		return false;

	// Assert not suppressed. Spew it, and optionally a backtrace.
#if defined( POSIX )
	if( isatty( STDERR_FILENO ) )
	{
		#define COLOR_YELLOW 	"\033[1;33m"
		#define COLOR_GREEN 	"\033[1;32m"
		#define COLOR_RED 		"\033[1;31m"
		#define COLOR_END		"\033[0m"
		fprintf(stderr,
			COLOR_YELLOW "ASSERT:" COLOR_END "
			" COLOR_RED "%s" COLOR_GREEN ":%i:" COLOR_END
			" " COLOR_RED "%s" COLOR_END "\n",
			pFilename, line, pExpression);
		if ( getenv( "POSIX_ASSERT_BACKTRACE" ) )
		{
			SpewBacktrace();
		}
	}
	else
#endif
	{
		fprintf(stderr, "ASSERT: %s:%i: %s\n", pFilename, line, pExpression);
	}

	// If they have the old mode enabled (always break immediately), then just break right into
	// the debugger like we used to do.
	if ( IsDebugBreakEnabled() )
		return true;

	CDialogInitInfo init_info{ pFilename, pExpression, line };

	// Now create the dialog. Just return true for old-style debug break upon failure.

	// Set to true if they want to break in the debugger.
	bool should_break{ false };

	const auto consoleAssertFallback = [&]() {
		// dimhotepus: Use console fallback if no SDL or not POSIX.
		fprintf( stderr, "\n=== ASSERTION FAILED ===\n" );
		fprintf( stderr, "File: %s\nLine: %i\nExpr: %s\n\n", pFilename, line, pExpression );
		fprintf( stderr, "Options:\n" );
		fprintf( stderr, "  [b] Break/Continue\n" );
		fprintf( stderr, "  [o] Ignore this once\n" );
		fprintf( stderr, "  [i] Ignore this always\n" );
		fprintf( stderr, "  [a] Ignore all\n" );
		fprintf( stderr, "  [f] Ignore file\n" );
		fprintf( stderr, "> " );

		const int ch{ getchar() };
		switch ( tolower( ch ) )
		{
		case 'b':
			should_break = true;
			break;
		case 'o':
			// Ignore this Assert once
			break;
		case 'i':
			IgnoreAssertsNearby( assert_disables, init_info, 0 );
			break;
		case 'a':
			g_bAssertsEnabled.store( false, std::memory_order_release );
			break;
		case 'f':
			IgnoreAssertsInCurrentFile( assert_disables, init_info );
			break;
		}

		return should_break;
	};

	// dimhotepus: No GUI in console mode, assert to console.
	if ( IsConsoleMode() )
	{
		return consoleAssertFallback();
	}

#if defined( _WIN32 )
	static unsigned last_ignore_num_times{ 1 };

	if ( !ThreadInMainThread() )
	{
		const auto buttonPressed = Plat_MessageBox(
			"Source - Assertion Failed",
			"Assert Failed",
			pExpression,
			Plat_MessageBoxButton::Cancel | Plat_MessageBoxButton::Retry,
			Plat_MessageBoxIcon::Information );

		if ( Plat_MessageBoxButton::Retry == buttonPressed )
		{
			should_break = true;
		}
		else if ( Plat_MessageBoxButton::Cancel == buttonPressed )
		{
			IgnoreAssertsNearby( assert_disables, init_info, 0 );
		}
	}
	else
	{
		HMODULE          hModule{ GetModuleHandle( V_STRINGIFY( DLLNAME ) DLL_EXT_STRING ) };
		HWND             hParentWindow{ FindLikelyParentWindow() };
		AssertDialogData dialogData = { last_ignore_num_times, init_info, assert_disables, should_break, g_bAssertsEnabled };

		DialogBoxParam(
			hModule,
			MAKEINTRESOURCE( IDD_ASSERT_DIALOG ),
			hParentWindow,
			AssertDialogProc,
			reinterpret_cast< LPARAM >( &dialogData ) );
	}

	return should_break;
#elif defined( POSIX )
#ifdef OSX
	static void *pSdlModule = dlopen( "libSDL3.dylib", RTLD_LAZY );
#else
	static void *pSdlModule = dlopen( "libSDL3.so.0", RTLD_LAZY );
#endif

	static auto *pfnSDLShowMessageBox = pSdlModule
		? (SDL_ShowMessageBoxFn *)dlsym( pSdlModule, "SDL_ShowMessageBox" )
		: nullptr;

	if( pfnSDLShowMessageBox )
	{
		const char *DefaultAction = Plat_IsInDebugSession() ? "Break" : "Corefile";
		SDL_MessageBoxButtonData buttondata[] =
		{
			{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,	IDC_BREAK,			DefaultAction			},
			{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,	IDC_IGNORE_THIS,	"Ignore"				},
			{ 0,										IDC_IGNORE_FILE,	"Ignore This File"		},
			{ 0,										IDC_IGNORE_ALWAYS,	"Always Ignore"			},
			{ 0,										IDC_IGNORE_ALL,		"Ignore All Asserts"	},
		};

		char text[4096];
		// Truncation is ok, just for assert.
		_snprintf( text, std::size( text ), "File: %s\nLine: %i\nExpr: %s\n", pFilename, line, pExpression );
		text[ std::size( text ) - 1 ] = '\0';

		SDL_MessageBoxData messageboxdata = { 0 };
		messageboxdata.window = GetAssertDialogParent();
		messageboxdata.title = "Assertion Failed";
		messageboxdata.message = text;
		messageboxdata.numbuttons = ssize( buttondata );
		messageboxdata.buttons = buttondata;

		int buttonid;
		int Ret = ( *pfnSDLShowMessageBox )( &messageboxdata, &buttonid );
		if( Ret == -1 )
		{
			buttonid = IDC_BREAK;
		}

		switch( buttonid )
		{
		default:
		case IDC_BREAK:
			// Break on this Assert
			should_break = true;
			break;
		case IDC_IGNORE_THIS:
			// Ignore this Assert once
			break;
		case IDC_IGNORE_FILE:
			IgnoreAssertsInCurrentFile( assert_disables, init_info );
			break;
		case IDC_IGNORE_ALWAYS:
			// Ignore this Assert from now on
			IgnoreAssertsNearby( assert_disables, init_info, 0 );
			break;
		case IDC_IGNORE_ALL:
			// Ignore all Asserts from now on
			g_bAssertsEnabled.store(false, std::memory_order_release);
			break;
		}

		return should_break;
	}

	// No SDL3, use console.
	return consoleAssertFallback();
#else
	// Not POSIX, use console.
	return consoleAssertFallback();
#endif
}