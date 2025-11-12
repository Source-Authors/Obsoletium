//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
//=============================================================================
#ifdef _WIN32

#include "winlite.h"

#include "appframework/AppFramework.h"
#include "appframework/VguiMatSysApp.h"
#include "filesystem.h"
#include "materialsystem/imaterialsystem.h"
#include "vgui/IVGui.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"
#include "vgui/IScheme.h"
#include "vgui/ILocalize.h"
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "materialsystem/materialsystem_config.h"
#include "filesystem_init.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "inputsystem/iinputsystem.h"
#include "tier3/tier3.h"


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CVguiMatSysApp::Create()
{
	AppSystemInfo_t appSystems[] = 
	{
		{ "inputsystem" DLL_EXT_STRING,			INPUTSYSTEM_INTERFACE_VERSION },
		{ "materialsystem" DLL_EXT_STRING,		MATERIAL_SYSTEM_INTERFACE_VERSION },

		// NOTE: This has to occur before vgui2 so it replaces vgui2's surface implementation
		{ "vguimatsurface" DLL_EXT_STRING,		VGUI_SURFACE_INTERFACE_VERSION },
		{ "vgui2" DLL_EXT_STRING,				VGUI_IVGUI_INTERFACE_VERSION },

		// Required to terminate the list
		{ "", "" }
	};

	if ( !AddSystems( appSystems ) ) 
	{
		Warning( "CVguiMatSysApp::Create: Unable to load app systems!\n" );
		return false;
	}

	auto *pMaterialSystem = FindSystem<IMaterialSystem>( MATERIAL_SYSTEM_INTERFACE_VERSION );
	if ( !pMaterialSystem )
	{
		Warning( "CVguiMatSysApp::Create: Unable to connect to '%s'!\n", MATERIAL_SYSTEM_INTERFACE_VERSION );
		return false;
	}

	char const* shaderApiDllName = "shaderapidx9" DLL_EXT_STRING;
	// dimhotepus: Allow to override shader API DLL.
	const char *pArg = nullptr;
	if ( CommandLine()->CheckParm( "-shaderapi", &pArg ))
	{
		shaderApiDllName = pArg;
	}

	pMaterialSystem->SetShaderAPI( shaderApiDllName );
	// dimhotepus: Ensure shader API is set.
	return pMaterialSystem->HasShaderAPI();
}

void CVguiMatSysApp::Destroy()
{
}



//-----------------------------------------------------------------------------
// Window management
//-----------------------------------------------------------------------------
void* CVguiMatSysApp::CreateAppWindow( char const *pTitle, bool bWindowed, int w, int h )
{
	WNDCLASSEX  wc   = {sizeof( wc ), 0, nullptr, 0, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	wc.style         = CS_OWNDC | CS_DBLCLKS;
	wc.lpfnWndProc   = DefWindowProc;
	wc.hInstance     = static_cast<HINSTANCE>(GetAppInstance());
	wc.lpszClassName = "Valve001";
	wc.hIconSm       = wc.hIcon;

	if (!RegisterClassExA( &wc ))
	{
		Warning("Unable to register window '%s' class: %s.\n",
			pTitle, std::system_category().message(::GetLastError()).c_str());
		return nullptr;
	}

	// Note, it's hidden
	DWORD style = WS_POPUP | WS_CLIPSIBLINGS;
	
	if ( bWindowed )
	{
		// Give it a frame
		style |= WS_OVERLAPPEDWINDOW;
		style &= ~WS_THICKFRAME;
	}

	// Never a max box
	style &= ~WS_MAXIMIZEBOX;

	RECT windowRect{0, 0, w, h};

	// Compute rect needed for that size client area based on window style
	::AdjustWindowRectExForDpi( &windowRect, style, FALSE, 0, ::GetDpiForSystem() );

	// Create the window
	HWND hWnd{CreateWindowExA( 0, wc.lpszClassName, pTitle, style, 0, 0,
		windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
		nullptr, nullptr, static_cast<HINSTANCE>(GetAppInstance()), nullptr )};
	if (!hWnd)
	{
		Warning("Unable to create window '%s': %s.\n", pTitle,
				std::system_category().message(::GetLastError()).c_str());
		return nullptr;
	}

	// dimhotepus: Apply Dark mode if any and Mica styles to window title bar.
	Plat_ApplySystemTitleBarTheme( hWnd, SystemBackdropType::MainWindow );

	const unsigned window_dpi{::GetDpiForWindow(hWnd)};
	// Compute rect needed for DPI + that size client area based on window style
	::AdjustWindowRectExForDpi(&windowRect, style, FALSE, 0, window_dpi);

	const int CenterX{std::max(0, (GetSystemMetricsForDpi(SM_CXSCREEN, window_dpi) - w) / 2)};
	const int CenterY{std::max(0, (GetSystemMetricsForDpi(SM_CYSCREEN, window_dpi) - h) / 2)};

	// In VCR modes, keep it in the upper left so mouse coordinates are always
	// relative to the window.
	::SetWindowPos( hWnd, nullptr, CenterX, CenterY,
		windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
		SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);

	return hWnd;
}


//-----------------------------------------------------------------------------
// Pump messages
//-----------------------------------------------------------------------------
void CVguiMatSysApp::AppPumpMessages()
{
	g_pInputSystem->PollInputState();
}


//-----------------------------------------------------------------------------
// Sets up the game path
//-----------------------------------------------------------------------------
bool CVguiMatSysApp::SetupSearchPaths( const char *pStartingDir, bool bOnlyUseStartingDir, bool bIsTool )
{
	if ( !BaseClass::SetupSearchPaths( pStartingDir, bOnlyUseStartingDir, bIsTool ) )
		return false;

	g_pFullFileSystem->AddSearchPath( GetGameInfoPath(), "SKIN", PATH_ADD_TO_HEAD );
	return true;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CVguiMatSysApp::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	if ( !g_pFullFileSystem || !g_pMaterialSystem || !g_pInputSystem || !g_pMatSystemSurface )
	{
		Warning( "CVguiMatSysApp::PreInit: Unable to connect to necessary interfaces!\n" );
		return false;
	}

	// Add paths...
	if ( !SetupSearchPaths( nullptr, false, true ) )
		return false;

	const char *pArg;
	int iWidth = 1024;
	int iHeight = 768;
	bool bWindowed = (CommandLine()->CheckParm( "-fullscreen" ) == nullptr);
	if (CommandLine()->CheckParm( "-width", &pArg ))
	{
		iWidth = atoi( pArg );
	}
	if (CommandLine()->CheckParm( "-height", &pArg ))
	{
		iHeight = atoi( pArg );
	}

	m_nWidth = iWidth;
	m_nHeight = iHeight;
	m_HWnd = CreateAppWindow( GetAppName(), bWindowed, iWidth, iHeight );
	if ( !m_HWnd )
		return false;

	g_pInputSystem->AttachToWindow( m_HWnd );
	g_pMatSystemSurface->AttachToWindow( m_HWnd );

	// NOTE: If we specifically wanted to use a particular shader DLL, we set it here...
	//m_pMaterialSystem->SetShaderAPI( "shaderapidx8" );

	// Get the adapter from the command line....
	const char *pAdapterString;
	unsigned adapter = 0;
	if (CommandLine()->CheckParm( "-adapter", &pAdapterString ))
	{
		adapter = strtoul( pAdapterString, nullptr, 10 );
	}

	int adapterFlags = 0;
	if ( CommandLine()->CheckParm( "-ref" ) )
	{
		adapterFlags |= MATERIAL_INIT_REFERENCE_RASTERIZER;
	}
	if ( AppUsesReadPixels() )
	{
		adapterFlags |= MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE;
	}

	g_pMaterialSystem->SetAdapter( adapter, adapterFlags );

	return true; 
}

void CVguiMatSysApp::PostShutdown()
{
	if ( g_pMatSystemSurface && g_pInputSystem )
	{
		g_pMatSystemSurface->AttachToWindow( nullptr );
		g_pInputSystem->DetachFromWindow( );
	}

	BaseClass::PostShutdown();
}


//-----------------------------------------------------------------------------
// Gets the window size
//-----------------------------------------------------------------------------
int CVguiMatSysApp::GetWindowWidth() const
{
	return m_nWidth;
}

int CVguiMatSysApp::GetWindowHeight() const
{
	return m_nHeight;
}


//-----------------------------------------------------------------------------
// Returns the window
//-----------------------------------------------------------------------------
void* CVguiMatSysApp::GetAppWindow()
{
	return m_HWnd;
}

	
//-----------------------------------------------------------------------------
// Sets the video mode
//-----------------------------------------------------------------------------
bool CVguiMatSysApp::SetVideoMode( )
{
	MaterialSystem_Config_t config;
	config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, !CommandLine()->CheckParm( "-fullscreen" ) );
	config.SetFlag( MATSYS_VIDCFG_FLAGS_NO_WINDOW_BORDER, !!CommandLine()->CheckParm( "-noborder" ) );

	if ( CommandLine()->CheckParm( "-resizing" ) )
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_RESIZING, true );
	}

	if ( CommandLine()->CheckParm( "-mat_vsync" ) )
	{
		config.SetFlag( MATSYS_VIDCFG_FLAGS_NO_WAIT_FOR_VSYNC, false );
	}

	config.m_nAASamples = CommandLine()->ParmValue( "-mat_antialias", 1 );
	config.m_nAAQuality = CommandLine()->ParmValue( "-mat_aaquality", 0 );

	config.m_VideoMode.m_Width = config.m_VideoMode.m_Height = 0;
	config.m_VideoMode.m_Format = IMAGE_FORMAT_BGRX8888;
	config.m_VideoMode.m_RefreshRate = 0;
	config.SetFlag(	MATSYS_VIDCFG_FLAGS_STENCIL, true );

	bool modeSet = g_pMaterialSystem->SetMode( m_HWnd, config );
	if (!modeSet)
	{
		Error( "Unable to set mode %d x %d, %d Hz, format BGRX88888.\n",
			config.m_VideoMode.m_Width,
			config.m_VideoMode.m_Height,
			config.m_VideoMode.m_RefreshRate );
	}

	g_pMaterialSystem->OverrideConfig( config, false );
	return true;
}

#endif // _WIN32

