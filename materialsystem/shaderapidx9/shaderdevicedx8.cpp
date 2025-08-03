//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#define DISABLE_PROTECTED_THINGS
#include "locald3dtypes.h"
#include <comdef.h>

#include "shaderdevicedx8.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapidx8_global.h"
#include "filesystem.h"
#include "tier0/icommandline.h"
#include "tier2/tier2.h"
#include "shadershadowdx8.h"
#include "colorformatdx8.h"
#include "materialsystem/IShader.h"
#include "shaderapidx8.h"
#include "imeshdx8.h"
#include "materialsystem/materialsystem_config.h"
#include "vertexshaderdx8.h"
#include "recording.h"
#include "winutils.h"
#include "tier0/vprof_telemetry.h"

#include "windows/com_error_category.h"

#if defined ( DX_TO_GL_ABSTRACTION )
// Placed here so inlines placed in dxabstract.h can access gGL
COpenGLEntryPoints *gGL = NULL;
#endif

#define D3D_BATCH_PERF_ANALYSIS 0

#if D3D_BATCH_PERF_ANALYSIS
#if defined( DX_TO_GL_ABSTRACTION )
#error Cannot enable D3D_BATCH_PERF_ANALYSIS when using DX_TO_GL_ABSTRACTION, use GL_BATCH_PERF_ANALYSIS instead.
#endif
// Define this if you want all d3d9 interfaces hooked and run through the dx9hook.h shim interfaces. For profiling, etc.
#define DO_DX9_HOOK
#endif

#ifdef DO_DX9_HOOK

#if D3D_BATCH_PERF_ANALYSIS
ConVar d3d_batch_vis( "d3d_batch_vis", "0" );
ConVar d3d_batch_vis_abs_scale( "d3d_batch_vis_abs_scale", ".050" );
ConVar d3d_present_vis_abs_scale( "d3d_batch_vis_abs_scale", ".050" );
ConVar d3d_batch_vis_y_scale( "d3d_batch_vis_y_scale", "0.0" );
uint64 g_nTotalD3DCalls, g_nTotalD3DCycles;
static double s_rdtsc_to_ms;
#endif

#include "dx9hook.h"
#endif

#ifndef _X360
#include "wmi.h"
#endif

//#define DX8_COMPATABILITY_MODE

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
static CShaderDeviceMgrDx8 g_ShaderDeviceMgrDx8;
CShaderDeviceMgrDx8* g_pShaderDeviceMgrDx8 = &g_ShaderDeviceMgrDx8;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderDeviceMgrDx8, IShaderDeviceMgr, 
	SHADER_DEVICE_MGR_INTERFACE_VERSION, g_ShaderDeviceMgrDx8 )

// hook into mat_forcedynamic from the engine.
static ConVar mat_forcedynamic( "mat_forcedynamic", "0", FCVAR_CHEAT );

// this is hooked into the engines convar
ConVar mat_debugalttab( "mat_debugalttab", "0", FCVAR_CHEAT );


//-----------------------------------------------------------------------------
//
// Device manager
//
//-----------------------------------------------------------------------------

	
//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CShaderDeviceMgrDx8::CShaderDeviceMgrDx8()
{
	m_bObeyDxCommandlineOverride = true;
	m_bAdapterInfoIntialized = false;

#if defined( PIX_INSTRUMENTATION ) && defined ( DX_TO_GL_ABSTRACTION ) && defined( _WIN32 )
	m_hD3D9 = NULL;
	m_pBeginEvent = NULL;
	m_pEndEvent = NULL;
	m_pSetMarker = NULL;
	m_pSetOptions = NULL;
#endif	
}

CShaderDeviceMgrDx8::~CShaderDeviceMgrDx8()
{
}

#ifdef OSX
#include <Carbon/Carbon.h>
#endif
//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrDx8::Connect( CreateInterfaceFn factory )
{
	LOCK_SHADERAPI();

	if ( !BaseClass::Connect( factory ) )
		return false;

#if defined ( DX_TO_GL_ABSTRACTION )
	gGL = ToGLConnectLibraries( factory );
#endif

#if defined(IS_WINDOWS_PC) && defined(SHADERAPIDX9) && !defined(RECORDING) && !defined( DX_TO_GL_ABSTRACTION )
	if (m_pD3D)
	{
		m_pD3D.Release();
	}

	const HRESULT hr{ Direct3DCreate9Ex( D3D_SDK_VERSION, &m_pD3D ) };
#else
	#if defined( DO_DX9_HOOK )
		m_pD3D = Direct3DCreate9Hook(D3D_SDK_VERSION);
	#else
		m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	#endif
	constexpr HRESULT hr = E_FAIL;
#endif

	if ( FAILED( hr ) )
	{
		Warning( "Direct3DCreate9Ex(sdk version = 0x%x) failed w/e %s.\n",
			D3D_SDK_VERSION, 
			se::win::com::com_error_category().message(hr).c_str() );
		return false;
	}

#if defined( PIX_INSTRUMENTATION ) && defined ( DX_TO_GL_ABSTRACTION ) && defined( _WIN32 )
	// This is a little odd, but AMD PerfStudio hooks D3D9.DLL and intercepts all of the D3DPERF API's (even for OpenGL apps).
	// So dynamically load d3d9.dll and get the address of these exported functions.
	if ( !m_hD3D9 )
	{
		m_hD3D9 = LoadLibraryExA("d3d9.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
	}
	if ( m_hD3D9 )
	{
		Plat_DebugString( "PIX_INSTRUMENTATION: Loaded d3d9.dll\n" );
		printf( "PIX_INSTRUMENTATION: Loaded d3d9.dll\n" );

		m_pBeginEvent = (D3DPERF_BeginEvent_FuncPtr)GetProcAddress( m_hD3D9, "D3DPERF_BeginEvent" );
		m_pEndEvent = (D3DPERF_EndEvent_FuncPtr)GetProcAddress( m_hD3D9, "D3DPERF_EndEvent" );
		m_pSetMarker = (D3DPERF_SetMarker_FuncPtr)GetProcAddress( m_hD3D9, "D3DPERF_SetOptions" );
		m_pSetOptions = (D3DPERF_SetOptions_FuncPtr)GetProcAddress( m_hD3D9, "D3DPERF_SetMarker" );
	}
#endif

	// FIXME: Want this to be here, but we can't because Steam
	// hasn't had it's application ID set up yet.
	// dimhotepus: Init adapters immediately.
	if ( !InitGpuInfo() )
	{
		Warning("No suitable GPU device. Do you have a GPU?\n");
		return false;
	}

	return true;
}

void CShaderDeviceMgrDx8::Disconnect()
{
	LOCK_SHADERAPI();

#if defined( PIX_INSTRUMENTATION ) && defined ( DX_TO_GL_ABSTRACTION ) && defined( _WIN32 )
	if ( m_hD3D9 )
	{
		m_pBeginEvent = NULL;
		m_pEndEvent = NULL;
		m_pSetMarker = NULL;
		m_pSetOptions = NULL;

		FreeLibrary( m_hD3D9 );
		m_hD3D9 = NULL;
	}
#endif

	if ( m_pD3D )
	{
		m_pD3D.Release();
	}

#if defined ( DX_TO_GL_ABSTRACTION )
	ToGLDisconnectLibraries();
#endif

	BaseClass::Disconnect();
}



//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
InitReturnVal_t CShaderDeviceMgrDx8::Init( )
{
	// FIXME: Remove call to InitGpuInfo once Steam startup issues are resolved.
	// Do it in Connect instead.
	// dimhotepus: Init adapters in Connect.
	// InitGpuInfo();

	return INIT_OK;
}


//-----------------------------------------------------------------------------
// Shutdown
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::Shutdown( )
{
	LOCK_SHADERAPI();
	
// FIXME: Make PIX work
	
// BeginPIXEvent( PIX_VALVE_ORANGE, "Shutdown" );
	
	if ( g_pShaderAPI )
	{
		g_pShaderAPI->OnDeviceShutdown();
	}
	
	if ( g_pShaderDevice )
	{
		g_pShaderDevice->ShutdownDevice();
		g_pMaterialSystemHardwareConfig = NULL;
	}

//	EndPIXEvent();

	
}



//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Initialize adapter information
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrDx8::InitGpuInfo()
{
	if ( m_bAdapterInfoIntialized )
		return true;

	m_bAdapterInfoIntialized = true;
	m_Adapters.RemoveAll();
	
	bool has_any_adapter = false;
	Assert(m_pD3D);
	unsigned nCount = m_pD3D->GetAdapterCount( );

	for( unsigned i = 0; i < nCount; ++i )
	{
		AdapterInfo_t &info = m_Adapters[m_Adapters.AddToTail()];
#ifdef _DEBUG
		memset( &info.m_ActualCaps, 0xDD, sizeof(info.m_ActualCaps) );
#endif

		info.m_ActualCaps.m_bDeviceOk = ComputeCapsFromD3D( &info.m_ActualCaps, i );
		if ( !info.m_ActualCaps.m_bDeviceOk )
			continue;

		ReadDXSupportLevels( info.m_ActualCaps );

		// Read dxsupport.cfg which has config overrides for particular cards.
		ReadHardwareCaps( info.m_ActualCaps, info.m_ActualCaps.m_nMaxDXSupportLevel );

		// What's in "-shader" overrides dxsupport.cfg
		const char *override = CommandLine()->ParmValue( "-shader" );
		if ( override )	V_strcpy_safe( info.m_ActualCaps.m_pShaderDLL, override );

		has_any_adapter = true;
	}

	return has_any_adapter;
}

//--------------------------------------------------------------------------------
// Code to detect support for texture border color (widely supported but the caps
// bit is messed up in drivers due to a stupid WHQL test that requires this to work
// with float textures which we don't generally care about wrt this address mode)
//--------------------------------------------------------------------------------
void CShaderDeviceMgrDx8::CheckBorderColorSupport( HardwareCaps_t *pCaps, unsigned nAdapter )
{
#ifdef DX_TO_GL_ABSTRACTION
	pCaps->m_bSupportsBorderColor = true;
#else
	if( IsX360() )
#endif
	{
		pCaps->m_bSupportsBorderColor = true;
	}
	else
	{
		// TODO: Most PC parts do this, but let's not deal with that yet (JasonM)
		pCaps->m_bSupportsBorderColor = false;
	}
}

//--------------------------------------------------------------------------------
// Vendor-dependent code to detect support for various flavors of shadow mapping
//--------------------------------------------------------------------------------
void CShaderDeviceMgrDx8::CheckVendorDependentShadowMappingSupport( HardwareCaps_t *pCaps, unsigned nAdapter )
{
	// Set a default null texture format...may be overridden below by IHV-specific surface type
	pCaps->m_NullTextureFormat = IMAGE_FORMAT_ARGB8888;
	if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, m_AdapterImageFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, D3DFMT_R5G6B5 ) == S_OK )
	{
		pCaps->m_NullTextureFormat = IMAGE_FORMAT_RGB565;
	}

#if defined ( DX_TO_GL_ABSTRACTION )
	// We may want to only do this on the higher-end Mac SKUs, since it's not free...
	pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_NV_DST16; // This format shunts us down the right shader combo path
	pCaps->m_bSupportsShadowDepthTextures = true;
	pCaps->m_bSupportsFetch4 = false;
	return;
#endif

	{
		bool bToolsMode = IsWindows() && ( CommandLine()->CheckParm( "-tools" ) != NULL );
		bool bFound16Bit = false;

		if ( ( pCaps->m_VendorID == VENDORID_NVIDIA ) && ( pCaps->m_SupportsShaderModel_3_0  ) )	// ps_3_0 parts from nVidia
		{
			// First, test for null texture support
			if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, m_AdapterImageFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, NVFMT_NULL ) == S_OK )
			{
				pCaps->m_NullTextureFormat = IMAGE_FORMAT_NV_NULL;
			}

			//
			// NVIDIA has two no-PCF formats (these are not filtering modes, but surface formats
			//   NVFMT_RAWZ is supported by NV4x (not supported here yet...requires a dp3 to reconstruct in shader code, which doesn't seem to work)
			//   NVFMT_INTZ is supported on newer chips as of G8x (just read like ATI non-fetch4 mode)
			//
/*
			if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, m_AdapterImageFormat, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE, NVFMT_INTZ ) == S_OK )
			{
				pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_NV_INTZ;
				pCaps->m_bSupportsFetch4 = false;
				pCaps->m_bSupportsShadowDepthTextures = true;
				return;
			}
*/
			if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, m_AdapterImageFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, D3DFMT_D16 ) == S_OK )
			{
				pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_NV_DST16;
				pCaps->m_bSupportsFetch4 = false;
				pCaps->m_bSupportsShadowDepthTextures = true;
				bFound16Bit = true;

				if ( !bToolsMode )	// Tools will continue on and try for 24 bit...
					return;
			}
			
			if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, m_AdapterImageFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, D3DFMT_D24S8 ) == S_OK )
			{
				pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_NV_DST24;
				pCaps->m_bSupportsFetch4 = false;
				pCaps->m_bSupportsShadowDepthTextures = true;
				return;
			}

			if ( bFound16Bit )		// Found 16 bit but not 24
				return;
		}
		else if ( ( pCaps->m_VendorID == VENDORID_ATI ) && pCaps->m_SupportsPixelShaders_2_b )		// ps_2_b parts from ATI
		{
			// Initially, check for Fetch4 (tied to ATIFMT_D24S8 support)
			pCaps->m_bSupportsFetch4 = false;
			if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, m_AdapterImageFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, ATIFMT_D24S8 ) == S_OK )
			{
				pCaps->m_bSupportsFetch4 = true;
			}

			if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, m_AdapterImageFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, ATIFMT_D16 ) == S_OK )	// Prefer 16-bit
			{
				pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_ATI_DST16;
				pCaps->m_bSupportsShadowDepthTextures = true;
				bFound16Bit = true;

				if ( !bToolsMode )	// Tools will continue on and try for 24 bit...
					return;
			}
			
			if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, m_AdapterImageFormat, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, ATIFMT_D24S8 ) == S_OK )
			{
				pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_ATI_DST24;
				pCaps->m_bSupportsShadowDepthTextures = true;
				return;
			}

			if ( bFound16Bit )		// Found 16 bit but not 24
				return;
		}
	}

	// Other vendor or old hardware
	pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_UNKNOWN;
	pCaps->m_bSupportsShadowDepthTextures = false;
	pCaps->m_bSupportsFetch4 = false;
}


//-----------------------------------------------------------------------------
// Vendor-dependent code to detect Alpha To Coverage Backdoors
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::CheckVendorDependentAlphaToCoverage( HardwareCaps_t *pCaps, unsigned nAdapter )
{
	pCaps->m_bSupportsAlphaToCoverage = false;

	// Bail out on OpenGL
#ifdef DX_TO_GL_ABSTRACTION
	pCaps->m_bSupportsAlphaToCoverage	 = true;
	pCaps->m_AlphaToCoverageEnableValue	 = TRUE;
	pCaps->m_AlphaToCoverageDisableValue = FALSE;
	pCaps->m_AlphaToCoverageState		 = D3DRS_ADAPTIVETESS_Y; // Just match the NVIDIA state hackery
	return;
#endif

	if ( pCaps->m_nDXSupportLevel < 90 )
		return;

	if ( pCaps->m_VendorID == VENDORID_NVIDIA )
	{
		// nVidia has two modes...assume SSAA is superior to MSAA and hence more desirable (though it's probably not)
		//
		// Currently, they only seem to expose any of this on 7800 and up though older parts certainly
		// support at least the MSAA mode since they support it on OpenGL via the arb_multisample extension
		bool bNVIDIA_MSAA = false;
		bool bNVIDIA_SSAA = false;

		if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE,					// Check MSAA version
			m_AdapterImageFormat, 0, D3DRTYPE_SURFACE,
			(D3DFORMAT)MAKEFOURCC('A', 'T', 'O', 'C')) == S_OK )
		{
			bNVIDIA_MSAA = true;
		}

		if ( m_pD3D->CheckDeviceFormat( nAdapter, DX8_DEVTYPE,					// Check SSAA version
			m_AdapterImageFormat, 0, D3DRTYPE_SURFACE,
			(D3DFORMAT)MAKEFOURCC('S', 'S', 'A', 'A')) == S_OK )
		{
			bNVIDIA_SSAA = true;
		}

		// nVidia pitches SSAA but we prefer ATOC
		if ( bNVIDIA_MSAA || bNVIDIA_SSAA )
		{
			if ( bNVIDIA_MSAA )
				pCaps->m_AlphaToCoverageEnableValue = MAKEFOURCC('A', 'T', 'O', 'C');
			else
				pCaps->m_AlphaToCoverageEnableValue	= MAKEFOURCC('S', 'S', 'A', 'A');

			pCaps->m_AlphaToCoverageState = D3DRS_ADAPTIVETESS_Y;
			pCaps->m_AlphaToCoverageDisableValue = (DWORD)D3DFMT_UNKNOWN;
			pCaps->m_bSupportsAlphaToCoverage = true;
			return;
		}
	}
	else if ( pCaps->m_VendorID == VENDORID_ATI )
	{
		// Supported on all ATI parts...just go ahead and set the state when appropriate
		pCaps->m_AlphaToCoverageState		= D3DRS_POINTSIZE;
		pCaps->m_AlphaToCoverageEnableValue	= MAKEFOURCC('A','2','M','1');
		pCaps->m_AlphaToCoverageDisableValue = MAKEFOURCC('A','2','M','0');
		pCaps->m_bSupportsAlphaToCoverage = true;
		return;
	}
}

ConVar mat_hdr_level( "mat_hdr_level", "2", FCVAR_ARCHIVE );
ConVar mat_slopescaledepthbias_shadowmap( "mat_slopescaledepthbias_shadowmap", "16", FCVAR_CHEAT );
#ifdef DX_TO_GL_ABSTRACTION
ConVar mat_depthbias_shadowmap(	"mat_depthbias_shadowmap", "20", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
#else
ConVar mat_depthbias_shadowmap(	"mat_depthbias_shadowmap", "0.0005", FCVAR_CHEAT  );
#endif

// For testing Fast Clip
ConVar mat_fastclip( "mat_fastclip", "0", FCVAR_CHEAT  );

//-----------------------------------------------------------------------------
// Determine capabilities
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrDx8::ComputeCapsFromD3D( HardwareCaps_t *pCaps, unsigned nAdapter )
{
	D3DCAPS caps;
	// NOTE: When getting the caps, we want to be limited by the hardware
	// even if we're running with software T&L...
	HRESULT hr = m_pD3D->GetDeviceCaps( nAdapter, DX8_DEVTYPE, &caps );
	if ( FAILED( hr ) )
	{
		Assert( false );
		Warning( "IDirect3D9Ex::GetDeviceCaps(adapter = %u, device type = 0x%x) failed w/e %s.\n",
			nAdapter,
			DX8_DEVTYPE,
			se::win::com::com_error_category().message(hr).c_str() );
		return false;
	}
	
	D3DADAPTER_IDENTIFIER9 ident;
	hr = m_pD3D->GetAdapterIdentifier( nAdapter, D3DENUM_WHQL_LEVEL, &ident );
	if ( FAILED( hr ) )
	{
		Assert( false );
		Warning( "IDirect3D9Ex::GetAdapterIdentifier(adapter = %u, flags = D3DENUM_WHQL_LEVEL) failed w/e %s.\n",
			nAdapter, 
			se::win::com::com_error_category().message(hr).c_str() );
		return false;
	}

	if ( IsOpenGL() )
	{
		if ( !ident.DeviceId && !ident.VendorId )
		{
			ident.DeviceId = 1; // fake default device/vendor ID for OpenGL
			ident.VendorId = 1;
		}
	}

	// Intended for debugging only
	if ( CommandLine()->CheckParm( "-force_device_id" ) )
	{
		const char *pDevID = CommandLine()->ParmValue( "-force_device_id", "" );
		if ( pDevID )
		{
			int nDevID = V_atoi( pDevID );	// use V_atoi for hex support
			if ( nDevID > 0 )
			{
				ident.DeviceId = nDevID;
			}
		}
	}

	// Intended for debugging only
	if ( CommandLine()->CheckParm( "-force_vendor_id" ) )
	{
		const char *pVendorID = CommandLine()->ParmValue( "-force_vendor_id", "" );
		if ( pVendorID )
		{
			int nVendorID = V_atoi( pVendorID );	// use V_atoi for hex support
			if ( nVendorID > 0 )
			{
				ident.VendorId = nVendorID;
			}
		}
	}

	V_strcpy_safe( pCaps->m_pDriverName, ident.Description );
	pCaps->m_VendorID = static_cast<VendorId>(ident.VendorId);
	pCaps->m_DeviceID = ident.DeviceId;
	pCaps->m_SubSysID = ident.SubSysId;
	pCaps->m_Revision = ident.Revision;

	pCaps->m_nDriverVersionHigh =  ident.DriverVersion.HighPart;
	pCaps->m_nDriverVersionLow = ident.DriverVersion.LowPart;

	pCaps->m_pShaderDLL[0] = 0;
	pCaps->m_nMaxViewports = 1;

	pCaps->m_PreferDynamicTextures = ( caps.Caps2 & D3DCAPS2_DYNAMICTEXTURES ) ? 1 : 0;

	pCaps->m_HasProjectedBumpEnv = ( caps.TextureCaps & D3DPTEXTURECAPS_NOPROJECTEDBUMPENV ) == 0;

	pCaps->m_HasSetDeviceGammaRamp = (caps.Caps2 & D3DCAPS2_CANCALIBRATEGAMMA) != 0;
	pCaps->m_SupportsVertexShaders = ((caps.VertexShaderVersion >> 8) & 0xFF) >= 1;
	pCaps->m_SupportsPixelShaders = ((caps.PixelShaderVersion >> 8) & 0xFF) >= 1;

	pCaps->m_bScissorSupported = ( caps.RasterCaps & D3DPRASTERCAPS_SCISSORTEST ) !=  0;

#if defined( DX8_COMPATABILITY_MODE )
	pCaps->m_SupportsPixelShaders_1_4  = false;
	pCaps->m_SupportsPixelShaders_2_0  = false;
	pCaps->m_SupportsPixelShaders_2_b  = false;
	pCaps->m_SupportsVertexShaders_2_0 = false;
	pCaps->m_SupportsShaderModel_3_0  = false;
	pCaps->m_SupportsMipmappedCubemaps = false;
#else
	pCaps->m_SupportsPixelShaders_1_4 = ( caps.PixelShaderVersion & 0xffff ) >= 0x0104;
	pCaps->m_SupportsPixelShaders_2_0 = ( caps.PixelShaderVersion & 0xffff ) >= 0x0200;
	pCaps->m_SupportsPixelShaders_2_b = ( ( caps.PixelShaderVersion & 0xffff ) >= 0x0200) && (caps.PS20Caps.NumInstructionSlots >= 512); // More caps to this, but this will do
	pCaps->m_SupportsVertexShaders_2_0 = ( caps.VertexShaderVersion & 0xffff ) >= 0x0200;
	pCaps->m_SupportsShaderModel_3_0 = ( caps.PixelShaderVersion & 0xffff ) >= 0x0300;
	pCaps->m_SupportsShaderModel_4_0 = false;
	pCaps->m_SupportsShaderModel_5_0 = false;
	pCaps->m_SupportsShaderModel_5_1 = false;
	pCaps->m_SupportsShaderModel_6_0 = false;
	pCaps->m_SupportsHullShaders = false;
	pCaps->m_SupportsDomainShaders = false;
	pCaps->m_SupportsComputeShaders = false;
	pCaps->m_SupportsMipmappedCubemaps = ( caps.TextureCaps & D3DPTEXTURECAPS_MIPCUBEMAP ) ? true : false;
#endif

	// Slam this off for OpenGL
	if ( IsOpenGL() )
	{
		pCaps->m_SupportsShaderModel_3_0 = false;
	}

	// Slam 3.0 shaders off for Intel
	if ( pCaps->m_VendorID == VENDORID_INTEL )
	{
		pCaps->m_SupportsShaderModel_3_0 = false;
	}

	pCaps->m_MaxVertexShader30InstructionSlots = 0;
	pCaps->m_MaxPixelShader30InstructionSlots  = 0;

	if ( pCaps->m_SupportsShaderModel_3_0 )
	{
		pCaps->m_MaxVertexShader30InstructionSlots = caps.MaxVertexShader30InstructionSlots;
		pCaps->m_MaxPixelShader30InstructionSlots  = caps.MaxPixelShader30InstructionSlots;
	}

	if( CommandLine()->CheckParm( "-nops2b" ) )
	{
		pCaps->m_SupportsPixelShaders_2_b = false;
	}

	pCaps->m_bSoftwareVertexProcessing = false;
	if ( IsWindows() && CommandLine()->CheckParm( "-mat_softwaretl" ) )
	{
		pCaps->m_bSoftwareVertexProcessing = true;
	}

	if ( IsWindows() && !( caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) )
	{
		// no hardware t&l. . use software
		pCaps->m_bSoftwareVertexProcessing = true;
	}

	// Set mat_forcedynamic if software vertex processing since the software vp pipe has 
	// problems with sparse vertex buffers (it transforms the whole thing.)
	if ( pCaps->m_bSoftwareVertexProcessing )
	{
		mat_forcedynamic.SetValue( 1 );

		pCaps->m_SupportsVertexShaders = true;
		pCaps->m_SupportsVertexShaders_2_0 = true;
	}

#ifdef OSX
	// Static control flow is disabled by default on OSX (the Mac version of togl has known bugs preventing this path from working properly that we've fixed in togl linux/win)
	pCaps->m_bSupportsStaticControlFlow = CommandLine()->CheckParm( "-glslcontrolflow" ) != NULL;
#else
	pCaps->m_bSupportsStaticControlFlow = !CommandLine()->CheckParm( "-noglslcontrolflow" );
#endif

	// NOTE: Texture stages is a fixed-function concept
	// NOTE: Normally, the number of texture units == the number of texture
	// stages except for NVidia hardware, which reports more stages than units.
	// The reason for this is because they expose the inner hardware pixel
	// pipeline through the extra stages. The only thing we use stages for
	// in the hardware is for configuring the color + alpha args + ops.
	pCaps->m_NumSamplers = caps.MaxSimultaneousTextures;
	pCaps->m_NumTextureStages = caps.MaxTextureBlendStages;
	if ( pCaps->m_SupportsPixelShaders_2_0 )
	{
		pCaps->m_NumSamplers = 16;
	}
	else
	{
		Assert( pCaps->m_NumSamplers <= pCaps->m_NumTextureStages );
	}

	// Clamp
	pCaps->m_NumSamplers = min( pCaps->m_NumSamplers, (int)MAX_SAMPLERS );
	pCaps->m_NumTextureStages = min( pCaps->m_NumTextureStages, (int)MAX_TEXTURE_STAGES );

	if ( D3DSupportsCompressedTextures() )
	{
		pCaps->m_SupportsCompressedTextures = COMPRESSED_TEXTURES_ON;
	}
	else
	{
		pCaps->m_SupportsCompressedTextures = COMPRESSED_TEXTURES_OFF;
	}

	pCaps->m_bSupportsAnisotropicFiltering = (caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) != 0;
	pCaps->m_bSupportsMagAnisotropicFiltering = (caps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFANISOTROPIC) != 0;

	// OpenGL does not support this--at least not on OSX which is the primary GL target, so just don't use that path on GL at all.
#if !defined( DX_TO_GL_ABSTRACTION )
	pCaps->m_bCanStretchRectFromTextures = ( ( caps.DevCaps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES ) != 0 ) && ( pCaps->m_VendorID != VENDORID_INTEL );
#else
	pCaps->m_bCanStretchRectFromTextures = false;
#endif

	pCaps->m_nMaxAnisotropy = pCaps->m_bSupportsAnisotropicFiltering ? caps.MaxAnisotropy : 1; 

	pCaps->m_SupportsCubeMaps = ( caps.TextureCaps & D3DPTEXTURECAPS_CUBEMAP ) ? true : false;
	pCaps->m_SupportsNonPow2Textures = 
		( !( caps.TextureCaps & D3DPTEXTURECAPS_POW2 ) || 
		( caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL ) );

	Assert( caps.TextureCaps & D3DPTEXTURECAPS_PROJECTED );

	if ( pCaps->m_bSoftwareVertexProcessing )
	{
		// This should be pushed down based on pixel shaders.
		pCaps->m_NumVertexShaderConstants = 256;
		pCaps->m_NumBooleanVertexShaderConstants = pCaps->m_SupportsPixelShaders_2_0 ? 16 : 0;	// 2.0 parts have 16 bool vs registers
		pCaps->m_NumBooleanPixelShaderConstants = pCaps->m_SupportsPixelShaders_2_0 ? 16 : 0;	// 2.0 parts have 16 bool ps registers
		pCaps->m_NumIntegerVertexShaderConstants = pCaps->m_SupportsPixelShaders_2_0 ? 16 : 0;	// 2.0 parts have 16 bool vs registers
		pCaps->m_NumIntegerPixelShaderConstants = pCaps->m_SupportsPixelShaders_2_0 ? 16 : 0;	// 2.0 parts have 16 bool ps registers
	}
	else
	{
		pCaps->m_NumVertexShaderConstants = caps.MaxVertexShaderConst;
		if ( CommandLine()->FindParm( "-limitvsconst" ) )
		{
			pCaps->m_NumVertexShaderConstants = min( 256, pCaps->m_NumVertexShaderConstants );
		}
		pCaps->m_NumBooleanVertexShaderConstants = pCaps->m_SupportsPixelShaders_2_0 ? 16 : 0;	// 2.0 parts have 16 bool vs registers
		pCaps->m_NumBooleanPixelShaderConstants = pCaps->m_SupportsPixelShaders_2_0 ? 16 : 0;	// 2.0 parts have 16 bool ps registers

		// This is a little misleading...this is really 16 int4 registers
		pCaps->m_NumIntegerVertexShaderConstants = pCaps->m_SupportsPixelShaders_2_0 ? 16 : 0;	// 2.0 parts have 16 bool vs registers
		pCaps->m_NumIntegerPixelShaderConstants = pCaps->m_SupportsPixelShaders_2_0 ? 16 : 0;	// 2.0 parts have 16 bool ps registers
	}

	if ( pCaps->m_SupportsPixelShaders )
	{
		if ( pCaps->m_SupportsPixelShaders_2_0 )
		{
			pCaps->m_NumPixelShaderConstants = 32;
		}
		else
		{
			pCaps->m_NumPixelShaderConstants = 8;
		}
	}
	else
	{
		pCaps->m_NumPixelShaderConstants = 0;
	}

	pCaps->m_SupportsHardwareLighting = (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0;

	pCaps->m_MaxNumLights = caps.MaxActiveLights;
	if ( pCaps->m_MaxNumLights > MAX_NUM_LIGHTS )
	{
		pCaps->m_MaxNumLights = MAX_NUM_LIGHTS;
	}

	if ( IsOpenGL() )
	{
		// Set according to control flow bit on OpenGL
		pCaps->m_MaxNumLights = MIN( pCaps->m_MaxNumLights, ( pCaps->m_bSupportsStaticControlFlow && pCaps->m_SupportsPixelShaders_2_b ) ? MAX_NUM_LIGHTS : ( MAX_NUM_LIGHTS - 2 ) );
	}

	if ( pCaps->m_bSoftwareVertexProcessing )
	{
		pCaps->m_SupportsHardwareLighting = true;
		pCaps->m_MaxNumLights = 2;
	}
	pCaps->m_MaxTextureWidth = caps.MaxTextureWidth;
	pCaps->m_MaxTextureHeight = caps.MaxTextureHeight;
	pCaps->m_MaxTextureDepth = caps.MaxVolumeExtent ? caps.MaxVolumeExtent : 1;
	pCaps->m_MaxTextureAspectRatio = caps.MaxTextureAspectRatio;
	if ( pCaps->m_MaxTextureAspectRatio == 0 )
	{
		pCaps->m_MaxTextureAspectRatio = max( pCaps->m_MaxTextureWidth, pCaps->m_MaxTextureHeight);
	}
	pCaps->m_MaxPrimitiveCount = caps.MaxPrimitiveCount;
	pCaps->m_MaxBlendMatrices = caps.MaxVertexBlendMatrices;
	pCaps->m_MaxBlendMatrixIndices = caps.MaxVertexBlendMatrixIndex;

	bool addSupported = (caps.TextureOpCaps & D3DTEXOPCAPS_ADD) != 0;
	bool modSupported = (caps.TextureOpCaps & D3DTEXOPCAPS_MODULATE2X) != 0;

	pCaps->m_bNeedsATICentroidHack = false;
	pCaps->m_bDisableShaderOptimizations = false;

	pCaps->m_SupportsMipmapping = true;
	pCaps->m_SupportsOverbright = true;

	// Thank you to all you driver writers who actually correctly return caps
	if ( !modSupported || !addSupported )
	{
		Assert( 0 );
		pCaps->m_SupportsOverbright = false;
	}

	// Check if ZBias and SlopeScaleDepthBias are supported. .if not, tweak the projection matrix instead
	// for polyoffset.
	pCaps->m_ZBiasAndSlopeScaledDepthBiasSupported =
		( ( caps.RasterCaps & D3DPRASTERCAPS_DEPTHBIAS) != 0 ) &&
		( ( caps.RasterCaps & D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS ) != 0 );

	// Spheremapping supported?
	pCaps->m_bSupportsSpheremapping = (caps.VertexProcessingCaps & D3DVTXPCAPS_TEXGEN_SPHEREMAP) != 0;

	// How many user clip planes?
	pCaps->m_MaxUserClipPlanes = caps.MaxUserClipPlanes;
	if ( CommandLine()->CheckParm( "-nouserclip" ) /* || (IsOpenGL() && (!CommandLine()->FindParm("-glslmode"))) || r_emulategl.GetBool() */ )
	{
		// rbarris 03Feb10: this now ignores POSIX / -glslmode / r_emulategl because we're defaulting GLSL mode "on".
		// so this will mean that the engine will always ask for user clip planes.
		// this will misbehave under ARB mode, since ARB shaders won't respect that state.
		// it's difficult to make this fluid without teaching the engine about a cap that could change during run.
		
		pCaps->m_MaxUserClipPlanes = 0;
	}

	if ( pCaps->m_MaxUserClipPlanes > MAXUSERCLIPPLANES )
	{
		pCaps->m_MaxUserClipPlanes = MAXUSERCLIPPLANES;
	}

	pCaps->m_FakeSRGBWrite = false;
	pCaps->m_CanDoSRGBReadFromRTs = true;
	pCaps->m_bSupportsGLMixedSizeTargets = false;
#ifdef DX_TO_GL_ABSTRACTION
	// using #if because we're referencing fields in the RHS which don't exist in Windows headers for the caps9 struct
	pCaps->m_FakeSRGBWrite = caps.FakeSRGBWrite != 0;
	pCaps->m_CanDoSRGBReadFromRTs = caps.CanDoSRGBReadFromRTs != 0;
	pCaps->m_bSupportsGLMixedSizeTargets = caps.MixedSizeTargets != 0; 	
#endif
	
	// Query for SRGB support as needed for our DX 9 stuff
	pCaps->m_SupportsSRGB = ( D3D()->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, m_AdapterImageFormat, D3DUSAGE_QUERY_SRGBREAD, D3DRTYPE_TEXTURE, D3DFMT_DXT1 ) == S_OK);

	if ( pCaps->m_SupportsSRGB )
	{
		pCaps->m_SupportsSRGB = ( D3D()->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, m_AdapterImageFormat, D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE, D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8 ) == S_OK);
	}

	if ( CommandLine()->CheckParm( "-nosrgb" ) )
	{
		pCaps->m_SupportsSRGB = false;
	}

	pCaps->m_bSupportsVertexTextures = ( D3D()->CheckDeviceFormat( nAdapter, DX8_DEVTYPE, m_AdapterImageFormat,
		D3DUSAGE_QUERY_VERTEXTEXTURE, D3DRTYPE_TEXTURE, D3DFMT_R32F ) == S_OK );

	if ( IsOpenGL() )
	{
		pCaps->m_bSupportsVertexTextures = false;
	}

	// Up t0 4 vertex textures supported (D3DVERTEXTEXTURESAMPLER0..3).
	pCaps->m_nVertexTextureCount = pCaps->m_bSupportsVertexTextures ? 4 : 0;

	// FIXME: How do I actually compute this?
	pCaps->m_nMaxVertexTextureDimension = pCaps->m_bSupportsVertexTextures ? 4096 : 0;

	// Does the device support filterable int16 textures?
	bool bSupportsInteger16Textures = 		
		( D3D()->CheckDeviceFormat( nAdapter, DX8_DEVTYPE,
		m_AdapterImageFormat, D3DUSAGE_QUERY_FILTER,
		D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16 ) == S_OK );

	// Does the device support filterable fp16 textures?
	bool bSupportsFloat16Textures = 		
		( D3D()->CheckDeviceFormat( nAdapter, DX8_DEVTYPE,
		m_AdapterImageFormat, D3DUSAGE_QUERY_FILTER,
		D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F ) == S_OK );

	// Does the device support blendable fp16 render targets?
	bool bSupportsFloat16RenderTargets = 		
		( D3D()->CheckDeviceFormat( nAdapter, DX8_DEVTYPE,
		m_AdapterImageFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING | D3DUSAGE_RENDERTARGET,
		D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F ) == S_OK );

	// Essentially a proxy for a DX10+ device running DX9 code path
	pCaps->m_bSupportsFloat32RenderTargets = ( D3D()->CheckDeviceFormat( nAdapter, DX8_DEVTYPE,
	m_AdapterImageFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING | D3DUSAGE_RENDERTARGET,
		D3DRTYPE_TEXTURE, D3DFMT_A32B32G32R32F ) == S_OK );

	pCaps->m_bFogColorSpecifiedInLinearSpace = false;
	pCaps->m_bFogColorAlwaysLinearSpace = false;

	// Assume not DX10.  Check below.
	pCaps->m_bDX10Card = false;
	pCaps->m_bDX10Blending = false;

	if ( IsOpenGL() && ( pCaps->m_VendorID == 1 ) )
	{
		// Linux/Win OpenGL - always assume the device supports DX10 style blending
		pCaps->m_bFogColorAlwaysLinearSpace = true;
		pCaps->m_bDX10Card = true;
		pCaps->m_bDX10Blending = true;
	}

	// NVidia wants fog color to be specified in linear space
	if ( IsPC() && pCaps->m_SupportsSRGB )
	{
		if ( pCaps->m_VendorID == VENDORID_NVIDIA )
		{
			pCaps->m_bFogColorSpecifiedInLinearSpace = true;

			if ( IsOpenGL() )
			{
				// If we're not the Quadro 4500 or GeForce 7x000, we're an NVIDIA DX10 part on MacOS
				if ( !( (pCaps->m_DeviceID == 0x009d) || ( (pCaps->m_DeviceID >= 0x0391) && (pCaps->m_DeviceID <= 0x0395) ) ) )
				{
					pCaps->m_bFogColorAlwaysLinearSpace = true;
					pCaps->m_bDX10Card = true;
					pCaps->m_bDX10Blending = true;
				}
			}
			else
			{	
				// On G80 and later, always specify in linear space
				if ( pCaps->m_bSupportsFloat32RenderTargets )
				{
					pCaps->m_bFogColorAlwaysLinearSpace = true;
					pCaps->m_bDX10Card = true;
					pCaps->m_bDX10Blending = true;
				}
			}
		}
		else if ( pCaps->m_VendorID == VENDORID_ATI )
		{
			if ( IsOpenGL() )
			{
				// If we're not a Radeon X1x00 (device IDs in this range), we're a DX10 chip
				if ( !( (pCaps->m_DeviceID >= 0x7109) && (pCaps->m_DeviceID <= 0x7291) ) )
				{
					pCaps->m_bFogColorSpecifiedInLinearSpace = true;
					pCaps->m_bFogColorAlwaysLinearSpace = true;
					pCaps->m_bDX10Card = true;
					pCaps->m_bDX10Blending = true;
				}
			}
			else
			{
				// Check for DX10 part
				pCaps->m_bDX10Card = pCaps->m_SupportsShaderModel_3_0 &&
				( pCaps->m_MaxVertexShader30InstructionSlots > 1024 ) &&
				( pCaps->m_MaxPixelShader30InstructionSlots > 512 ) ;
				
				// On ATI, DX10 card means DX10 blending
				pCaps->m_bDX10Blending = pCaps->m_bDX10Card;

				if( pCaps->m_bDX10Blending )
				{
					pCaps->m_bFogColorSpecifiedInLinearSpace = true;
					pCaps->m_bFogColorAlwaysLinearSpace = true;
				}
			}
		}
		else if ( pCaps->m_VendorID == VENDORID_INTEL )
		{
			// Intel does not have performant vertex textures
			pCaps->m_bDX10Card = false;

			// Intel supports DX10 SRGB on Broadwater and better
			// The two checks are for devices from GMA generation (0x29A2-0x2A43) and HD graphics (0x0042-0x2500)
			pCaps->m_bDX10Blending = ( ( pCaps->m_DeviceID >= 0x29A2 ) && ( pCaps->m_DeviceID <= 0x2A43  ) ) ||
									 ( ( pCaps->m_DeviceID >= 0x0042 ) && ( pCaps->m_DeviceID <= 0x2500  ) );

			if( pCaps->m_bDX10Blending )
			{
				pCaps->m_bFogColorSpecifiedInLinearSpace = true;
				pCaps->m_bFogColorAlwaysLinearSpace = true;
			}
		}
	}

	// Do we have everything necessary to run with integer HDR?  Note that
	// even if we don't support integer 16-bit/component textures, we
	// can still run in this mode if fp16 textures are supported.
	bool bSupportsIntegerHDR = pCaps->m_SupportsPixelShaders_2_0 &&
		pCaps->m_SupportsVertexShaders_2_0 &&
		//		(caps.Caps3 & D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD) &&
		//		(caps.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND) &&
		( bSupportsInteger16Textures || bSupportsFloat16Textures ) &&
		pCaps->m_SupportsSRGB;

	// Do we have everything necessary to run with float HDR?
	bool bSupportsFloatHDR = pCaps->m_SupportsShaderModel_3_0 &&
		//		(caps.Caps3 & D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD) &&
		//		(caps.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND) &&
		bSupportsFloat16Textures &&
		bSupportsFloat16RenderTargets &&
		pCaps->m_SupportsSRGB && 
		!IsX360();

	pCaps->m_MaxHDRType = HDR_TYPE_NONE;
	if ( bSupportsFloatHDR )
		pCaps->m_MaxHDRType = HDR_TYPE_FLOAT;
	else
		if ( bSupportsIntegerHDR )
			pCaps->m_MaxHDRType = HDR_TYPE_INTEGER;
		
	if ( bSupportsFloatHDR  && ( mat_hdr_level.GetInt() == 3 ) )
	{
		pCaps->m_HDRType = HDR_TYPE_FLOAT;
	}
	else if ( bSupportsIntegerHDR )
	{
		pCaps->m_HDRType = HDR_TYPE_INTEGER;
	}
	else
	{
		pCaps->m_HDRType = HDR_TYPE_NONE;
	}

	pCaps->m_bColorOnSecondStream = caps.MaxStreams > 1;

	pCaps->m_bSupportsStreamOffset = ( ( caps.DevCaps2 & D3DDEVCAPS2_STREAMOFFSET ) &&	// Tie these caps together since we want to filter out
										pCaps->m_SupportsPixelShaders_2_0 );			// any DX8 parts which export D3DDEVCAPS2_STREAMOFFSET

	pCaps->m_flMinGammaControlPoint = 0.0f;
	pCaps->m_flMaxGammaControlPoint = 65535.0f;
	pCaps->m_nGammaControlPointCount = 256;

	// Compute the effective DX support level based on all the other caps
	ComputeDXSupportLevel( *pCaps );
	int nCmdlineMaxDXLevel = CommandLine()->ParmValue( "-maxdxlevel", 0 );
	if ( IsOpenGL() && ( nCmdlineMaxDXLevel > 0 ) )
	{
		// Prevent customers from slamming us below DX level 90 in OpenGL mode.
		nCmdlineMaxDXLevel = MAX( nCmdlineMaxDXLevel, 90 );
	}
	if( nCmdlineMaxDXLevel > 0 )
	{
		pCaps->m_nMaxDXSupportLevel = min( pCaps->m_nMaxDXSupportLevel, nCmdlineMaxDXLevel );
	}
	pCaps->m_nDXSupportLevel = pCaps->m_nMaxDXSupportLevel;

	int nModelIndex = pCaps->m_nDXSupportLevel < 90 ? VERTEX_SHADER_MODEL - 10 : VERTEX_SHADER_MODEL;
	pCaps->m_MaxVertexShaderBlendMatrices = (pCaps->m_NumVertexShaderConstants - nModelIndex) / 3;

	if ( pCaps->m_MaxVertexShaderBlendMatrices > NUM_MODEL_TRANSFORMS )
	{
		pCaps->m_MaxVertexShaderBlendMatrices = NUM_MODEL_TRANSFORMS;
	}

	CheckBorderColorSupport( pCaps, nAdapter );

	// This may get more complex if we start using multiple flavors of compressed vertex - for now it's "on or off"
	pCaps->m_SupportsCompressedVertices = ( pCaps->m_nDXSupportLevel >= 90 ) && ( pCaps->m_CanDoSRGBReadFromRTs ) ? VERTEX_COMPRESSION_ON : VERTEX_COMPRESSION_NONE;
	if ( CommandLine()->CheckParm( "-no_compressed_verts" ) )						  // m_CanDoSRGBReadFromRTs limits us to Snow Leopard or later on OSX
	{
		pCaps->m_SupportsCompressedVertices = VERTEX_COMPRESSION_NONE;
	}

	// Various vendor-dependent checks...
	CheckVendorDependentAlphaToCoverage( pCaps, nAdapter );
	CheckVendorDependentShadowMappingSupport( pCaps, nAdapter );

	// If we're not on a 3.0 part, these values are more appropriate (X800 & X850 parts from ATI do shadow mapping but not 3.0 )
	if ( !IsOpenGL() )
	{
		if ( !pCaps->m_SupportsShaderModel_3_0 )
		{
			mat_slopescaledepthbias_shadowmap.SetValue( 5.9f );
			mat_depthbias_shadowmap.SetValue( 0.003f );
		}
	}

	if( pCaps->m_MaxUserClipPlanes == 0 )
	{
		pCaps->m_UseFastClipping = true;
	}

	pCaps->m_MaxSimultaneousRenderTargets = caps.NumSimultaneousRTs;

	return true;
}

//-----------------------------------------------------------------------------
// Compute the effective DX support level based on all the other caps
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::ComputeDXSupportLevel( HardwareCaps_t &caps )
{
	// NOTE: Support level is actually DX level * 10 + subversion
	// So, 70 = DX7, 80 = DX8, 81 = DX8 w/ 1.4 pixel shaders
	// 90 = DX9 w/ 2.0 pixel shaders
	// 95 = DX9 w/ 3.0 pixel shaders and vertex textures
	// 98 = DX9 XBox360
	// NOTE: 82 = NVidia nv3x cards, which can't run dx9 fast

	// FIXME: Improve this!! There should be a whole list of features
	// we require in order to be considered a DX7 board, DX8 board, etc.

	bool bIsOpenGL = IsOpenGL();

	if ( caps.m_SupportsShaderModel_3_0 && !bIsOpenGL ) // Note that we don't tie vertex textures to 30 shaders anymore
	{
		caps.m_nMaxDXSupportLevel = 95;
		return;
	}

	// NOTE: sRGB is currently required for DX90 because it isn't doing 
	// gamma correctly if that feature doesn't exist
	if ( caps.m_SupportsVertexShaders_2_0 && caps.m_SupportsPixelShaders_2_0 && caps.m_SupportsSRGB )
	{
		caps.m_nMaxDXSupportLevel = 90;
		return;
	}

	if ( caps.m_SupportsPixelShaders && caps.m_SupportsVertexShaders )// && caps.m_bColorOnSecondStream)
	{
		if (caps.m_SupportsPixelShaders_1_4)
		{
			caps.m_nMaxDXSupportLevel = 81;
			return;
		}
		caps.m_nMaxDXSupportLevel = 80;
		return;
	}

	if( caps.m_SupportsCubeMaps && ( caps.m_MaxBlendMatrices >= 2 ) )
	{
		caps.m_nMaxDXSupportLevel = 70;
		return;
	}

	if ( ( caps.m_NumSamplers >= 2) && caps.m_SupportsMipmapping )
	{
		caps.m_nMaxDXSupportLevel = 60;
		return;
	}

	Assert( 0 ); 
	// we don't support this!
	caps.m_nMaxDXSupportLevel = 50;
}



//-----------------------------------------------------------------------------
// Gets the number of adapters...
//-----------------------------------------------------------------------------
unsigned CShaderDeviceMgrDx8::GetAdapterCount() const
{
	// FIXME: Remove call to InitGpuInfo once Steam startup issues are resolved.
	// dimhotepus: Init adapters in Connect.
	// const_cast<CShaderDeviceMgrDx8*>( this )->InitGpuInfo();

	return static_cast<unsigned>(m_Adapters.Count());
}


//-----------------------------------------------------------------------------
// Returns info about each adapter
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::GetAdapterInfo( unsigned nAdapter, MaterialAdapterInfo_t& info ) const
{
	// FIXME: Remove call to InitGpuInfo once Steam startup issues are resolved.
	// dimhotepus: Init adapters in Connect.
	// const_cast<CShaderDeviceMgrDx8*>( this )->InitGpuInfo();

	Assert( ( nAdapter >= 0 ) && ( nAdapter < (unsigned)m_Adapters.Count() ) );
	const HardwareCaps_t &caps = m_Adapters[ nAdapter ].m_ActualCaps;
	memcpy( &info, &caps, sizeof(MaterialAdapterInfo_t) );
}


//-----------------------------------------------------------------------------
// Sets the adapter
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrDx8::SetAdapter( unsigned nAdapter, int nAdapterFlags )
{
	LOCK_SHADERAPI();

	// FIXME:
	//	g_pShaderDeviceDx8->m_bReadPixelsEnabled = (nAdapterFlags & MATERIAL_INIT_READ_PIXELS_ENABLED) != 0;

	// Set up hardware information for this adapter...
	g_pShaderDeviceDx8->m_DeviceType = (nAdapterFlags & MATERIAL_INIT_REFERENCE_RASTERIZER) ? 
		D3DDEVTYPE_REF : D3DDEVTYPE_HAL;
	g_pShaderDeviceDx8->m_DisplayAdapter = Clamp(nAdapter, 0U, GetAdapterCount() - 1);

#ifdef NVPERFHUD
	// hack for nvperfhud
	g_pShaderDeviceDx8->m_DisplayAdapter = m_pD3D->GetAdapterCount() - 1;
	g_pShaderDeviceDx8->m_DeviceType = D3DDEVTYPE_REF;
#endif

	// backward compat
	if ( !g_pShaderDeviceDx8->OnAdapterSet() )
		return false;

//	if ( !g_pShaderDeviceDx8->Init() )
//	{
//		Warning( "Unable to initialize dx8 device!\n" );
//		return false;
//	}

	g_pShaderDevice = g_pShaderDeviceDx8;

	return true;
}


//-----------------------------------------------------------------------------
// Returns the number of modes
//-----------------------------------------------------------------------------
unsigned CShaderDeviceMgrDx8::GetModeCount( unsigned nAdapter ) const
{
	LOCK_SHADERAPI();
	Assert( m_pD3D && nAdapter < GetAdapterCount() );

	D3DDISPLAYMODEFILTER filter = {};
	filter.Size = sizeof(filter);
	filter.Format = m_AdapterImageFormat;
	filter.ScanLineOrdering = m_AdapterEnumScanlineOrdering;
	return m_pD3D->GetAdapterModeCountEx( nAdapter, &filter );
}


//-----------------------------------------------------------------------------
// Returns mode information..
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::GetModeInfo( ShaderDisplayMode_t* pInfo, unsigned nAdapter, unsigned nMode ) const
{
	Assert( pInfo->m_nVersion == SHADER_DISPLAY_MODE_VERSION );

	LOCK_SHADERAPI();
	Assert( m_pD3D && nAdapter < GetAdapterCount() );
	Assert( nMode < GetModeCount( nAdapter ) );

	D3DDISPLAYMODEEX mode = {};
	mode.Size = sizeof(mode);
	
	D3DDISPLAYMODEFILTER filter = {};
	filter.Size = sizeof(filter);
	filter.Format = m_AdapterImageFormat;
	filter.ScanLineOrdering = m_AdapterEnumScanlineOrdering;

	const HRESULT hr = D3D()->EnumAdapterModesEx( nAdapter, &filter, nMode, &mode );
	if ( FAILED(hr) )
	{
		Error( "IDirect3D9Ex::EnumAdapterModesEx(adapter = %u, format = 0x%x, mode = %u) failed w/e %s.\n",
			nAdapter,
			m_AdapterImageFormat,
			nMode,
			se::win::com::com_error_category().message(hr).c_str() );
	}

	pInfo->m_nWidth      = mode.Width;
	pInfo->m_nHeight     = mode.Height;
	pInfo->m_Format      = ImageLoader::D3DFormatToImageFormat( mode.Format );
	pInfo->m_nRefreshRateNumerator = mode.RefreshRate;
	pInfo->m_nRefreshRateDenominator = 1;
}


//-----------------------------------------------------------------------------
// Returns the current mode information for an adapter
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx8::GetCurrentModeInfo( ShaderDisplayMode_t* pInfo, unsigned nAdapter ) const
{
	Assert( pInfo->m_nVersion == SHADER_DISPLAY_MODE_VERSION );

	LOCK_SHADERAPI();
	Assert( D3D() );

	D3DDISPLAYMODEEX mode = {};
	mode.Size = sizeof(mode);

	D3DDISPLAYROTATION rotation;
	HRESULT hr = D3D()->GetAdapterDisplayModeEx(nAdapter, &mode, &rotation);
	if ( FAILED(hr) )
	{
		Error( "IDirect3D9Ex::GetAdapterDisplayModeEx(adapter = %u) failed w/e %s.\n",
			nAdapter,
			se::win::com::com_error_category().message(hr).c_str() );
	}

	pInfo->m_nWidth = mode.Width;
	pInfo->m_nHeight = mode.Height;
	pInfo->m_Format = ImageLoader::D3DFormatToImageFormat( mode.Format );
	pInfo->m_nRefreshRateNumerator = mode.RefreshRate;
	pInfo->m_nRefreshRateDenominator = 1;
}


//-----------------------------------------------------------------------------
// Sets the video mode
//-----------------------------------------------------------------------------
CreateInterfaceFn CShaderDeviceMgrDx8::SetMode( void *hWnd, unsigned nAdapter, const ShaderDeviceInfo_t& mode )
{
	LOCK_SHADERAPI();

	Assert( nAdapter < GetAdapterCount() );
	int nDXLevel = mode.m_nDXLevel != 0 ? mode.m_nDXLevel : m_Adapters[nAdapter].m_ActualCaps.m_nDXSupportLevel;
	if ( m_bObeyDxCommandlineOverride )
	{
		nDXLevel = CommandLine()->ParmValue( "-dxlevel", nDXLevel );
		m_bObeyDxCommandlineOverride = false;
	}
	if ( nDXLevel > m_Adapters[nAdapter].m_ActualCaps.m_nMaxDXSupportLevel )
	{
		nDXLevel = m_Adapters[nAdapter].m_ActualCaps.m_nMaxDXSupportLevel;
	}
	nDXLevel = GetClosestActualDXLevel( nDXLevel );

	if ( nDXLevel >= 100 )
		return NULL;

	bool bReacquireResourcesNeeded = false;
	if ( g_pShaderDevice )
	{
		bReacquireResourcesNeeded = IsPC();
		g_pShaderDevice->ReleaseResources();
	}

	if ( g_pShaderAPI )
	{
		g_pShaderAPI->OnDeviceShutdown();
		g_pShaderAPI = NULL;
	}

	if ( g_pShaderDevice )
	{
		g_pShaderDevice->ShutdownDevice();
		g_pShaderDevice = NULL;
	}

	g_pShaderShadow = NULL;

	ShaderDeviceInfo_t adjustedMode = mode;
	adjustedMode.m_nDXLevel = nDXLevel;
	if ( !g_pShaderDeviceDx8->InitDevice( hWnd, nAdapter, adjustedMode ) )
		return NULL;

	if ( !g_pShaderAPIDX8->OnDeviceInit() )
		return NULL;

	g_pShaderDevice = g_pShaderDeviceDx8;
	g_pShaderAPI = g_pShaderAPIDX8;
	g_pShaderShadow = g_pShaderShadowDx8;

	if ( bReacquireResourcesNeeded )
	{
		g_pShaderDevice->ReacquireResources();
	}

	return ShaderInterfaceFactory;
}


//-----------------------------------------------------------------------------
// Validates the mode...
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrDx8::ValidateMode( unsigned nAdapter, const ShaderDeviceInfo_t &info ) const
{
	if ( nAdapter >= D3D()->GetAdapterCount() )
		return false;

	ShaderDisplayMode_t displayMode;

	if ( info.m_bWindowed )
	{
		// windowed mode always appears on the primary display, so we should use that adapter's
		// settings
		GetCurrentModeInfo( &displayMode, 0 );

		// make sure the window fits within the current video mode
		if ( ( info.m_DisplayMode.m_nWidth > displayMode.m_nWidth ) ||
			 ( info.m_DisplayMode.m_nHeight > displayMode.m_nHeight ) )
			return false;
	}
	else
	{
		GetCurrentModeInfo( &displayMode, nAdapter );
	}

	// Make sure the image format requested is valid
	ImageFormat backBufferFormat = FindNearestSupportedBackBufferFormat( nAdapter,
		DX8_DEVTYPE, displayMode.m_Format, info.m_DisplayMode.m_Format, info.m_bWindowed );
	return backBufferFormat != IMAGE_FORMAT_UNKNOWN;
}


//-----------------------------------------------------------------------------
// Returns the amount of video memory in bytes for a particular adapter
//-----------------------------------------------------------------------------
size_t CShaderDeviceMgrDx8::GetVidMemBytes( unsigned nAdapter ) const
{
#if defined (DX_TO_GL_ABSTRACTION)
	D3DADAPTER_IDENTIFIER9 devIndentifier;
	D3D()->GetAdapterIdentifier( nAdapter, D3DENUM_WHQL_LEVEL, &devIndentifier );
	return devIndentifier.VideoMemory;
#else
	return ::GetVidMemBytes(nAdapter);
#endif
}



//-----------------------------------------------------------------------------
//
// Shader device
//
//-----------------------------------------------------------------------------

#if 0
// FIXME: Enable after I've separated it out from shaderapidx8 a little better
static CShaderDeviceDx8 s_ShaderDeviceDX8;
CShaderDeviceDx8* g_pShaderDeviceDx8 = &s_ShaderDeviceDX8;
#endif


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CShaderDeviceDx8::CShaderDeviceDx8()
{
	m_DisplayAdapter = UINT_MAX;
	m_DeviceType = D3DDEVTYPE_FORCE_DWORD;

	memset( &m_PresentParameters, 0, sizeof(m_PresentParameters) );
	m_AdapterFormat = IMAGE_FORMAT_UNKNOWN;

	for ( size_t i = 0; i < std::size(m_pFrameSyncQueryObject); i++ )
	{
		m_bQueryIssued[i] = false;
	}
	m_currentSyncQuery = 0;

	m_bQueuedDeviceLost = false;
	m_bQueuedDeviceHung = false;
	m_bQueuedDeviceOutOfGpuMemory = false;
	m_bRenderingOccluded = false;
	m_DeviceState = DEVICE_STATE_OK;
	m_bOtherAppInitializing = false;
	m_IsResizing = false;
	m_bPendingVideoModeChange = false;
	m_DeviceSupportsCreateQuery = -1;
	m_bUsingStencil = false;
	m_bResourcesReleased = false;
	m_iStencilBufferBits = 0;
	m_numReleaseResourcesRefCount = 0;
#ifdef DEBUG
	m_createDeviceThreadId = std::numeric_limits<ThreadId_t>::max();
#endif
}

CShaderDeviceDx8::~CShaderDeviceDx8()
{
}


//-----------------------------------------------------------------------------
// Computes device creation paramters
//-----------------------------------------------------------------------------
static DWORD ComputeDeviceCreationFlags( D3DCAPS& caps, bool bSoftwareVertexProcessing )
{
	// Find out what type of device to make
	bool bPureDeviceSupported = (caps.DevCaps & D3DDEVCAPS_PUREDEVICE) != 0;

	DWORD nDeviceCreationFlags;
	if ( !bSoftwareVertexProcessing )
	{
		nDeviceCreationFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
		if ( bPureDeviceSupported )
		{
			nDeviceCreationFlags |= D3DCREATE_PUREDEVICE;
		}
	}
	else
	{
		nDeviceCreationFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	}
	nDeviceCreationFlags |= D3DCREATE_FPU_PRESERVE;

	return nDeviceCreationFlags;
}


//-----------------------------------------------------------------------------
// Computes the supersample flags
//-----------------------------------------------------------------------------
D3DMULTISAMPLE_TYPE CShaderDeviceDx8::ComputeMultisampleType( int nSampleCount )
{
	switch (nSampleCount)
	{
	case 2: return D3DMULTISAMPLE_2_SAMPLES;
	case 3: return D3DMULTISAMPLE_3_SAMPLES;
	case 4: return D3DMULTISAMPLE_4_SAMPLES;
	case 5: return D3DMULTISAMPLE_5_SAMPLES;
	case 6: return D3DMULTISAMPLE_6_SAMPLES;
	case 7: return D3DMULTISAMPLE_7_SAMPLES;
	case 8: return D3DMULTISAMPLE_8_SAMPLES;
	case 9: return D3DMULTISAMPLE_9_SAMPLES;
	case 10: return D3DMULTISAMPLE_10_SAMPLES;
	case 11: return D3DMULTISAMPLE_11_SAMPLES;
	case 12: return D3DMULTISAMPLE_12_SAMPLES;
	case 13: return D3DMULTISAMPLE_13_SAMPLES;
	case 14: return D3DMULTISAMPLE_14_SAMPLES;
	case 15: return D3DMULTISAMPLE_15_SAMPLES;
	case 16: return D3DMULTISAMPLE_16_SAMPLES;
	default:
	case 0:
	case 1:
		return D3DMULTISAMPLE_NONE;
	}
}


//-----------------------------------------------------------------------------
// Sets the present parameters
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::SetPresentParameters( void* hWnd, unsigned nAdapter, const ShaderDeviceInfo_t &info )
{
	ShaderDisplayMode_t mode;
	g_pShaderDeviceMgr->GetCurrentModeInfo( &mode, nAdapter );

	BitwiseClear( m_PresentParameters );

	D3DSWAPEFFECT swap_effect{D3DSWAPEFFECT_FORCE_DWORD};
	if (info.m_bUsingMultipleWindows)
	{
		// D3DSWAPEFFECT_COPY requires single back buffer.
		// See https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dpresent-parameters
		Assert(info.m_nBackBufferCount == 1);
		swap_effect = D3DSWAPEFFECT_COPY;
	}
	else if (info.m_bUsingPartialPresentation)
	{
		swap_effect = D3DSWAPEFFECT_DISCARD;
	}
	else
	{
		// dimhotepus: Add modern, fast D3DSWAPEFFECT_FLIPEX. 

		// D3DSWAPEFFECT_FLIPEX requires 2+ back buffers.
		Assert(info.m_nBackBufferCount >= 2);
		swap_effect = D3DSWAPEFFECT_FLIPEX;
	}
	
	m_PresentParameters.Windowed = info.m_bWindowed;
	m_PresentParameters.SwapEffect = swap_effect;
	m_PresentParameters.EnableAutoDepthStencil = TRUE;

	// What back-buffer format should we use?
	ImageFormat backBufferFormat = FindNearestSupportedBackBufferFormat( nAdapter,
		DX8_DEVTYPE, m_AdapterFormat, info.m_DisplayMode.m_Format, info.m_bWindowed );

	// What depth format should we use?
	// always stencil for dx9/hdr
	m_bUsingStencil = info.m_bUseStencil || info.m_nDXLevel >= 80;
	D3DFORMAT nDepthFormat = m_bUsingStencil ? D3DFMT_D24S8 : D3DFMT_D24X8;
	m_PresentParameters.AutoDepthStencilFormat = FindNearestSupportedDepthFormat(
		nAdapter, m_AdapterFormat, backBufferFormat, nDepthFormat );
	m_PresentParameters.hDeviceWindow = (VD3DHWND)hWnd;

	// store how many stencil buffer bits we have available with the depth/stencil buffer
	switch( m_PresentParameters.AutoDepthStencilFormat )
	{
	case D3DFMT_D24S8:
		m_iStencilBufferBits = 8;
		break;
	case D3DFMT_D24X4S4:
		m_iStencilBufferBits = 4;
		break;
	case D3DFMT_D15S1:
		m_iStencilBufferBits = 1;
		break;
	default:
		m_iStencilBufferBits = 0;
		m_bUsingStencil = false; //couldn't acquire a stencil buffer
	};

	if ( !info.m_bWindowed )
	{
		bool useDefault = ( info.m_DisplayMode.m_nWidth == 0 ) || ( info.m_DisplayMode.m_nHeight == 0 );
		m_PresentParameters.BackBufferWidth = useDefault ? mode.m_nWidth : info.m_DisplayMode.m_nWidth;
		m_PresentParameters.BackBufferHeight = useDefault ? mode.m_nHeight : info.m_DisplayMode.m_nHeight;
		m_PresentParameters.BackBufferFormat = ImageLoader::ImageFormatToD3DFormat( backBufferFormat );
		m_PresentParameters.BackBufferCount = info.m_nBackBufferCount;
		if ( !info.m_bWaitForVSync || CommandLine()->FindParm( "-forcenovsync" ) )
		{
			m_PresentParameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
		}
		else
		{
			m_PresentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
		}

		m_PresentParameters.FullScreen_RefreshRateInHz = info.m_DisplayMode.m_nRefreshRateDenominator ? 
			info.m_DisplayMode.m_nRefreshRateNumerator / info.m_DisplayMode.m_nRefreshRateDenominator : D3DPRESENT_RATE_DEFAULT;
	}
	else
	{
		// NJS: We are seeing a lot of time spent in present in some cases when this isn't set.
		m_PresentParameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
		if ( info.m_bResizing )
		{
			if ( info.m_bLimitWindowedSize &&
				( info.m_nWindowedSizeLimitWidth < mode.m_nWidth || info.m_nWindowedSizeLimitHeight < mode.m_nHeight ) )
			{
				// When using material system in windowed resizing apps, it's
				// sometimes not a good idea to allocate stuff as big as the screen
				// video cards can soo run out of resources
				m_PresentParameters.BackBufferWidth = info.m_nWindowedSizeLimitWidth;
				m_PresentParameters.BackBufferHeight = info.m_nWindowedSizeLimitHeight;
			}
			else
			{
				// When in resizing windowed mode, 
				// we want to allocate enough memory to deal with any resizing...
				m_PresentParameters.BackBufferWidth = mode.m_nWidth;
				m_PresentParameters.BackBufferHeight = mode.m_nHeight;
			}
		}
		else
		{
			m_PresentParameters.BackBufferWidth = info.m_DisplayMode.m_nWidth;
			m_PresentParameters.BackBufferHeight = info.m_DisplayMode.m_nHeight;
		}
		m_PresentParameters.BackBufferFormat = ImageLoader::ImageFormatToD3DFormat( backBufferFormat );
		m_PresentParameters.BackBufferCount = info.m_nBackBufferCount;
	}

	if ( info.m_nAASamples > 0 && ( m_PresentParameters.SwapEffect == D3DSWAPEFFECT_DISCARD ) )
	{
		D3DMULTISAMPLE_TYPE multiSampleType = ComputeMultisampleType( info.m_nAASamples );
		DWORD nQualityLevel;

		HRESULT hr;

		// FIXME: Should we add the quality level to the ShaderAdapterMode_t struct?
		// 16x on nVidia refers to CSAA or "Coverage Sampled Antialiasing"
		const HardwareCaps_t &adapterCaps = g_ShaderDeviceMgrDx8.GetHardwareCaps( nAdapter );
		if ( ( info.m_nAASamples == 16 ) && ( adapterCaps.m_VendorID == VENDORID_NVIDIA ) )
		{
			multiSampleType = ComputeMultisampleType(4);
			hr = D3D()->CheckDeviceMultiSampleType( nAdapter, DX8_DEVTYPE, 
				m_PresentParameters.BackBufferFormat, m_PresentParameters.Windowed, 
				multiSampleType, &nQualityLevel );						// 4x at highest quality level

			if ( !FAILED( hr ) && ( nQualityLevel == 16 ) )
			{
				nQualityLevel = nQualityLevel - 1;						// Highest quality level triggers 16x CSAA
			}
			else
			{
				nQualityLevel  = 0;										// No CSAA
			}
		}
		else	// Regular MSAA on any old vendor
		{
			hr = D3D()->CheckDeviceMultiSampleType( nAdapter, DX8_DEVTYPE, 
				m_PresentParameters.BackBufferFormat, m_PresentParameters.Windowed, 
				multiSampleType, &nQualityLevel );

			nQualityLevel = 0;
		}

		if ( !FAILED( hr ) )
		{
			m_PresentParameters.MultiSampleType = multiSampleType;
			m_PresentParameters.MultiSampleQuality = nQualityLevel;
		}
	}
	else
	{
		m_PresentParameters.MultiSampleType = D3DMULTISAMPLE_NONE;
		m_PresentParameters.MultiSampleQuality = 0;
	}
}


//-----------------------------------------------------------------------------
// Computes display mode from presentation parameters.
//-----------------------------------------------------------------------------
D3DDISPLAYMODEEX CShaderDeviceDx8::GetFullScreenDisplayModeFromPresentParameters( const D3DPRESENT_PARAMETERS &parameters )
{
	D3DDISPLAYMODEEX displayMode;
	BitwiseClear(displayMode);

	displayMode.Size = sizeof(displayMode);
	displayMode.Width = parameters.BackBufferWidth;
	displayMode.Height = parameters.BackBufferHeight;
	displayMode.RefreshRate = parameters.FullScreen_RefreshRateInHz;
	displayMode.Format = parameters.BackBufferFormat;
	displayMode.ScanLineOrdering = D3DSCANLINEORDERING_UNKNOWN;

	return displayMode;
}


//-----------------------------------------------------------------------------
// Initializes, shuts down the D3D device
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::InitDevice( void* hwnd, unsigned nAdapter, const ShaderDeviceInfo_t &info )
{
	// windowed
	if ( !CreateD3DDevice( hwnd, nAdapter, info ) )
		return false;

	// Hook up our own windows proc to get at messages to tell us when
	// other instances of the material system are trying to set the mode
	InstallWindowHook( m_hWnd );
	return true;
}

void CShaderDeviceDx8::ShutdownDevice()
{
	if ( IsActive() )
	{
		m_d3d9_device_ex.Release();

#ifdef STUBD3D
		delete ( CStubD3DDevice * )D3D9Device();
#endif

		RemoveWindowHook( (VD3DHWND)m_hWnd );
		m_hWnd = 0;
	}
}


//-----------------------------------------------------------------------------
// Are we using graphics?
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::IsUsingGraphics() const
{
	//*****LOCK_SHADERAPI();
	return IsActive();
}


//-----------------------------------------------------------------------------
// Returns the current adapter in use
//-----------------------------------------------------------------------------
unsigned CShaderDeviceDx8::GetCurrentAdapter() const
{
	LOCK_SHADERAPI();
	return m_DisplayAdapter;
}


//-----------------------------------------------------------------------------
// Returns the current adapter in use
//-----------------------------------------------------------------------------
const char *CShaderDeviceDx8::GetDisplayDeviceName() 
{
	if( m_sDisplayDeviceName.IsEmpty() )
	{
		D3DADAPTER_IDENTIFIER9 ident;
		const unsigned adapter = m_nAdapter == UINT_MAX ? 0U : m_nAdapter;

		// On Win10, this function is getting called with m_nAdapter still initialized to -1.
		// It's failing, and m_sDisplayDeviceName has garbage, and tf2 fails to launch.
		// To repro this, run "hl2.exe -dev -fullscreen -game tf" on Win10.
		const HRESULT hr = D3D()->GetAdapterIdentifier( adapter, 0, &ident );
		if ( FAILED(hr) )
		{
			Assert(false);
			Warning( "IDirect3D9Ex::GetAdapterIdentifier(adapter = %u) failed w/e %s.\n",
				m_nAdapter,
				se::win::com::com_error_category().message(hr).c_str() );
			ident.DeviceName[0] = '\0';
		}
		m_sDisplayDeviceName = ident.DeviceName;
	}
	return m_sDisplayDeviceName.Get();
}


//-----------------------------------------------------------------------------
// Use this to spew information about the 3D layer 
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::SpewDriverInfo() const
{
	LOCK_SHADERAPI();
	D3DCAPS caps;
	D3DADAPTER_IDENTIFIER9 ident;

	RECORD_COMMAND( DX8_GET_DEVICE_CAPS, 0 );

	RECORD_COMMAND( DX8_GET_ADAPTER_IDENTIFIER, 2 );
	RECORD_INT( m_nAdapter );
	RECORD_INT( 0 );

	HRESULT hr = m_d3d9_device_ex->GetDeviceCaps(&caps);
	if ( FAILED(hr) )
	{
		Assert(false);
		Warning( "IDirect3D9Ex::GetDeviceCaps() failed w/e %s.\n",
			se::win::com::com_error_category().message(hr).c_str() );
		return;
	}

	hr = D3D()->GetAdapterIdentifier( m_nAdapter, D3DENUM_WHQL_LEVEL, &ident );
	if ( FAILED(hr) )
	{
		Assert(false);
		Warning( "IDirect3D9Ex::GetAdapterIdentifier(adapter = %u, flags = D3DENUM_WHQL_LEVEL) failed w/e %s.\n",
			m_nAdapter,
			se::win::com::com_error_category().message(hr).c_str() );
		return;
	}

	// dimhotepus: Dump version in high.low format.
	Warning("Shader API Driver Info:\n\nDriver : %s Version : %lu.%lu\n",
		ident.Driver, ident.DriverVersion.HighPart, ident.DriverVersion.LowPart );
	Warning("Driver Description :  %s\n", ident.Description );
	// dimhotepus: Dump version with dots.
	Warning("GPU version %lu.%lu.%lu.%lu\n\n", 
		ident.VendorId, ident.DeviceId, ident.SubSysId, ident.Revision );

	ShaderDisplayMode_t mode;
	g_pShaderDeviceMgr->GetCurrentModeInfo( &mode, m_nAdapter );
	Warning("Display mode : %d x %d (%s)\n", 
		mode.m_nWidth, mode.m_nHeight, ImageLoader::GetName( mode.m_Format ) );
	Warning("Vertex Shader Version : %d.%d Pixel Shader Version : %d.%d\n",
		(caps.VertexShaderVersion >> 8) & 0xFF, caps.VertexShaderVersion & 0xFF,
		(caps.PixelShaderVersion >> 8) & 0xFF, caps.PixelShaderVersion & 0xFF);
	Warning("\nDevice Caps :\n");
	Warning("CANBLTSYSTONONLOCAL %s CANRENDERAFTERFLIP %s HWRASTERIZATION %s\n",
		(caps.DevCaps & D3DDEVCAPS_CANBLTSYSTONONLOCAL) ? " Y " : " N ",
		(caps.DevCaps & D3DDEVCAPS_CANRENDERAFTERFLIP) ? " Y " : " N ",
		(caps.DevCaps & D3DDEVCAPS_HWRASTERIZATION) ? " Y " : "*N*" );
	Warning("HWTRANSFORMANDLIGHT %s NPATCHES %s PUREDEVICE %s\n",
		(caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) ? " Y " : " N ",
		(caps.DevCaps & D3DDEVCAPS_NPATCHES) ? " Y " : " N ",
		(caps.DevCaps & D3DDEVCAPS_PUREDEVICE) ? " Y " : " N " );
	Warning("SEPARATETEXTUREMEMORIES %s TEXTURENONLOCALVIDMEM %s TEXTURESYSTEMMEMORY %s\n",
		(caps.DevCaps & D3DDEVCAPS_SEPARATETEXTUREMEMORIES) ? "*Y*" : " N ",
		(caps.DevCaps & D3DDEVCAPS_TEXTURENONLOCALVIDMEM) ? " Y " : " N ",
		(caps.DevCaps & D3DDEVCAPS_TEXTURESYSTEMMEMORY) ? " Y " : " N " );
	Warning("TEXTUREVIDEOMEMORY %s TLVERTEXSYSTEMMEMORY %s TLVERTEXVIDEOMEMORY %s\n",
		(caps.DevCaps & D3DDEVCAPS_TEXTUREVIDEOMEMORY) ? " Y " : "*N*",
		(caps.DevCaps & D3DDEVCAPS_TLVERTEXSYSTEMMEMORY) ? " Y " : "*N*",
		(caps.DevCaps & D3DDEVCAPS_TLVERTEXVIDEOMEMORY) ? " Y " : " N " );

	Warning("\nPrimitive Caps :\n");
	Warning("BLENDOP %s CLIPPLANESCALEDPOINTS %s CLIPTLVERTS %s\n",
		(caps.PrimitiveMiscCaps & D3DPMISCCAPS_BLENDOP) ? " Y " : " N ",
		(caps.PrimitiveMiscCaps & D3DPMISCCAPS_CLIPPLANESCALEDPOINTS) ? " Y " : " N ",
		(caps.PrimitiveMiscCaps & D3DPMISCCAPS_CLIPTLVERTS) ? " Y " : " N " );
	Warning("COLORWRITEENABLE %s MASKZ %s TSSARGTEMP %s\n",
		(caps.PrimitiveMiscCaps & D3DPMISCCAPS_COLORWRITEENABLE) ? " Y " : " N ",
		(caps.PrimitiveMiscCaps & D3DPMISCCAPS_MASKZ) ? " Y " : "*N*",
		(caps.PrimitiveMiscCaps & D3DPMISCCAPS_TSSARGTEMP) ? " Y " : " N " );

	Warning("\nRaster Caps :\n");
	Warning("FOGRANGE %s FOGTABLE %s FOGVERTEX %s ZFOG %s WFOG %s\n",
		(caps.RasterCaps & D3DPRASTERCAPS_FOGRANGE) ? " Y " : " N ",
		(caps.RasterCaps & D3DPRASTERCAPS_FOGTABLE) ? " Y " : " N ",
		(caps.RasterCaps & D3DPRASTERCAPS_FOGVERTEX) ? " Y " : " N ",
		(caps.RasterCaps & D3DPRASTERCAPS_ZFOG) ? " Y " : " N ",
		(caps.RasterCaps & D3DPRASTERCAPS_WFOG) ? " Y " : " N " );
	Warning("MIPMAPLODBIAS %s WBUFFER %s ZBIAS %s ZTEST %s\n",
		(caps.RasterCaps & D3DPRASTERCAPS_MIPMAPLODBIAS) ? " Y " : " N ",
		(caps.RasterCaps & D3DPRASTERCAPS_WBUFFER) ? " Y " : " N ",
		(caps.RasterCaps & D3DPRASTERCAPS_DEPTHBIAS) ? " Y " : " N ",
		(caps.RasterCaps & D3DPRASTERCAPS_ZTEST) ? " Y " : "*N*" );

	Warning("Size of Texture Memory : %zu KiB\n", g_pHardwareConfig->Caps().m_TextureMemorySize / 1024 );
	Warning("Max Texture Dimensions : %d x %d\n", 
		caps.MaxTextureWidth, caps.MaxTextureHeight );
	if (caps.MaxTextureAspectRatio != 0)
		Warning("Max Texture Aspect Ratio : *%d*\n", caps.MaxTextureAspectRatio );
	Warning("Max Textures : %d Max Stages : %d\n", 
		caps.MaxSimultaneousTextures, caps.MaxTextureBlendStages );

	Warning("\nTexture Caps :\n");
	Warning("ALPHA %s CUBEMAP %s MIPCUBEMAP %s SQUAREONLY %s\n",
		(caps.TextureCaps & D3DPTEXTURECAPS_ALPHA) ? " Y " : " N ",
		(caps.TextureCaps & D3DPTEXTURECAPS_CUBEMAP) ? " Y " : " N ",
		(caps.TextureCaps & D3DPTEXTURECAPS_MIPCUBEMAP) ? " Y " : " N ",
		(caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) ? "*Y*" : " N " );

	Warning( "vendor id: 0x%x\n", g_pHardwareConfig->ActualCaps().m_VendorID );
	Warning( "device id: 0x%x\n", g_pHardwareConfig->ActualCaps().m_DeviceID );

	Warning( "SHADERAPI CAPS:\n" );
	Warning( "m_NumSamplers: %d\n", g_pHardwareConfig->Caps().m_NumSamplers );
	Warning( "m_NumTextureStages: %d\n", g_pHardwareConfig->Caps().m_NumTextureStages );
	Warning( "m_HasSetDeviceGammaRamp: %s\n", g_pHardwareConfig->Caps().m_HasSetDeviceGammaRamp ? "yes" : "no" );
	Warning( "m_SupportsVertexShaders (1.1): %s\n", g_pHardwareConfig->Caps().m_SupportsVertexShaders ? "yes" : "no" );
	Warning( "m_SupportsVertexShaders_2_0: %s\n", g_pHardwareConfig->Caps().m_SupportsVertexShaders_2_0 ? "yes" : "no" );
	Warning( "m_SupportsPixelShaders (1.1): %s\n", g_pHardwareConfig->Caps().m_SupportsPixelShaders ? "yes" : "no" );
	Warning( "m_SupportsPixelShaders_1_4: %s\n", g_pHardwareConfig->Caps().m_SupportsPixelShaders_1_4 ? "yes" : "no" );
	Warning( "m_SupportsPixelShaders_2_0: %s\n", g_pHardwareConfig->Caps().m_SupportsPixelShaders_2_0 ? "yes" : "no" );
	Warning( "m_SupportsPixelShaders_2_b: %s\n", g_pHardwareConfig->Caps().m_SupportsPixelShaders_2_b ? "yes" : "no" );
	Warning( "m_SupportsShaderModel_3_0: %s\n", g_pHardwareConfig->Caps().m_SupportsShaderModel_3_0 ? "yes" : "no" );
	
	switch( g_pHardwareConfig->Caps().m_SupportsCompressedTextures )
	{
	case COMPRESSED_TEXTURES_ON:
		Warning( "m_SupportsCompressedTextures: COMPRESSED_TEXTURES_ON\n" );
		break;
	case COMPRESSED_TEXTURES_OFF:
		Warning( "m_SupportsCompressedTextures: COMPRESSED_TEXTURES_OFF\n" );
		break;
	case COMPRESSED_TEXTURES_NOT_INITIALIZED:
		Warning( "m_SupportsCompressedTextures: COMPRESSED_TEXTURES_NOT_INITIALIZED\n" );
		break;
	default:
		Assert( 0 );
		break;
	}
	Warning( "m_SupportsCompressedVertices: %d\n", g_pHardwareConfig->Caps().m_SupportsCompressedVertices );
	Warning( "m_bSupportsAnisotropicFiltering: %s\n", g_pHardwareConfig->Caps().m_bSupportsAnisotropicFiltering ? "yes" : "no" );
	Warning( "m_nMaxAnisotropy: %d\n", g_pHardwareConfig->Caps().m_nMaxAnisotropy );
	Warning( "m_MaxTextureWidth: %d\n", g_pHardwareConfig->Caps().m_MaxTextureWidth );
	Warning( "m_MaxTextureHeight: %d\n", g_pHardwareConfig->Caps().m_MaxTextureHeight );
	Warning( "m_MaxTextureAspectRatio: %d\n", g_pHardwareConfig->Caps().m_MaxTextureAspectRatio );
	Warning( "m_MaxPrimitiveCount: %u\n", g_pHardwareConfig->Caps().m_MaxPrimitiveCount );
	Warning( "m_ZBiasAndSlopeScaledDepthBiasSupported: %s\n", g_pHardwareConfig->Caps().m_ZBiasAndSlopeScaledDepthBiasSupported ? "yes" : "no" );
	Warning( "m_SupportsMipmapping: %s\n", g_pHardwareConfig->Caps().m_SupportsMipmapping ? "yes" : "no" );
	Warning( "m_SupportsOverbright: %s\n", g_pHardwareConfig->Caps().m_SupportsOverbright ? "yes" : "no" );
	Warning( "m_SupportsCubeMaps: %s\n", g_pHardwareConfig->Caps().m_SupportsCubeMaps ? "yes" : "no" );
	Warning( "m_NumPixelShaderConstants: %d\n", g_pHardwareConfig->Caps().m_NumPixelShaderConstants );
	Warning( "m_NumVertexShaderConstants: %d\n", g_pHardwareConfig->Caps().m_NumVertexShaderConstants );
	Warning( "m_NumBooleanVertexShaderConstants: %d\n", g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants );
	Warning( "m_NumIntegerVertexShaderConstants: %d\n", g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants );
	Warning( "m_TextureMemorySize: %zu\n", g_pHardwareConfig->Caps().m_TextureMemorySize );
	Warning( "m_MaxNumLights: %d\n", g_pHardwareConfig->Caps().m_MaxNumLights );
	Warning( "m_SupportsHardwareLighting: %s\n", g_pHardwareConfig->Caps().m_SupportsHardwareLighting ? "yes" : "no" );
	Warning( "m_MaxBlendMatrices: %d\n", g_pHardwareConfig->Caps().m_MaxBlendMatrices );
	Warning( "m_MaxBlendMatrixIndices: %d\n", g_pHardwareConfig->Caps().m_MaxBlendMatrixIndices );
	Warning( "m_MaxVertexShaderBlendMatrices: %d\n", g_pHardwareConfig->Caps().m_MaxVertexShaderBlendMatrices );
	Warning( "m_SupportsMipmappedCubemaps: %s\n", g_pHardwareConfig->Caps().m_SupportsMipmappedCubemaps ? "yes" : "no" );
	Warning( "m_SupportsNonPow2Textures: %s\n", g_pHardwareConfig->Caps().m_SupportsNonPow2Textures ? "yes" : "no" );
	Warning( "m_nDXSupportLevel: %d\n", g_pHardwareConfig->Caps().m_nDXSupportLevel );
	Warning( "m_PreferDynamicTextures: %s\n", g_pHardwareConfig->Caps().m_PreferDynamicTextures ? "yes" : "no" );
	Warning( "m_HasProjectedBumpEnv: %s\n", g_pHardwareConfig->Caps().m_HasProjectedBumpEnv ? "yes" : "no" );
	Warning( "m_MaxUserClipPlanes: %d\n", g_pHardwareConfig->Caps().m_MaxUserClipPlanes );
	Warning( "m_SupportsSRGB: %s\n", g_pHardwareConfig->Caps().m_SupportsSRGB ? "yes" : "no" );
	switch( g_pHardwareConfig->Caps().m_HDRType )
	{
	case HDR_TYPE_NONE:
		Warning( "m_HDRType: HDR_TYPE_NONE\n" );
		break;
	case HDR_TYPE_INTEGER:
		Warning( "m_HDRType: HDR_TYPE_INTEGER\n" );
		break;
	case HDR_TYPE_FLOAT:
		Warning( "m_HDRType: HDR_TYPE_FLOAT\n" );
		break;
	default:
		Assert( 0 );
		break;
	}
	Warning( "m_bSupportsSpheremapping: %s\n", g_pHardwareConfig->Caps().m_bSupportsSpheremapping ? "yes" : "no" );
	Warning( "m_UseFastClipping: %s\n", g_pHardwareConfig->Caps().m_UseFastClipping ? "yes" : "no" );
	Warning( "m_pShaderDLL: %s\n", g_pHardwareConfig->Caps().m_pShaderDLL );
	Warning( "m_bNeedsATICentroidHack: %s\n", g_pHardwareConfig->Caps().m_bNeedsATICentroidHack ? "yes" : "no" );
	Warning( "m_bDisableShaderOptimizations: %s\n", g_pHardwareConfig->Caps().m_bDisableShaderOptimizations ? "yes" : "no" );
	Warning( "m_bColorOnSecondStream: %s\n", g_pHardwareConfig->Caps().m_bColorOnSecondStream ? "yes" : "no" );
	Warning( "m_MaxSimultaneousRenderTargets: %d\n", g_pHardwareConfig->Caps().m_MaxSimultaneousRenderTargets );
}


//-----------------------------------------------------------------------------
// Back buffer information
//-----------------------------------------------------------------------------
ImageFormat CShaderDeviceDx8::GetBackBufferFormat() const
{
	return ImageLoader::D3DFormatToImageFormat( m_PresentParameters.BackBufferFormat );
}

void CShaderDeviceDx8::GetBackBufferDimensions( int& width, int& height ) const
{
	width = m_PresentParameters.BackBufferWidth;
	height = m_PresentParameters.BackBufferHeight;
}


//-----------------------------------------------------------------------------
// Detects support for CreateQuery
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::DetectQuerySupport( IDirect3DDevice9 *pD3DDevice )
{
	// Do I need to detect whether this device supports CreateQuery before creating it?
	if ( m_DeviceSupportsCreateQuery != -1 )
		return;

	se::win::com::com_ptr<IDirect3DQuery9> pQueryObject;
	// Detect whether query is supported by creating and releasing:
	HRESULT hr = pD3DDevice->CreateQuery( D3DQUERYTYPE_EVENT, &pQueryObject );
	m_DeviceSupportsCreateQuery = SUCCEEDED(hr) ? 1 : 0;
}


//-----------------------------------------------------------------------------
// Actually creates the D3D Device once the present parameters are set up
//-----------------------------------------------------------------------------
se::win::com::com_ptr<IDirect3DDevice9Ex> CShaderDeviceDx8::InvokeCreateDevice( void* hWnd, unsigned nAdapter, DWORD deviceCreationFlags )
{
#if !defined(NVPERFHUD)
	D3DDEVTYPE devType{DX8_DEVTYPE};
#else 
	D3DDEVTYPE devType{D3DDEVTYPE_REF};

	nAdapter = D3D()->GetAdapterCount()-1;
	deviceCreationFlags = D3DCREATE_FPU_PRESERVE | D3DCREATE_HARDWARE_VERTEXPROCESSING;
#endif

#if defined(ENABLE_NULLREF_DEVICE_SUPPORT)
	devType = CommandLine()->FindParm("-nulldevice") ? D3DDEVTYPE_NULLREF : devType;
#endif

	// Create the device with multi-threaded safeguards if we're using mat_queue_mode 2.
	// The logic to enable multithreaded rendering happens well after the device has been created,
	// so we replicate some of that logic here.
	static ConVarRef mat_queue_mode( "mat_queue_mode" );
	const uint8_t physicalCpuCoresCount = GetCPUInformation()->m_nPhysicalProcessors;
	if ( mat_queue_mode.GetInt() == 2 ||
		 ( mat_queue_mode.GetInt() == -2 && physicalCpuCoresCount >= 2 ) ||
	     ( mat_queue_mode.GetInt() == -1 && physicalCpuCoresCount >= 2 ) )
	{
		deviceCreationFlags |= D3DCREATE_MULTITHREADED;
	}
	
	// dimhotepus: For fullscreen display mode we must initialize one. Should be nullptr when windowed.
	// See https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3d9ex-createdeviceex
	D3DDISPLAYMODEEX fullScreenDisplayMode = GetFullScreenDisplayModeFromPresentParameters( m_PresentParameters );
	D3DDISPLAYMODEEX *pFullScreenDisplayMode = m_PresentParameters.Windowed ? nullptr : &fullScreenDisplayMode;

	se::win::com::com_ptr<IDirect3DDevice9Ex> d3d9_ex_device;
	HRESULT hr = D3D()->CreateDeviceEx( nAdapter, devType,
		(VD3DHWND)hWnd, deviceCreationFlags, &m_PresentParameters, pFullScreenDisplayMode, &d3d9_ex_device );
	if ( SUCCEEDED( hr ) )
		return d3d9_ex_device;

	// Try again, other applications may be taking their time.
	ThreadSleep( 1000 );

	hr = D3D()->CreateDeviceEx( nAdapter, devType,
		(VD3DHWND)hWnd, deviceCreationFlags, &m_PresentParameters, pFullScreenDisplayMode, &d3d9_ex_device );
	if ( SUCCEEDED( hr ) )
		return d3d9_ex_device;

	const char *more_info = nullptr;
	switch ( hr )
	{
#ifdef _WIN32
	case E_OUTOFMEMORY:
		more_info = "E_OUTOFMEMORY: Out of RAM when create device. Close existing RAM consuming apps and retry.";
		break;
	case D3DERR_OUTOFVIDEOMEMORY:
		more_info = "D3DERR_OUTOFVIDEOMEMORY: Out of GPU memory when create device. Close existing GPU consuming apps and retry.";
		break;
#endif // _WIN32
	}

	// Otherwise we failed, show a message and shutdown.
	if ( more_info )
	{
		DWarning( "init",
			0,
			"IDirect3D9Ex::CreateDeviceEx(adapter = %u, device type = 0x%x, window = 0x%p, flags = 0x%x,"
				" parameters = (width = %u, height = %u, back buffer format = 0x%x, back buffer count = %u,"
				" multisample type = %u, multisample quality = %u, swap effect = 0x%x, window = 0x%p,"
				" windowed = %d, enable auto depth stencil = %d, auto depth stencil format = 0x%x,"
				" flags = 0x%x, full screen refresh rate = %u, presentation interval = %u)"
			") failed to create %s device!\n\nError %s: %s\n\nPlease see the following for more info:\n"
			"https://help.steampowered.com/en/faqs/view/102E-D170-B891-7145\n",
			nAdapter,
			devType,
			hWnd,
			deviceCreationFlags,
			m_PresentParameters.BackBufferWidth,
			m_PresentParameters.BackBufferHeight,
			m_PresentParameters.BackBufferFormat,
			m_PresentParameters.BackBufferCount,
			m_PresentParameters.MultiSampleType,
			m_PresentParameters.MultiSampleQuality,
			m_PresentParameters.SwapEffect,
			m_PresentParameters.hDeviceWindow,
			m_PresentParameters.Windowed,
			m_PresentParameters.EnableAutoDepthStencil,
			m_PresentParameters.AutoDepthStencilFormat,
			m_PresentParameters.Flags,
			m_PresentParameters.FullScreen_RefreshRateInHz,
			m_PresentParameters.PresentationInterval,
			IsOpenGL() ? "OpenGL" : "Direct3D 9Ex",
			se::win::com::com_error_category().message(hr).c_str(),
			more_info );
	}
	else
	{
		DWarning( "init",
			0,
			"IDirect3D9Ex::CreateDeviceEx(adapter = %u, device type = 0x%x, window = 0x%p, flags = 0x%x,"
				" parameters = (width = %u, height = %u, back buffer format = 0x%x, back buffer count = %u,"
				" multisample type = %u, multisample quality = %u, swap effect = 0x%x, window = 0x%p,"
				" windowed = %d, enable auto depth stencil = %d, auto depth stencil format = 0x%x,"
				" flags = 0x%x, full screen refresh rate = %u, presentation interval = %u)"
			") failed to create %s device!\n\nError %s\n\nPlease see the following for more info:\n"
			"https://help.steampowered.com/en/faqs/view/102E-D170-B891-7145\n",
			nAdapter,
			devType,
			hWnd,
			deviceCreationFlags,
			m_PresentParameters.BackBufferWidth,
			m_PresentParameters.BackBufferHeight,
			m_PresentParameters.BackBufferFormat,
			m_PresentParameters.BackBufferCount,
			m_PresentParameters.MultiSampleType,
			m_PresentParameters.MultiSampleQuality,
			m_PresentParameters.SwapEffect,
			m_PresentParameters.hDeviceWindow,
			m_PresentParameters.Windowed,
			m_PresentParameters.EnableAutoDepthStencil,
			m_PresentParameters.AutoDepthStencilFormat,
			m_PresentParameters.Flags,
			m_PresentParameters.FullScreen_RefreshRateInHz,
			m_PresentParameters.PresentationInterval,
			IsOpenGL() ? "OpenGL" : "Direct3D 9Ex",
			se::win::com::com_error_category().message(hr).c_str() );
	}

	return {};
}


//-----------------------------------------------------------------------------
// Creates the D3D Device
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::CreateD3DDevice( void* pHWnd, unsigned nAdapter, const ShaderDeviceInfo_t &info )
{
	Assert( info.m_nVersion == SHADER_DEVICE_INFO_VERSION );

	MEM_ALLOC_CREDIT_( __FILE__ ": D3D Device" );

	VD3DHWND hWnd = (VD3DHWND)pHWnd;

#if ( !defined( PIX_INSTRUMENTATION ) && !defined( _X360 ) && !defined( NVPERFHUD ) )
	D3DPERF_SetOptions(1);	// Explicitly disallow PIX instrumented profiling in external builds
#endif

	// Get some caps....
	D3DCAPS caps;
	HRESULT hr = D3D()->GetDeviceCaps( nAdapter, DX8_DEVTYPE, &caps );
	if ( FAILED( hr ) )
	{
		Assert(false);
		Warning( "IDirect3D9Ex::GetDeviceCaps(adapter = %u, device type = 0x%x) failed w/e %s.\n",
			m_nAdapter,
			DX8_DEVTYPE, 
			se::win::com::com_error_category().message(hr).c_str() );
		return false;
	}

	// Determine the adapter format
	ShaderDisplayMode_t mode;
	g_pShaderDeviceMgrDx8->GetCurrentModeInfo( &mode, nAdapter );
	m_AdapterFormat = mode.m_Format;

	// FIXME: Need to do this prior to SetPresentParameters. Fix.
	// Make it part of HardwareCaps_t
	InitializeColorInformation( nAdapter, DX8_DEVTYPE, m_AdapterFormat );

	const HardwareCaps_t &adapterCaps = g_ShaderDeviceMgrDx8.GetHardwareCaps( nAdapter );
	DWORD deviceCreationFlags = ComputeDeviceCreationFlags( caps, adapterCaps.m_bSoftwareVertexProcessing );
	SetPresentParameters( hWnd, nAdapter, info );

	// Tell all other instances of the material system to let go of memory
	SendIPCMessage( RELEASE_MESSAGE );

	// Creates the device
	m_d3d9_device_ex = std::move( InvokeCreateDevice( pHWnd, nAdapter, deviceCreationFlags ) );
	if ( !m_d3d9_device_ex )
		return false;

#ifdef DEBUG
	m_createDeviceThreadId = ThreadGetCurrentId();
#endif

	// Check to see if query is supported
	DetectQuerySupport( m_d3d9_device_ex );

#ifdef STUBD3D
	m_d3d9_device_ex = new CStubD3DDevice( d3d9ex_device, g_pFullFileSystem );
#endif

	// Tell all other instances of the material system it's ok to grab memory
	SendIPCMessage( REACQUIRE_MESSAGE );

	m_hWnd = pHWnd;
	m_nAdapter = m_DisplayAdapter = nAdapter;
	m_DeviceState = DEVICE_STATE_OK;
	m_bQueuedDeviceLost = false;
	m_bQueuedDeviceHung = false;
	m_bQueuedDeviceOutOfGpuMemory = false;
	m_bRenderingOccluded = false;

	m_IsResizing = info.m_bWindowed && info.m_bResizing;

	// This is our current view.
	m_ViewHWnd = hWnd;
	GetWindowSize( m_nWindowWidth, m_nWindowHeight );

	g_pHardwareConfig->SetupHardwareCaps( info, g_ShaderDeviceMgrDx8.GetHardwareCaps( nAdapter ) );

	// FIXME: Bake this into hardware config
	// What texture formats do we support?
	if ( D3DSupportsCompressedTextures() )
	{
		g_pHardwareConfig->ActualCapsForEdit().m_SupportsCompressedTextures = COMPRESSED_TEXTURES_ON;
		g_pHardwareConfig->CapsForEdit().m_SupportsCompressedTextures = COMPRESSED_TEXTURES_ON;
	}
	else
	{
		g_pHardwareConfig->ActualCapsForEdit().m_SupportsCompressedTextures = COMPRESSED_TEXTURES_OFF;
		g_pHardwareConfig->CapsForEdit().m_SupportsCompressedTextures = COMPRESSED_TEXTURES_OFF;
	}

	return ( !FAILED( hr ) );
}


//-----------------------------------------------------------------------------
// Frame sync
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::AllocFrameSyncTextureObject()
{
	FreeFrameSyncTextureObject();

	// Create a tiny managed texture.
	const HRESULT hr = D3D9Device()->CreateTexture( 
		1, 1,	// width, height
		0,		// levels
		D3DUSAGE_DYNAMIC,	// usage
		D3DFMT_A8R8G8B8,	// format
		D3DPOOL_DEFAULT,
		&m_pFrameSyncTexture,
		NULL );
	if ( FAILED( hr ) )
	{
		Assert( false );
		Warning( __FUNCTION__ ": IDirect3DDevice9Ex::CreateTexture(width = 1, height = 1, levels = 0, usage = D3DUSAGE_DYNAMIC, format = D3DFMT_A8R8G8B8, pool = D3DPOOL_DEFAULT) failed w/e %s.\n",
			 se::win::com::com_error_category().message(hr).c_str() );
	}
}

void CShaderDeviceDx8::FreeFrameSyncTextureObject()
{
	if ( m_pFrameSyncTexture )
	{
		m_pFrameSyncTexture.Release();
	}
}

void CShaderDeviceDx8::AllocFrameSyncObjects( void )
{
	if ( mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: CShaderAPIDX8::AllocFrameSyncObjects\n" );
	}

	// Allocate the texture for frame syncing in case we force that to be on.
	AllocFrameSyncTextureObject();

	if ( m_DeviceSupportsCreateQuery == 0 )
	{
		for ( size_t i = 0; i < std::size(m_pFrameSyncQueryObject); i++ )
		{
			if ( m_pFrameSyncQueryObject[i] )
			{
				m_pFrameSyncQueryObject[i].Release();
			}
			m_bQueryIssued[i] = false;
		}
		return;
	}

	// FIXME FIXME FIXME!!!!!  Need to record this.
	for ( size_t i = 0; i < std::size(m_pFrameSyncQueryObject); i++ )
	{
		const HRESULT hr = D3D9Device()->CreateQuery( D3DQUERYTYPE_EVENT, &m_pFrameSyncQueryObject[i] );
		if ( hr == D3DERR_NOTAVAILABLE )
		{
			Warning( "D3DQUERYTYPE_EVENT not available on GPU driver. It means asynchronous events are not supported.\n" );
			Assert( !m_pFrameSyncQueryObject[i] );
		}
		else if ( hr == E_OUTOFMEMORY )
		{
			Warning( __FUNCTION__ ": IDirect3DDevice9Ex::CreateQuery(type = D3DQUERYTYPE_EVENT) returns E_OUTOFMEMORY. Please stop other apps consuming GPU VRAM.\n" );
			Assert( !m_pFrameSyncQueryObject[i] );
		}
		else
		{
			Assert( SUCCEEDED( hr ) );
			Assert( m_pFrameSyncQueryObject[i] );
			m_pFrameSyncQueryObject[i]->Issue( D3DISSUE_END );
			m_bQueryIssued[i] = true;
		}
	}
}

void CShaderDeviceDx8::FreeFrameSyncObjects( void )
{
	if ( mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: CShaderAPIDX8::FreeFrameSyncObjects\n" );
	}

	FreeFrameSyncTextureObject();

	// FIXME FIXME FIXME!!!!!  Need to record this.
	for ( size_t i = 0; i < std::size(m_pFrameSyncQueryObject); i++ )
	{
		if ( m_pFrameSyncQueryObject[i] )
		{
			if ( m_bQueryIssued[i] )
			{
				tmZone( TELEMETRY_LEVEL1, TMZF_NONE, "D3DQueryGetData %t", tmSendCallStack( TELEMETRY_LEVEL0, 0 ) );

				double flStartTime = Plat_FloatTime();
				BOOL dummyData = 0;
				HRESULT hr = S_OK;

				// Make every attempt (within 2 seconds) to get the result from the query.  Doing so may prevent
				// crashes in the driver if we try to release outstanding queries.
				do
				{
					hr = m_pFrameSyncQueryObject[i]->GetData( &dummyData, sizeof( dummyData ), D3DGETDATA_FLUSH );
					double flCurrTime = Plat_FloatTime();

					// don't wait more than 2 seconds for these
					if ( flCurrTime - flStartTime > 2.00 )
						break;
				} while ( hr == S_FALSE );

				// dimhotepus: Handle lost device case.
				if ( hr == D3DERR_DEVICELOST )
				{
					MarkDeviceLost( );
				}
			}
			m_pFrameSyncQueryObject[i].Release();
			m_bQueryIssued[i] = false;
		}
	}
}


//-----------------------------------------------------------------------------
// Occurs when another application is initializing
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::OtherAppInitializing( bool initializing )
{
	if ( !ThreadOwnsDevice() || !ThreadInMainThread() )
	{
		if ( initializing )
		{
			ShaderUtil()->OnThreadEvent( SHADER_THREAD_OTHER_APP_START );
		}
		else
		{
			ShaderUtil()->OnThreadEvent( SHADER_THREAD_OTHER_APP_END );
		}
		return;
	}
	Assert( m_bOtherAppInitializing != initializing );

	if ( !IsDeactivated() )
	{
		const HRESULT hr = D3D9Device()->EndScene();
		if (FAILED(hr))
		{
			Assert(false);
			Warning( __FUNCTION__ ": IDirect3DDevice9Ex::EndScene failed w/e %s.\n",
				se::win::com::com_error_category().message(hr).c_str() );
		}
	}

	// NOTE: OtherApp is set in this way because we need to know we're
	// active as we release and restore everything
	CheckDeviceState( initializing );

	if ( !IsDeactivated() )
	{
		const HRESULT hr = D3D9Device()->BeginScene();
		if (FAILED(hr))
		{
			Assert(false);
			Warning( __FUNCTION__ ": IDirect3DDevice9Ex::BeginScene failed w/e %s.\n",
				se::win::com::com_error_category().message(hr).c_str() );
		}
	}
}


void CShaderDeviceDx8::HandleThreadEvent( uint32 threadEvent )
{
	Assert(ThreadOwnsDevice());
	switch ( threadEvent )
	{
	case SHADER_THREAD_OTHER_APP_START:
		OtherAppInitializing(true);
		break;
	case SHADER_THREAD_RELEASE_RESOURCES:
		ReleaseResources();
		break;
	case SHADER_THREAD_EVICT_RESOURCES:
		EvictManagedResourcesInternal();
		break;
	case SHADER_THREAD_RESET_RENDER_STATE:
		ResetRenderState();
		break;
	case SHADER_THREAD_ACQUIRE_RESOURCES:
		ReacquireResources();
		break;
	case SHADER_THREAD_OTHER_APP_END:
		OtherAppInitializing(false);
		break;
	}
}

//-----------------------------------------------------------------------------
// We lost the device, but we have a chance to recover
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::TryDeviceReset( DeviceState_t deviceState )
{
	// Don't try to reset the device until we're sure our resources have been released
	if ( !m_bResourcesReleased )
	{
		return false;
	}

	AssertMsg( m_createDeviceThreadId == ThreadGetCurrentId(),
		"IDirect3DDevice9Ex::Reset can be perfomed only on same thread which created device." );

	// FIXME: Make this rebuild the Dx9Device from scratch!
	// Helps with compatibility
	const HRESULT hr{ D3D9Device()->Reset( &m_PresentParameters ) };
	bool bResetSuccess = SUCCEEDED( hr );
	if ( !bResetSuccess )
	{
		Warning( __FUNCTION__ ": IDirect3DDevice9Ex::Reset failed w/e %s.\n",
			se::win::com::com_error_category().message(hr).c_str() );
	}

#if defined(IS_WINDOWS_PC) && defined(SHADERAPIDX9)
	if ( bResetSuccess )
	{
		// Possible return values include:
		// D3D_OK,
		// D3DERR_DEVICELOST,
		// D3DERR_DEVICEHUNG,
		// D3DERR_DEVICEREMOVED,
		// D3DERR_OUTOFVIDEOMEMORY
		// S_PRESENT_MODE_CHANGED or S_PRESENT_OCCLUDED
		bResetSuccess = SUCCEEDED( D3D9Device()->CheckDeviceState(static_cast<HWND>(m_hWnd)) );
		if ( bResetSuccess )
		{
			Warning("%s. Device has been reset, re-uploading resources now.\n",
				GetDeviceStateDescription( deviceState ) );
		}
	}
#endif

	if ( bResetSuccess )
		m_bResourcesReleased = false;

	return bResetSuccess;
}


//-----------------------------------------------------------------------------
// Release, reacquire resources
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::ReleaseResources()
{
	if ( !ThreadOwnsDevice() || !ThreadInMainThread() )
	{
		// Set our resources as not being released yet.  
		// We reset this in two places since release resources can be called without a call to TryDeviceReset.
		m_bResourcesReleased = false;
		ShaderUtil()->OnThreadEvent( SHADER_THREAD_RELEASE_RESOURCES );
		return;
	}

	// Only the initial "ReleaseResources" actually has effect
	if ( m_numReleaseResourcesRefCount ++ != 0 )
	{
		Warning( "ReleaseResources has no effect, now at level %d.\n", m_numReleaseResourcesRefCount );
		DevWarning( "ReleaseResources called twice is a bug: use IsDeactivated to check for a valid device.\n" );
		Assert( 0 );
		return;
	}

	LOCK_SHADERAPI();
	CPixEvent( PIX_VALVE_ORANGE, "ReleaseResources" );

	FreeFrameSyncObjects();
	ShaderUtil()->ReleaseShaderObjects();
	MeshMgr()->ReleaseBuffers();
	g_pShaderAPI->ReleaseShaderObjects();

#ifdef _DEBUG
	for( int i = 0; i < MeshMgr()->BufferCount(); i++ )
	{
	}
#endif

	// All meshes cleaned up?
	Assert( MeshMgr()->BufferCount() == 0 );

	// Signal that our resources have been released so that we can try to reset the device
	m_bResourcesReleased = true;
}


void CShaderDeviceDx8::ReacquireResources()
{
	ReacquireResourcesInternal();
}

void CShaderDeviceDx8::ReacquireResourcesInternal( bool bResetState, bool bForceReacquire, char const *pszForceReason )
{
	if ( !ThreadOwnsDevice() || !ThreadInMainThread() )
	{
		if ( bResetState )
		{
			ShaderUtil()->OnThreadEvent( SHADER_THREAD_RESET_RENDER_STATE );
		}
		ShaderUtil()->OnThreadEvent( SHADER_THREAD_ACQUIRE_RESOURCES );
		return;
	}
	if ( bForceReacquire )
	{
		// If we are forcing reacquire then warn if release calls are remaining unpaired
		if ( m_numReleaseResourcesRefCount > 1 )
		{
			Warning( "Forcefully resetting device (%s), resources release level was %d.\n", pszForceReason ? pszForceReason : "unspecified", m_numReleaseResourcesRefCount );
			Assert( 0 );
		}
		m_numReleaseResourcesRefCount = 0;
	}
	else
	{
		// Only the final "ReacquireResources" actually has effect
		if ( -- m_numReleaseResourcesRefCount != 0 )
		{
			Warning( "ReacquireResources has no effect, now at level %d.\n", m_numReleaseResourcesRefCount );
			DevWarning( "ReacquireResources being discarded is a bug: use IsDeactivated to check for a valid device.\n" );
			Assert( 0 );

			if ( m_numReleaseResourcesRefCount < 0 )
			{
				m_numReleaseResourcesRefCount = 0;
			}

			return;
		}
	}

	if ( bResetState )
	{
		ResetRenderState();
	}

	LOCK_SHADERAPI();
	CPixEvent event( PIX_VALVE_ORANGE, "ReacquireResources" );

	g_pShaderAPI->RestoreShaderObjects();
	AllocFrameSyncObjects();
	MeshMgr()->RestoreBuffers();
	ShaderUtil()->RestoreShaderObjects( CShaderDeviceMgrBase::ShaderInterfaceFactory );
}


//-----------------------------------------------------------------------------
// Changes the window size
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::ResizeWindow( const ShaderDeviceInfo_t &info ) 
{
	m_bPendingVideoModeChange = false;

	// We don't need to do crap if the window was set up to set up
	// to be resizing...
	if ( info.m_bResizing )
		return false;

	g_pShaderDeviceMgr->InvokeModeChangeCallbacks();

	ReleaseResources();
	SetPresentParameters( m_hWnd, m_DisplayAdapter, info );

	AssertMsg( m_createDeviceThreadId == ThreadGetCurrentId(),
		"IDirect3DDevice9Ex::Reset can be perfomed only on same thread which created device." );

	const HRESULT hr{ D3D9Device()->Reset( &m_PresentParameters ) };
	if ( SUCCEEDED( hr ) )
	{
		ReacquireResourcesInternal( true, true, "ResizeWindow" );
		return true;
	}

	Warning( __FUNCTION__ ": IDirect3DDevice9Ex::Reset failed w/e %s.\n",
			se::win::com::com_error_category().message(hr).c_str() );
	return false;
}


bool CShaderDeviceDx8::IsPresentOccluded()
{
	if ( m_bRenderingOccluded )
	{
		// Occluded applications can continue rendering and all calls will succeed,
		// but the occluded presentation window will not be updated.  Preferably
		// the application should stop rendering to the presentation window using
		// the device and keep calling CheckDeviceState until S_OK or
		// S_PRESENT_MODE_CHANGED returns.
		//
		// See https://learn.microsoft.com/en-us/windows/win32/direct3d9/device-state-return-codes
		//
		// Possible return values include:
		// D3D_OK,
		// D3DERR_DEVICELOST,
		// D3DERR_DEVICEHUNG,
		// D3DERR_DEVICEREMOVED,
		// D3DERR_OUTOFVIDEOMEMORY
		// S_PRESENT_MODE_CHANGED or S_PRESENT_OCCLUDED
		const HRESULT hr = D3D9Device()->CheckDeviceState(static_cast<HWND>(m_hWnd));
		if ( hr == S_PRESENT_OCCLUDED )
		{
			return true;
		}
		
		// Window not occluded anymore (either S_OK, S_PRESENT_MODE_CHANGED or D3DERR_*).
		m_bRenderingOccluded = false;

		DMsg( "render", 0, "Window is not occluded anymore. Restore rendering.\n" );

		// We continue rendering here, so need to start scene as PresentEx was set m_bRenderingOccluded == true
		// and skipped start of new scene at previous frame.
		EndPresent();

		// D3DERR_DEVICELOST, D3DERR_DEVICEHUNG and D3DERR_OUTOFVIDEOMEMORY checked later by CheckDeviceState & PresentEx.
	}

	return false;
}


void CShaderDeviceDx8::MarkPresentOccluded()
{
	m_bRenderingOccluded = true;

	DMsg( "render", 0, "Window is occluded. Skipping rendering to save power...\n" );
}


void CShaderDeviceDx8::BeginPresent()
{
	if ( !IsDeactivated() )
	{
		if ( const HRESULT hr = D3D9Device()->EndScene(); FAILED(hr) )
		{
			Assert(false);
			Warning( __FUNCTION__ ": IDirect3DDevice9Ex::EndScene failed w/e %s.\n",
				se::win::com::com_error_category().message(hr).c_str() );
		}
	}
}


void CShaderDeviceDx8::EndPresent()
{
	if ( !IsDeactivated() )
	{
#ifndef DX_TO_GL_ABSTRACTION
		if ( ( ShaderUtil()->GetConfig().bMeasureFillRate || ShaderUtil()->GetConfig().bVisualizeFillRate ) )
		{
			g_pShaderAPI->ClearBuffers( true, true, true, -1, -1 );
		}
#endif

		if ( const HRESULT hr = D3D9Device()->BeginScene(); FAILED(hr) )
		{
			Assert(false);
			Warning( __FUNCTION__ ": IDirect3DDevice9Ex::BeginScene failed w/e %s.\n",
				se::win::com::com_error_category().message(hr).c_str() );
		}
	}
}


//-----------------------------------------------------------------------------
// Queue up the fact that the device was lost
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::MarkDeviceLost( )
{
	m_bQueuedDeviceLost = true;
}


//-----------------------------------------------------------------------------
// Queue up the fact that the device was hung
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::MarkDeviceHung( )
{
	m_bQueuedDeviceHung = true;
}


//-----------------------------------------------------------------------------
// Queue up the fact that the device was out of GPU memory.
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::MarkDeviceOutOfGpuMemory()
{
	m_bQueuedDeviceOutOfGpuMemory = true;
}


//-----------------------------------------------------------------------------
// Checks if the device was lost
//-----------------------------------------------------------------------------
ConVar mat_forcelostdevice( "mat_forcelostdevice", "0" );

//-----------------------------------------------------------------------------
// dimhotepus: Checks if the device was hung
//-----------------------------------------------------------------------------
ConVar mat_forcehungdevice( "mat_forcehungdevice", "0" );

//-----------------------------------------------------------------------------
// dimhotepus: Checks if the device was out of GPU memory
//-----------------------------------------------------------------------------
ConVar mat_forceoutofgpumemorydevice("mat_forceoutofgpumemorydevice", "0");

void CShaderDeviceDx8::CheckDeviceState( bool bOtherAppInitializing )
{
	m_bOtherAppInitializing = bOtherAppInitializing;

#ifdef _DEBUG
	if ( mat_forcelostdevice.GetBool() )
	{
		mat_forcelostdevice.SetValue( 0 );
		MarkDeviceLost();
	}

	if ( mat_forcehungdevice.GetBool() )
	{
		mat_forcehungdevice.SetValue( 0 );
		MarkDeviceHung();
	}

	if (mat_forceoutofgpumemorydevice.GetBool())
	{
		mat_forceoutofgpumemorydevice.SetValue( 0 );
		MarkDeviceOutOfGpuMemory();
	}
#endif
	
	HRESULT hr = D3D_OK;

#if defined(IS_WINDOWS_PC) && defined(SHADERAPIDX9)
	if ( m_DeviceState == DEVICE_STATE_OK )
	{
		// Steady state - PresentEx return value will mark us lost, hung or out of GPU memory if necessary.
	}
	else
#endif
	{
		RECORD_COMMAND( DX8_CHECK_DEVICE_STATE, 0 );
		// Possible return values include:
		// D3D_OK,
		// D3DERR_DEVICELOST,
		// D3DERR_DEVICEHUNG,
		// D3DERR_DEVICEREMOVED,
		// D3DERR_OUTOFVIDEOMEMORY
		// S_PRESENT_MODE_CHANGED or S_PRESENT_OCCLUDED
		hr = D3D9Device()->CheckDeviceState(static_cast<HWND>(m_hWnd));
	}

	DeviceState_t newDeviceState = DEVICE_STATE_OK;

	// If some other call returned device lost previously in the frame, spoof the return value from CDS
	if ( m_bQueuedDeviceLost )
	{
		hr = FAILED( hr ) ? hr : D3DERR_DEVICENOTRESET;
		m_bQueuedDeviceLost = false;

		newDeviceState = DEVICE_STATE_LOST_DEVICE;
	}
	else if ( hr == D3DERR_DEVICELOST )
	{
		newDeviceState = DEVICE_STATE_LOST_DEVICE;
	}

	// If some other call returned device hung previously in the frame, spoof the return value from CDS
	if ( m_bQueuedDeviceHung )
	{
		hr = FAILED( hr ) ? hr : D3DERR_DEVICENOTRESET;
		m_bQueuedDeviceHung = false;

		newDeviceState = DEVICE_STATE_HUNG_DEVICE;
	}
	else if ( hr == D3DERR_DEVICEHUNG )
	{
		newDeviceState = DEVICE_STATE_HUNG_DEVICE;
	}

	// If some other call returned device out of GPU memory previously in the frame, spoof the return value from CDS
	if ( m_bQueuedDeviceOutOfGpuMemory )
	{
		hr = FAILED( hr ) ? hr : D3DERR_DEVICENOTRESET;
		m_bQueuedDeviceOutOfGpuMemory = false;

		newDeviceState = DEVICE_STATE_OUT_OF_GPU_MEMORY;
	}
	else if ( hr == D3DERR_OUTOFVIDEOMEMORY )
	{
		newDeviceState = DEVICE_STATE_OUT_OF_GPU_MEMORY;
	}

	if ( newDeviceState == DEVICE_STATE_OUT_OF_GPU_MEMORY )
	{
		// Don't have the memory for this.  Try flushing all managed resources out of vid mem and try again.
		// Managed resources here including POOL_MANAGED (not used since Direct3D 9Ex) and driver internal ones (still used in Direct3D 9Ex).
		const HRESULT hr2 = D3D9Device()->EvictManagedResources();
		if ( FAILED( hr2 ) )
		{
			Warning( __FUNCTION__ ": IDirect3DDevice9Ex::EvictManagedResources() failed w/e %s.\n",
				se::win::com::com_error_category().message(hr2).c_str() );
		}
	}

	if ( m_DeviceState == DEVICE_STATE_OK )
	{
		// We can transition out of ok if bOtherAppInitializing is set
		// or if we become minimized, or if CDS returns D3DERR_DEVICELOST, D3DERR_DEVICEHUNG, D3DERR_OUTOFVIDEOMEMORY or D3DERR_DEVICENOTRESET.
		if ( hr == D3DERR_DEVICEHUNG ||
			 hr == D3DERR_OUTOFVIDEOMEMORY ||
			 hr == D3DERR_DEVICELOST ||
			 hr == D3DERR_DEVICENOTRESET )
		{
			// purge unreferenced materials
			g_pShaderUtil->UncacheUnusedMaterials( true );

			// We were ok, now we're not. Release resources
			ReleaseResources();
			m_DeviceState = newDeviceState;
		}
		else if ( bOtherAppInitializing )
		{
			// purge unreferenced materials
			g_pShaderUtil->UncacheUnusedMaterials( true );

			// We were ok, now we're not. Release resources
			ReleaseResources();
			m_DeviceState = DEVICE_STATE_OTHER_APP_INIT; 
		}
	}

	// Immediately checking device lost / hung / out of GPU memory after ok helps in the case where
	// we got D3DERR_DEVICENOTRESET in which case we want to immediately try to switch out of DEVICE_STATE_LOST_DEVICE,
	// DEVICE_STATE_HUNG_DEVICE or DEVICE_STATE_OUT_OF_GPU_MEMORY and into DEVICE_STATE_NEEDS_RESET
	// Check DEVICE_STATE_LOST_DEVICE last as since Direct3D 9Ex devices are rarely lost.
	if ( m_DeviceState == DEVICE_STATE_HUNG_DEVICE ||
		 m_DeviceState == DEVICE_STATE_OUT_OF_GPU_MEMORY ||
		 m_DeviceState == DEVICE_STATE_LOST_DEVICE )
	{
		// We can only try to reset if we're not lost, hung or out of video memory.
		if ( hr != D3DERR_DEVICEHUNG &&
			 hr != D3DERR_OUTOFVIDEOMEMORY &&
			 hr != D3DERR_DEVICELOST )
		{
			m_DeviceState = DEVICE_STATE_NEEDS_RESET;
		}
	}

	// Immediately checking needs reset also helps for the case where we got D3DERR_DEVICENOTRESET
	if ( m_DeviceState == DEVICE_STATE_NEEDS_RESET )
	{
		if ( hr == D3DERR_DEVICEHUNG ||
			 hr == D3DERR_OUTOFVIDEOMEMORY ||
			 hr == D3DERR_DEVICELOST )
		{
			m_DeviceState = newDeviceState;
		}
		else
		{
			bool bResetSucceeded = TryDeviceReset( newDeviceState );
			if ( bResetSucceeded )
			{
				if ( !bOtherAppInitializing	)
				{
					m_DeviceState = DEVICE_STATE_OK;

					// We were bad, now we're ok. Restore resources and reset render state.
					ReacquireResourcesInternal( true, true, "NeedsReset" );
				}
				else
				{
					m_DeviceState = DEVICE_STATE_OTHER_APP_INIT;
				}
			}
		}
	}

	if ( m_DeviceState == DEVICE_STATE_OTHER_APP_INIT )
	{
		if ( hr == D3DERR_DEVICEHUNG ||
			 hr == D3DERR_OUTOFVIDEOMEMORY ||
			 hr == D3DERR_DEVICELOST )
		{
			m_DeviceState = newDeviceState;
		}
		else if ( !bOtherAppInitializing )
		{
			m_DeviceState = DEVICE_STATE_OK; 

			// We were bad, now we're ok. Restore resources and reset render state.
			ReacquireResourcesInternal( true, true, "OtherAppInit" );
		}
	}

	// Do mode change if we have a video mode change.
	if ( m_bPendingVideoModeChange && !IsDeactivated() )
	{
		const ShaderDisplayMode_t &newMode = m_PendingVideoModeChangeConfig.m_DisplayMode;
		DevMsg
		(
			"Video mode change detected [dx level %d, %d x %d, %s]\n",
			m_PendingVideoModeChangeConfig.m_nDXLevel,
			newMode.m_nWidth,
			newMode.m_nHeight,
			m_PendingVideoModeChangeConfig.m_bWindowed ? "window" : "fullscreen"
		);

		// now purge unreferenced materials
		g_pShaderUtil->UncacheUnusedMaterials( true );

		ResizeWindow( m_PendingVideoModeChangeConfig );
	}
}

const char *CShaderDeviceDx8::GetDeviceStateDescription( CShaderDeviceDx8::DeviceState_t state )
{
	switch ( state )
	{
		case DEVICE_STATE_OK:
			return "Device is ok";
		case DEVICE_STATE_OTHER_APP_INIT:
			return "Other app init";
		case DEVICE_STATE_LOST_DEVICE:
			return "Device is lost";
		case DEVICE_STATE_HUNG_DEVICE:
			return "Device is hung";
		case DEVICE_STATE_OUT_OF_GPU_MEMORY:
			return "Device is out of GPU memory";
		case DEVICE_STATE_NEEDS_RESET:
			return "Device needs reset";
		default:
			AssertMsg( false, "Unknown device state 0x%x.", state );
			return "Unknown device state";
	}
}

//-----------------------------------------------------------------------------
// Special method to refresh the screen on the XBox360
//-----------------------------------------------------------------------------
bool CShaderDeviceDx8::AllocNonInteractiveRefreshObjects()
{	
	return true;
}

void CShaderDeviceDx8::FreeNonInteractiveRefreshObjects()
{
}

void CShaderDeviceDx8::EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode, ShaderNonInteractiveInfo_t *pInfo )
{
}

void CShaderDeviceDx8::RefreshFrontBufferNonInteractive()
{
}


//-----------------------------------------------------------------------------
// Page flip
//-----------------------------------------------------------------------------
void CShaderDeviceDx8::Present()
{
	LOCK_SHADERAPI();

	// Handle window occlusion.
	if ( IsPresentOccluded() )
	{
		// Window is occluded, take system time to run.
		ThreadSleep( 30 );
		return;
	}

	// Need to flush the dynamic buffer
	g_pShaderAPI->FlushBufferedPrimitives();

	// End current scene.
	BeginPresent();

	// if we're in queued mode, don't present if the device is already lost
	bool bValidPresent = true;
	bool bInMainThread = ThreadInMainThread();
	if ( !bInMainThread )
	{
		// don't present if the device is in an invalid state and in queued mode
		if ( m_DeviceState != DEVICE_STATE_OK )
		{
			bValidPresent = false;
		}
		// check for lost device early in threaded mode
		CheckDeviceState( m_bOtherAppInitializing );
		if ( m_DeviceState != DEVICE_STATE_OK )
		{
			bValidPresent = false;
		}
	}

	// If we're not iconified, try to present (without this check, we can flicker when Alt-Tabbed away)
	if ( bValidPresent )
	{
		HRESULT hr;

		// Rects work only for D3DSWAPEFFECT_COPY.
		// See https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9ex-presentex
		if ( ( m_IsResizing || m_ViewHWnd != (VD3DHWND)m_hWnd ) &&
			 m_PresentParameters.SwapEffect == D3DSWAPEFFECT_COPY )
		{
			RECT destRect;
#ifndef DX_TO_GL_ABSTRACTION
			GetClientRect( ( HWND )m_ViewHWnd, &destRect );
#else
			toglGetClientRect( (VD3DHWND)m_ViewHWnd, &destRect );
#endif

			ShaderViewport_t viewport;
			g_pShaderAPI->GetViewports( &viewport, 1 );

			RECT srcRect;
			srcRect.left = viewport.m_nTopLeftX;
			srcRect.right = viewport.m_nTopLeftX + viewport.m_nWidth;
			srcRect.top = viewport.m_nTopLeftY;
			srcRect.bottom = viewport.m_nTopLeftY + viewport.m_nHeight;

			hr = D3D9Device()->PresentEx( &srcRect, &destRect, (VD3DHWND)m_ViewHWnd, nullptr, 0 );
		}
		else
		{
			hr = D3D9Device()->PresentEx( nullptr, nullptr, nullptr, nullptr, 0 );
		}

		if constexpr ( IsWindows() )
		{
			// Possible PresentEx return values include:
			// S_OK,
			// D3DERR_DEVICELOST,
			// D3DERR_DEVICEHUNG,
			// D3DERR_DEVICEREMOVED,
			// D3DERR_OUTOFVIDEOMEMORY
			// D3DERR_DRIVERINTERNALERROR
			// and S_PRESENT_MODE_CHANGED or S_PRESENT_OCCLUDED
			if ( FAILED( hr ) )
			{
				Warning( "IDirect3DDevice9Ex::PresentEx() failed w/e %s.\n",
					se::win::com::com_error_category().message(hr).c_str() );

				// Sometimes the API calls get loaded into a command buffer and are batched up to be sent
				// to the GPU (see https://learn.microsoft.com/en-us/windows/win32/direct3d9/accurately-profiling-direct3d-api-calls).
				// In this case, the errors cannot be relayed to the application when action needs to be
				// taken, so the error code is consumed by the runtime and a note is made on the device
				// object that this happened.
				// 
				// Later when the application invokes IDirect3DDevice9(Ex)::Present(Ex),
				// IDirect3DDevice9(Ex)::Present(Ex) will return D3DERR_DRIVERINTERNALERROR.  This is why
				// the best approach for an application to take when receiving a
				// D3DERR_DRIVERINTERNALERROR from IDirect3DDevice9::Present is to destroy and recreate the
				// device.
				// 
				// See https://learn.microsoft.com/en-us/windows/win32/direct3d9/driver-internal-errors
				if ( hr == D3DERR_DRIVERINTERNALERROR )
				{
					/*	Usually this bug means that the driver has run out of internal video
						memory, due to leaking it slowly over several application restarts. 
					*/
					Error( "GPU internal driver error at Present.\n"
						   "You're likely out of OS Paged Pool Memory! For more info, see\n"
						   "https://help.steampowered.com/\n" );
				}
				else if ( hr == D3DERR_DEVICEHUNG )
				{
					MarkDeviceHung();
				}
				else if ( hr == D3DERR_OUTOFVIDEOMEMORY )
				{
					MarkDeviceOutOfGpuMemory();
				}
				// Last as since Direct3D 9Ex devices are rarely lost.
				else if ( hr == D3DERR_DEVICELOST )
				{
					MarkDeviceLost();
				}
				}
			else
			{
				// S_OK, S_PRESENT_OCCLUDED and S_PRESENT_MODE_CHANGED

				if ( hr == S_PRESENT_OCCLUDED )
				{
					// S_PRESENT_OCCLUDED: The presentation area is occluded. Occlusion means
					// that the presentation window is minimized or another device entered the
					// fullscreen mode on the same monitor as the presentation window and the
					// presentation window is completely on that monitor.  Occlusion will not
					// occur if the client area is covered by another 2indow.
					//
					// Occluded applications can continue rendering and all calls will succeed,
					// but the occluded presentation window will not be updated.
					MarkPresentOccluded();
				}
			}
		}
	}

	MeshMgr()->DiscardVertexBuffers();

	if ( bInMainThread )
	{
		CheckDeviceState( m_bOtherAppInitializing );
	}

#ifdef RECORD_KEYFRAMES
	static int frame = 0;
	++frame;
	if (frame == KEYFRAME_INTERVAL)
	{
		RECORD_COMMAND( DX8_KEYFRAME, 0 );

		g_pShaderAPI->ResetRenderState();
		frame = 0;
	}
#endif

	g_pShaderAPI->AdvancePIXFrame();

	// Start next scene.
	EndPresent();
}


// We need to scale our colors to the range [16, 235] to keep our colors within TV standards.  Some colors might
//    still be out of gamut if any of the R, G, or B channels are more than 191 units apart from each other in
//    the 0-255 scale, but it looks like the 360 deals with this for us by lowering the bright saturated color components.
// NOTE: I'm leaving the max at 255 to retain whiter than whites.  On most TV's, we seems a little dark in the bright colors
//    compared to TV and movies when played in the same conditions.  This keeps out brights on par with what customers are
//    used to seeing.
// TV's generally have a 2.5 gamma, so we need to convert our 2.2 frame buffer into a 2.5 frame buffer for display on a TV

void CShaderDeviceDx8::SetHardwareGammaRamp( float fGamma, float fGammaTVRangeMin, float fGammaTVRangeMax, float fGammaTVExponent, bool bTVEnabled )
{
	DevMsg( 2, "SetHardwareGammaRamp( %f )\n", fGamma );

	Assert( D3D9Device() );
	if( !D3D9Device() )
		return;

	D3DGAMMARAMP gammaRamp;
	for ( int i = 0; i < 256; i++ )
	{
		float flSrgbGammaValue = float( i ) / 255.0f;

		// Apply the user controlled exponent curve
		float flCorrection = clamp( powf( flSrgbGammaValue, fGamma / 2.2f ), 0.0f, 1.0f );

		// TV adjustment - Apply an exp and a scale and bias
		if ( bTVEnabled )
		{
			// Adjust for TV gamma of 2.5 by applying an exponent of 2.2 / 2.5 = 0.88
			flCorrection = powf( flCorrection, 2.2f / fGammaTVExponent );
			flCorrection = clamp( flCorrection, 0.0f, 1.0f );

			// Scale and bias to fit into the 16-235 range for TV's
			flCorrection = ( flCorrection * ( fGammaTVRangeMax - fGammaTVRangeMin ) / 255.0f ) + ( fGammaTVRangeMin / 255.0f );
			flCorrection = clamp( flCorrection, 0.0f, 1.0f );
		}

		// Generate final int value
		const auto val = static_cast<unsigned short>( flCorrection * 65535.0f );

		gammaRamp.red[i] = val;
		gammaRamp.green[i] = val;
		gammaRamp.blue[i] = val;
	}

	D3D9Device()->SetGammaRamp(0, D3DSGR_NO_CALIBRATION, &gammaRamp);
}


//-----------------------------------------------------------------------------
// Shader compilation
//-----------------------------------------------------------------------------
IShaderBuffer* CShaderDeviceDx8::CompileShader( const char *pProgram, size_t nBufLen, const char *pShaderVersion )
{
	return ShaderManager()->CompileShader( pProgram, nBufLen, pShaderVersion );
}

VertexShaderHandle_t CShaderDeviceDx8::CreateVertexShader( IShaderBuffer *pBuffer )
{
	return ShaderManager()->CreateVertexShader( pBuffer );
}

void CShaderDeviceDx8::DestroyVertexShader( VertexShaderHandle_t hShader )
{
	ShaderManager()->DestroyVertexShader( hShader );
}

GeometryShaderHandle_t CShaderDeviceDx8::CreateGeometryShader( IShaderBuffer* pShaderBuffer )
{
	Assert( 0 );
	return GEOMETRY_SHADER_HANDLE_INVALID;
}

void CShaderDeviceDx8::DestroyGeometryShader( GeometryShaderHandle_t hShader )
{
	Assert( hShader == GEOMETRY_SHADER_HANDLE_INVALID );
}

PixelShaderHandle_t CShaderDeviceDx8::CreatePixelShader( IShaderBuffer *pBuffer )
{
	return ShaderManager()->CreatePixelShader( pBuffer );
}

void CShaderDeviceDx8::DestroyPixelShader( PixelShaderHandle_t hShader )
{
	ShaderManager()->DestroyPixelShader( hShader );
}

#ifdef DX_TO_GL_ABSTRACTION
void CShaderDeviceDx8::DoStartupShaderPreloading( void )
{
	ShaderManager()->DoStartupShaderPreloading();
}
#endif


//-----------------------------------------------------------------------------
// Creates/destroys Mesh
// NOTE: Will be deprecated soon!
//-----------------------------------------------------------------------------
IMesh* CShaderDeviceDx8::CreateStaticMesh( VertexFormat_t vertexFormat, const char *pTextureBudgetGroup, IMaterial * pMaterial )
{
	LOCK_SHADERAPI();
	return MeshMgr()->CreateStaticMesh( vertexFormat, pTextureBudgetGroup, pMaterial );
}

void CShaderDeviceDx8::DestroyStaticMesh( IMesh* pMesh )
{
	LOCK_SHADERAPI();
	MeshMgr()->DestroyStaticMesh( pMesh );
}


//-----------------------------------------------------------------------------
// Creates/destroys vertex buffers + index buffers
//-----------------------------------------------------------------------------
IVertexBuffer *CShaderDeviceDx8::CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup )
{
	LOCK_SHADERAPI();
	return MeshMgr()->CreateVertexBuffer( type, fmt, nVertexCount, pBudgetGroup );
}

void CShaderDeviceDx8::DestroyVertexBuffer( IVertexBuffer *pVertexBuffer )
{
	LOCK_SHADERAPI();
	MeshMgr()->DestroyVertexBuffer( pVertexBuffer );
}

IIndexBuffer *CShaderDeviceDx8::CreateIndexBuffer( ShaderBufferType_t bufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup )
{
	LOCK_SHADERAPI();
	return MeshMgr()->CreateIndexBuffer( bufferType, fmt, nIndexCount, pBudgetGroup );
}

void CShaderDeviceDx8::DestroyIndexBuffer( IIndexBuffer *pIndexBuffer )
{
	LOCK_SHADERAPI();
	MeshMgr()->DestroyIndexBuffer( pIndexBuffer );
}

IVertexBuffer *CShaderDeviceDx8::GetDynamicVertexBuffer( int streamID, VertexFormat_t vertexFormat, bool bBuffered )
{
	LOCK_SHADERAPI();
	return MeshMgr()->GetDynamicVertexBuffer( streamID, vertexFormat, bBuffered );
}

IIndexBuffer *CShaderDeviceDx8::GetDynamicIndexBuffer( MaterialIndexFormat_t fmt, bool bBuffered )
{
	LOCK_SHADERAPI();
	return MeshMgr()->GetDynamicIndexBuffer( fmt, bBuffered );
}
