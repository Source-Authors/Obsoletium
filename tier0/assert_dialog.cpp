// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"

#include "tier0/valve_off.h"

#if defined( _WIN32 )
#include "winlite.h"
#include "windows/dpi_wnd_behavior.h"

// dimhotepus: Launcher icon id.
#include "../launcher_main/resource.h"

// dimhotepus: Remove launcher definition clashing with tier0 one.
#undef SRC_PRODUCT_FILE_DESCRIPTION_STRING
#elif defined( POSIX )
#include <cstdlib>
#endif
#include "resource.h"
#include "tier0/valve_on.h"
#include "tier0/threadtools.h"

#if defined( POSIX )
#include <dlfcn.h>
#endif

#if defined( LINUX ) || defined( USE_SDL )

// We lazily load the SDL shared object, and only reference functions if it's
// available, so this can be included on the dedicated server too.
#include "include/SDL3/SDL.h"

typedef int ( SDLCALL FUNC_SDL_ShowMessageBox )( const SDL_MessageBoxData *messageboxdata, int *buttonid );
#endif

struct CDialogInitInfo
{
	const tchar *m_pFilename;
	int m_iLine;
	const tchar *m_pExpression;
};


struct CAssertDisable
{
	tchar m_Filename[512];
	
	// If these are not -1, then this CAssertDisable only disables asserts on lines between
	// these values (inclusive).
	int m_LineMin;		
	int m_LineMax;
	
	// Decremented each time we hit this assert and ignore it, until it's 0. 
	// Then the CAssertDisable is removed.
	// If this is -1, then we always ignore this assert.
	int m_nIgnoreTimes;	

	CAssertDisable *m_pNext;
};

#ifdef _WIN32
static HINSTANCE g_hTier0Instance = nullptr;
#endif

static bool g_bAssertsEnabled = true;

static CAssertDisable *g_pAssertDisables = NULL;

#if ( defined( _WIN32 ) && !defined( _X360 ) )
static int g_iLastLineRange = 5;
static int g_nLastIgnoreNumTimes = 1;
#endif

// Set to true if they want to break in the debugger.
static bool g_bBreak = false;

static CDialogInitInfo g_Info;


// -------------------------------------------------------------------------------- //
// Internal functions.
// -------------------------------------------------------------------------------- //

#if defined(_WIN32) && !defined(STATIC_TIER0)
extern "C" BOOL APIENTRY MemDbgDllMain( HMODULE hDll, DWORD dwReason, void* );

extern void InitTime();

BOOL WINAPI DllMain( HINSTANCE module, DWORD fdwReason, LPVOID lpvReserved )
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		g_hTier0Instance = module;

		InitTime();
		
		// dimhotepus: Do not notify on thread creation for performance.
		DisableThreadLibraryCalls(module);
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
		// dimhotepus: Cleanup instance.
		g_hTier0Instance = nullptr;
	}

#ifdef DEBUG
	MemDbgDllMain( module, fdwReason, lpvReserved );
#endif

	return TRUE;
}
#endif

static bool IsDebugBreakEnabled()
{
	static bool bResult = ( _tcsstr( Plat_GetCommandLine(), _T("-debugbreak") )    != NULL ) || \
	                      ( _tcsstr( Plat_GetCommandLine(), _T("-raiseonassert") ) != NULL ) || \
	                      getenv( "RAISE_ON_ASSERT" );
	return bResult;
}

static bool AreAssertsDisabled()
{
	static bool bResult = ( _tcsstr( Plat_GetCommandLine(), _T("-noassert") ) != NULL );
	return bResult;
}

static bool AreAssertsEnabledInFileLine( const tchar *pFilename, int iLine )
{
	CAssertDisable **pPrev = &g_pAssertDisables;
	CAssertDisable *pNext = nullptr;
	for ( CAssertDisable *pCur=g_pAssertDisables; pCur; pCur=pNext )
	{
		pNext = pCur->m_pNext;

		if ( _tcsicmp( pFilename, pCur->m_Filename ) == 0 )
		{
			// Are asserts disabled in the whole file?
			bool bAssertsEnabled = true;
			if ( pCur->m_LineMin == -1 && pCur->m_LineMax == -1 )
				bAssertsEnabled = false;
			
			// Are asserts disabled on the specified line?
			if ( iLine >= pCur->m_LineMin && iLine <= pCur->m_LineMax )
				bAssertsEnabled = false;

			if ( !bAssertsEnabled )
			{
				// If this assert is only disabled for the next N times, then countdown..
				if ( pCur->m_nIgnoreTimes > 0 )
				{
					--pCur->m_nIgnoreTimes;
					if ( pCur->m_nIgnoreTimes == 0 )
					{
						// Remove this one from the list.
						*pPrev = pNext;
						delete pCur;
						continue;
					}
				}
				
				return false;
			}
		}

		pPrev = &pCur->m_pNext;
	}

	return true;
}


static CAssertDisable* CreateNewAssertDisable( const tchar *pFilename )
{
	CAssertDisable *pDisable = new CAssertDisable;
	pDisable->m_pNext = g_pAssertDisables;
	g_pAssertDisables = pDisable;

	pDisable->m_LineMin = pDisable->m_LineMax = -1;
	pDisable->m_nIgnoreTimes = -1;
	
	// dimhotepus: Use passed file name to create assert disable.
	_tcsncpy( pDisable->m_Filename, pFilename, sizeof( pDisable->m_Filename ) - 1 );
	pDisable->m_Filename[ sizeof( pDisable->m_Filename ) - 1 ] = 0;
	
	return pDisable;
}


static void IgnoreAssertsInCurrentFile()
{
	CreateNewAssertDisable( g_Info.m_pFilename );
}


static CAssertDisable* IgnoreAssertsNearby( int nRange )
{
	CAssertDisable *pDisable = CreateNewAssertDisable( g_Info.m_pFilename );
	pDisable->m_LineMin = g_Info.m_iLine - nRange;
	pDisable->m_LineMax = g_Info.m_iLine - nRange;
	return pDisable;
}

#if defined( _WIN32 )
se::windows::ui::CDpiWindowBehavior g_dpi_window_behavior{false};

static INT_PTR CALLBACK AssertDialogProc(
  HWND hDlg,                      // handle to dialog box
  UINT uMsg,                      // message
  WPARAM wParam,                  // first message parameter
  LPARAM lParam                   // second message parameter
)
{
	switch( uMsg )
	{
		case WM_INITDIALOG:
		{
#ifdef TCHAR_IS_WCHAR
			SetDlgItemTextW( hDlg, IDC_ASSERT_MSG_CTRL, g_Info.m_pExpression );
			SetDlgItemTextW( hDlg, IDC_FILENAME_CONTROL, g_Info.m_pFilename );
#else
			SetDlgItemText( hDlg, IDC_ASSERT_MSG_CTRL, g_Info.m_pExpression );
			SetDlgItemText( hDlg, IDC_FILENAME_CONTROL, g_Info.m_pFilename );
#endif
			SetDlgItemInt( hDlg, IDC_LINE_CONTROL, g_Info.m_iLine, false );
			SetDlgItemInt( hDlg, IDC_IGNORE_NUMLINES, g_iLastLineRange, false );
			SetDlgItemInt( hDlg, IDC_IGNORE_NUMTIMES, g_nLastIgnoreNumTimes, false );

			// dimhotepus: Honor per monitor V2 DPI.
			// It is DialogBoxParam (uses CreateWindowEx), so need scaling.
			g_dpi_window_behavior.OnCreateWindow(hDlg);

			// dimhotepus: Add launcher icon for Assert dialog.
			HANDLE hExeIcon = LoadImageW( GetModuleHandleW( nullptr ), MAKEINTRESOURCEW( SRC_IDI_APP_MAIN ), IMAGE_ICON, 0, 0, LR_SHARED );
			SendMessage( hDlg, WM_SETICON, ICON_BIG, (LPARAM)hExeIcon );
		
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
			switch( LOWORD( wParam ) )
			{
				case IDC_IGNORE_FILE:
				{
					IgnoreAssertsInCurrentFile();
					EndDialog( hDlg, 0 );
					return TRUE;
				}

				// Ignore this assert N times.
				case IDC_IGNORE_THIS:
				{
					BOOL bTranslated = false;
					UINT value = GetDlgItemInt( hDlg, IDC_IGNORE_NUMTIMES, &bTranslated, false );
					if ( bTranslated && value > 1 )
					{
						CAssertDisable *pDisable = IgnoreAssertsNearby( 0 );
						pDisable->m_nIgnoreTimes = value - 1;
						g_nLastIgnoreNumTimes = value;
					}

					EndDialog( hDlg, 0 );
					return TRUE;
				}

				// Always ignore this assert.
				case IDC_IGNORE_ALWAYS:
				{
					IgnoreAssertsNearby( 0 );
					EndDialog( hDlg, 0 );
					return TRUE;
				}
				
				case IDC_IGNORE_NEARBY:
				{
					BOOL bTranslated = false;
					UINT value = GetDlgItemInt( hDlg, IDC_IGNORE_NUMLINES, &bTranslated, false );
					if ( !bTranslated || value < 1 )
						return TRUE;

					IgnoreAssertsNearby( value );
					EndDialog( hDlg, 0 );
					return TRUE;
				}

				case IDC_IGNORE_ALL:
				{
					g_bAssertsEnabled = false;
					EndDialog( hDlg, 0 );
					return TRUE;
				}

				case IDC_BREAK:
				{
					g_bBreak = true;
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
			return !g_dpi_window_behavior.OnWindowDpiChanged(wParam, lParam)
				? TRUE : FALSE;
		}

		case WM_DESTROY:
		{
			g_dpi_window_behavior.OnDestroyWindow();
			return TRUE;
		}
	}

	return FALSE;
}


static HWND g_hBestParentWindow;


static BOOL CALLBACK ParentWindowEnumProc(
  HWND hWnd,      // handle to parent window
  LPARAM lParam   // application-defined value
)
{
	if ( IsWindowVisible( hWnd ) )
	{
		DWORD procID;
		GetWindowThreadProcessId( hWnd, &procID );
		if ( procID == static_cast<DWORD>(lParam) )
		{
			g_hBestParentWindow = hWnd;
			return FALSE; // don't iterate any more.
		}
	}
	return TRUE;
}


static HWND FindLikelyParentWindow()
{
	// Enumerate top-level windows and take the first visible one with our processID.
	g_hBestParentWindow = NULL;
	EnumWindows( ParentWindowEnumProc, GetCurrentProcessId() );
	return g_hBestParentWindow;
}
#endif // ( defined( _WIN32 ) && !defined( _X360 ) )

// -------------------------------------------------------------------------------- //
// Interface functions.
// -------------------------------------------------------------------------------- //

// provides access to the global that turns asserts on and off
DBG_INTERFACE bool AreAllAssertsDisabled()
{
	return !g_bAssertsEnabled;
}

DBG_INTERFACE void SetAllAssertsDisabled( bool bAssertsDisabled )
{
	g_bAssertsEnabled = !bAssertsDisabled;
}

#if defined( LINUX ) || defined( USE_SDL )
SDL_Window *g_SDLWindow = NULL;

DBG_INTERFACE void SetAssertDialogParent( struct SDL_Window *window )
{
	g_SDLWindow = window;
}

DBG_INTERFACE struct SDL_Window * GetAssertDialogParent()
{
	return g_SDLWindow;
}
#endif

DBG_INTERFACE bool ShouldUseNewAssertDialog()
{
	static bool bMPIWorker = ( _tcsstr( Plat_GetCommandLine(), _T("-mpi_worker") ) != NULL );
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
		char **strings = backtrace_symbols(buffer, nptrs);
		if ( strings )
		{
			for ( int i = 0; i < nptrs; i++)
			{
				const char *module = strrchr( strings[ i ], '/' );
				module = module ? ( module + 1 ) : strings[ i ];

				fprintf( stderr, "  %s\n", module );
			}

			free( strings );
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
	if ( !g_bAssertsEnabled )
		return false;

	// Has this specific Assert been disabled?
	if ( !AreAssertsEnabledInFileLine( pFilename, line ) )
		return false;

	// Assert not suppressed. Spew it, and optionally a backtrace.
#if defined( POSIX )
	if( isatty( STDERR_FILENO ) )
	{
		#define COLOR_YELLOW 	"\033[1;33m"
		#define COLOR_GREEN 	"\033[1;32m"
		#define COLOR_RED 		"\033[1;31m"
		#define COLOR_END		"\033[0m"
		fprintf(stderr, COLOR_YELLOW "ASSERT:" COLOR_END " " COLOR_RED "%s" COLOR_GREEN ":%i:" COLOR_END " " COLOR_RED "%s" COLOR_END "\n",
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

	// Now create the dialog. Just return true for old-style debug break upon failure.
	g_Info.m_pFilename = pFilename;
	g_Info.m_iLine = line;
	g_Info.m_pExpression = pExpression;

	g_bBreak = false;

#if defined( _WIN32 )

	if ( !ThreadInMainThread() )
	{
		int result = MessageBox( NULL, pExpression, "Source - Assertion Failed", MB_SYSTEMMODAL | MB_CANCELTRYCONTINUE | MB_ICONQUESTION );

		if ( result == IDCANCEL )
		{
			IgnoreAssertsNearby( 0 );
		}
		else if ( result == IDCONTINUE )
		{
			g_bBreak = true;
		}
	}
	else
	{
		HWND hParentWindow = FindLikelyParentWindow();

		DialogBox( g_hTier0Instance, MAKEINTRESOURCE( IDD_ASSERT_DIALOG ), hParentWindow, AssertDialogProc );
	}

#elif defined( POSIX )
	static FUNC_SDL_ShowMessageBox *pfnSDLShowMessageBox = NULL;
	if( !pfnSDLShowMessageBox )
	{
#ifdef OSX
		void *ret = dlopen( "libSDL2-2.0.0.dylib", RTLD_LAZY );
#else
		void *ret = dlopen( "libSDL2-2.0.so.0", RTLD_LAZY );
#endif
		if ( ret )
			{ pfnSDLShowMessageBox = ( FUNC_SDL_ShowMessageBox * )dlsym( ret, "SDL_ShowMessageBox" ); }
	}

	if( pfnSDLShowMessageBox )
	{
		int buttonid;
		char text[ 4096 ];
		SDL_MessageBoxData messageboxdata = { 0 };
		const char *DefaultAction = Plat_IsInDebugSession() ? "Break" : "Corefile";
		SDL_MessageBoxButtonData buttondata[] =
		{
			{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,	IDC_BREAK,			DefaultAction			},
			{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,	IDC_IGNORE_THIS,	"Ignore"				},
			{ 0,										IDC_IGNORE_FILE,	"Ignore This File"		},
			{ 0,										IDC_IGNORE_ALWAYS,	"Always Ignore"			},
			{ 0,										IDC_IGNORE_ALL,		"Ignore All Asserts"	},
		};

		_snprintf( text, std::size( text ), "File: %s\nLine: %i\nExpr: %s\n", pFilename, line, pExpression );
		text[ std::size( text ) - 1 ] = '\0';

		messageboxdata.window = g_SDLWindow;
		messageboxdata.title = "Assertion Failed";
		messageboxdata.message = text;
		messageboxdata.numbuttons = ssize( buttondata );
		messageboxdata.buttons = buttondata;

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
			g_bBreak = true;
			break;
		case IDC_IGNORE_THIS:
			// Ignore this Assert once
			break;
		case IDC_IGNORE_FILE:
			IgnoreAssertsInCurrentFile();
			break;
		case IDC_IGNORE_ALWAYS:
			// Ignore this Assert from now on
			IgnoreAssertsNearby( 0 );
			break;
		case IDC_IGNORE_ALL:
			// Ignore all Asserts from now on
			g_bAssertsEnabled = false;
			break;
		}
	}
	else
	{
		// Couldn't SDL it up
		g_bBreak = true;
	}

#else
	// No dialog mode on this platform
	g_bBreak = true;
#endif

	return g_bBreak;
}

