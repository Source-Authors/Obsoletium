//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include <d3d10.h>
#include <d3dcompiler.h>

#include "shaderdevicedx10.h"
#include "shaderdevicedx8.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapidx10.h"
#include "ShaderShadowDx10.h"
#include "meshdx10.h"
#include "shaderapidx10_global.h"
#include "tier1/KeyValues.h"
#include "tier2/tier2.h"
#include "tier0/icommandline.h"
#include "inputlayoutdx10.h"
#include "shaderapibase.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

// For testing Fast Clip
ConVar mat_fastclip("mat_fastclip", "0", FCVAR_CHEAT);

//-----------------------------------------------------------------------------
// Explicit instantiation of shader buffer implementation
//-----------------------------------------------------------------------------
template class CShaderBuffer< ID3D10Blob >;


//-----------------------------------------------------------------------------
//
// Device manager
//
//-----------------------------------------------------------------------------
static CShaderDeviceMgrDx10 g_ShaderDeviceMgrDx10;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderDeviceMgrDx10, IShaderDeviceMgr, 
	SHADER_DEVICE_MGR_INTERFACE_VERSION, g_ShaderDeviceMgrDx10 )

static CShaderDeviceDx10 g_ShaderDeviceDx10;
CShaderDeviceDx10* g_pShaderDeviceDx10 = &g_ShaderDeviceDx10;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderDeviceDx10, IShaderDevice, 
	SHADER_DEVICE_INTERFACE_VERSION, g_ShaderDeviceDx10 )

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CShaderDeviceMgrDx10::CShaderDeviceMgrDx10()
{
	m_bObeyDxCommandlineOverride = true;
}

CShaderDeviceMgrDx10::~CShaderDeviceMgrDx10()
{
}


//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrDx10::Connect( CreateInterfaceFn factory )
{
	LOCK_SHADERAPI();

	if ( !BaseClass::Connect( factory ) )
		return false;

	// Windows 7+.
	HRESULT hr{CreateDXGIFactory1( IID_PPV_ARGS(&m_pDXGIFactory) )};
	if ( FAILED( hr ) )
	{
		Warning( "Failed to create the DXGI Factory #1!\n" );
		return false;
	}

	InitAdapterInfo();
	return true;
}

void CShaderDeviceMgrDx10::Disconnect()
{
	LOCK_SHADERAPI();

	if (m_pDXGIFactory)
	{
		m_pDXGIFactory.Release();
	}

	BaseClass::Disconnect();
}


//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
InitReturnVal_t CShaderDeviceMgrDx10::Init( )
{
	LOCK_SHADERAPI();

	return INIT_OK;
}


//-----------------------------------------------------------------------------
// Shutdown
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx10::Shutdown( )
{
	LOCK_SHADERAPI();

	if ( g_pShaderDevice )
	{
		g_pShaderDevice->ShutdownDevice();
		g_pShaderDevice = NULL;
	}
}


//-----------------------------------------------------------------------------
// Initialize adapter information
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx10::InitAdapterInfo()
{
	m_Adapters.RemoveAll();

	se::win::com::com_ptr<IDXGIAdapter1> pAdapter;
	for( UINT nCount{0}; m_pDXGIFactory->EnumAdapters1( nCount, &pAdapter ) != DXGI_ERROR_NOT_FOUND; ++nCount )
	{
		AdapterInfo_t &info{m_Adapters[m_Adapters.AddToTail()]};

#ifdef _DEBUG
		memset( &info.m_ActualCaps, 0xDD, sizeof(info.m_ActualCaps) );
#endif

		se::win::com::com_ptr<IDXGIOutput> pOutput{GetAdapterOutput( nCount )};
		info.m_ActualCaps.m_bDeviceOk = ComputeCapsFromD3D( &info.m_ActualCaps, pAdapter, pOutput );
		if ( !info.m_ActualCaps.m_bDeviceOk ) continue;

		ReadDXSupportLevels( info.m_ActualCaps );

		// Read dxsupport.cfg which has config overrides for particular cards.
		ReadHardwareCaps( info.m_ActualCaps, info.m_ActualCaps.m_nMaxDXSupportLevel );

		// What's in "-shader" overrides dxsupport.cfg
		const char *pShaderParam = CommandLine()->ParmValue( "-shader" );
		if ( pShaderParam )
		{
			Q_strncpy( info.m_ActualCaps.m_pShaderDLL, pShaderParam, sizeof( info.m_ActualCaps.m_pShaderDLL ) );
		}
	}
}


//-----------------------------------------------------------------------------
// Determines hardware caps from D3D
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrDx10::ComputeCapsFromD3D( HardwareCaps_t *pCaps, IDXGIAdapter1 *pAdapter, IDXGIOutput *pOutput )
{
  LARGE_INTEGER umdVersion;
	// If you try to use CheckInterfaceSupport to check whether a Direct3D 11.x and later version interface is supported,
	// CheckInterfaceSupport returns DXGI_ERROR_UNSUPPORTED.
	HRESULT hr = pAdapter->CheckInterfaceSupport( __uuidof(ID3D10Device), &umdVersion );
	if ( hr != S_OK )
	{
		// Fall back to Dx9
		AssertMsg( false, "No Dx10 support!" );
		return false;
	}

	DXGI_ADAPTER_DESC1 desc;
	hr = pAdapter->GetDesc1( &desc );
	Assert( !FAILED( hr ) );
	if ( FAILED(hr) || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) )
		return false;

	bool bForceFloatHDR = ( CommandLine()->CheckParm( "-floathdr" ) != NULL );

	// DX10 settings
	// NOTE: We'll need to have different settings for dx10.1 and dx11
	Q_UnicodeToUTF8( desc.Description, pCaps->m_pDriverName, MATERIAL_ADAPTER_NAME_LENGTH );
	pCaps->m_VendorID = desc.VendorId;
	pCaps->m_DeviceID = desc.DeviceId;
	pCaps->m_SubSysID = desc.SubSysId;
	pCaps->m_Revision = desc.Revision;
	pCaps->m_nDXSupportLevel = 100;
	pCaps->m_nMaxDXSupportLevel = 100;

	pCaps->m_NumSamplers = 16;
	pCaps->m_NumTextureStages = 0;
	pCaps->m_HasSetDeviceGammaRamp = true;
	pCaps->m_bSoftwareVertexProcessing = false;
	pCaps->m_SupportsVertexShaders = true;
	pCaps->m_SupportsVertexShaders_2_0 = false;
	pCaps->m_SupportsPixelShaders = true;
	pCaps->m_SupportsPixelShaders_1_4 = false;
	pCaps->m_SupportsPixelShaders_2_0 = false;
	pCaps->m_SupportsPixelShaders_2_b = false;
	pCaps->m_SupportsShaderModel_3_0 = false;
	// pCaps->m_SupportsShaderModel_4_0 = true;
	pCaps->m_SupportsCompressedTextures = COMPRESSED_TEXTURES_ON;
	pCaps->m_SupportsCompressedVertices = VERTEX_COMPRESSION_ON;
	pCaps->m_bSupportsAnisotropicFiltering = true;
	pCaps->m_bSupportsMagAnisotropicFiltering = true;
	pCaps->m_bSupportsVertexTextures = true;
	pCaps->m_nMaxAnisotropy = D3D10_REQ_MAXANISOTROPY;
	pCaps->m_MaxTextureWidth = D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	pCaps->m_MaxTextureHeight = D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	pCaps->m_MaxTextureDepth = D3D10_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
	pCaps->m_MaxTextureAspectRatio = 1024;	// FIXME
	pCaps->m_MaxPrimitiveCount = (1ULL << D3D10_IA_PRIMITIVE_ID_BIT_COUNT) - 1;		// FIXME
	pCaps->m_ZBiasAndSlopeScaledDepthBiasSupported = true;
	pCaps->m_SupportsMipmapping = true;
	pCaps->m_SupportsOverbright = true;
	pCaps->m_SupportsCubeMaps = true;
	pCaps->m_NumPixelShaderConstants = 1024;	// FIXME
	pCaps->m_NumVertexShaderConstants = 1024;	// FIXME
	pCaps->m_TextureMemorySize = desc.DedicatedVideoMemory;
	pCaps->m_MaxNumLights = 4;
	pCaps->m_SupportsHardwareLighting = false;
	pCaps->m_MaxBlendMatrices = 0;
	pCaps->m_MaxBlendMatrixIndices = 0;
	pCaps->m_MaxVertexShaderBlendMatrices = 53;	// FIXME
	pCaps->m_SupportsMipmappedCubemaps = true;
	pCaps->m_SupportsNonPow2Textures = true;
	pCaps->m_PreferDynamicTextures = false;
	pCaps->m_HasProjectedBumpEnv = true;
	pCaps->m_MaxUserClipPlanes = 6;		// FIXME
	pCaps->m_HDRType = bForceFloatHDR ? HDR_TYPE_FLOAT : HDR_TYPE_INTEGER;
	pCaps->m_SupportsSRGB = true;
	pCaps->m_FakeSRGBWrite = true;
	pCaps->m_CanDoSRGBReadFromRTs = true;
	pCaps->m_bSupportsSpheremapping = true;
	pCaps->m_UseFastClipping = false;
	pCaps->m_pShaderDLL[0] = 0;
	pCaps->m_bNeedsATICentroidHack = false;
	pCaps->m_bColorOnSecondStream = true;
	pCaps->m_bSupportsStreamOffset = true;
	pCaps->m_bFogColorSpecifiedInLinearSpace = ( desc.VendorId == VENDORID_NVIDIA );
	pCaps->m_nVertexTextureCount = D3D10_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
	pCaps->m_nMaxVertexTextureDimension = D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	pCaps->m_bSupportsAlphaToCoverage = false;	// FIXME
	pCaps->m_bSupportsShadowDepthTextures = true;
	pCaps->m_bSupportsFetch4 = ( desc.VendorId == VENDORID_ATI );
	pCaps->m_bSupportsBorderColor = true;
	pCaps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_UNKNOWN;
	pCaps->m_nMaxViewports = 4;

	DXGI_GAMMA_CONTROL_CAPABILITIES gammaCaps;
	// Calling this method is only supported while in full-screen mode.
	if ( SUCCEEDED(pOutput->GetGammaControlCapabilities( &gammaCaps )) )
	{
    pCaps->m_flMinGammaControlPoint = gammaCaps.MinConvertedValue;
    pCaps->m_flMaxGammaControlPoint = gammaCaps.MaxConvertedValue;
    pCaps->m_nGammaControlPointCount = gammaCaps.NumGammaControlPoints;
	}
	pCaps->m_bCanStretchRectFromTextures = true;
	return true;
}


//-----------------------------------------------------------------------------
// Gets the number of adapters...
//-----------------------------------------------------------------------------
unsigned CShaderDeviceMgrDx10::GetAdapterCount() const
{
	return m_Adapters.Count();
}


//-----------------------------------------------------------------------------
// Returns info about each adapter
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx10::GetAdapterInfo( unsigned nAdapter, MaterialAdapterInfo_t& info ) const
{
	Assert( ( nAdapter >= 0 ) && ( nAdapter < (unsigned)m_Adapters.Count() ) );
	const HardwareCaps_t &caps = m_Adapters[ nAdapter ].m_ActualCaps;
	memcpy( &info, &caps, sizeof(MaterialAdapterInfo_t) );
}


//-----------------------------------------------------------------------------
// Returns the adapter interface for a particular adapter
//-----------------------------------------------------------------------------
se::win::com::com_ptr<IDXGIAdapter1> CShaderDeviceMgrDx10::GetAdapter( unsigned nAdapter ) const
{
	Assert( m_pDXGIFactory && ( nAdapter < GetAdapterCount() ) );

	se::win::com::com_ptr<IDXGIAdapter1> pAdapter;
	const HRESULT hr{m_pDXGIFactory->EnumAdapters1( nAdapter, &pAdapter )};
	return SUCCEEDED(hr) ? pAdapter : se::win::com::com_ptr<IDXGIAdapter1>{};
}


//-----------------------------------------------------------------------------
// Returns the amount of video memory in bytes for a particular adapter
//-----------------------------------------------------------------------------
size_t CShaderDeviceMgrDx10::GetVidMemBytes( unsigned nAdapter ) const
{
	LOCK_SHADERAPI();

  se::win::com::com_ptr<IDXGIAdapter1> pAdapter{GetAdapter(nAdapter)};
	if ( !pAdapter )
		return 0;

	DXGI_ADAPTER_DESC1 desc;
	const HRESULT hr{pAdapter->GetDesc1(&desc)};

	Assert( !FAILED( hr ) );

	return SUCCEEDED(hr) ? desc.DedicatedVideoMemory : 0;
}


//-----------------------------------------------------------------------------
// Returns the appropriate adapter output to use
//-----------------------------------------------------------------------------
se::win::com::com_ptr<IDXGIOutput> CShaderDeviceMgrDx10::GetAdapterOutput( unsigned nAdapter ) const
{
	LOCK_SHADERAPI();

	se::win::com::com_ptr<IDXGIAdapter1> pAdapter{GetAdapter(nAdapter)};
	if ( !pAdapter ) return {};

	DXGI_OUTPUT_DESC desc;
	se::win::com::com_ptr<IDXGIOutput> pOutput;
	for( UINT i{0}; pAdapter->EnumOutputs( i, &pOutput ) != DXGI_ERROR_NOT_FOUND; ++i )
	{
		const HRESULT hr{pOutput->GetDesc( &desc )};
		if ( FAILED( hr ) )
			continue;

		// FIXME: Is this what I want? Or should I be looking at other fields,
		// like DXGI_MODE_ROTATION_IDENTITY?
		if ( !desc.AttachedToDesktop )
			continue;

		return pOutput;
	}

	return {};
}


//-----------------------------------------------------------------------------
// Returns the number of modes
//-----------------------------------------------------------------------------
unsigned CShaderDeviceMgrDx10::GetModeCount( unsigned nAdapter ) const
{
	LOCK_SHADERAPI();
	Assert( m_pDXGIFactory && ( nAdapter < GetAdapterCount() ) );

	se::win::com::com_ptr<IDXGIOutput> pOutput{GetAdapterOutput( nAdapter )};
	if ( !pOutput )
		return 0;
	
	UINT num = 0;
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; //desired color format
	UINT flags         = 0; //desired scanline order and/or scaling

	// get the number of available display mode for the given format and scanline order
	HRESULT hr = pOutput->GetDisplayModeList( format, flags, &num, nullptr );
	Assert( !FAILED( hr ) );

	return SUCCEEDED(hr) ? num : 0;
}


//-----------------------------------------------------------------------------
// Returns mode information..
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx10::GetModeInfo( ShaderDisplayMode_t* pInfo, unsigned nAdapter, unsigned nMode ) const
{
	// Default error state
	pInfo->m_nWidth = pInfo->m_nHeight = 0;
	pInfo->m_Format = IMAGE_FORMAT_UNKNOWN;
	pInfo->m_nRefreshRateNumerator = pInfo->m_nRefreshRateDenominator = 0;

	LOCK_SHADERAPI();
	Assert( m_pDXGIFactory && ( nAdapter < GetAdapterCount() ) );

	se::win::com::com_ptr<IDXGIOutput> pOutput{GetAdapterOutput( nAdapter )};
	if ( !pOutput ) return;

	UINT num = 0;
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; //desired color format
	UINT flags         = DXGI_ENUM_MODES_INTERLACED; //desired scanline order and/or scaling

	// get the number of available display mode for the given format and scanline order
	HRESULT hr = pOutput->GetDisplayModeList( format, flags, &num, 0 );
	Assert( !FAILED( hr ) );

	if ( nMode >= num ) return;

	auto *pDescs = (DXGI_MODE_DESC*)_alloca( num * sizeof( DXGI_MODE_DESC ) );
	hr = pOutput->GetDisplayModeList( format, flags, &num, pDescs );
	Assert( !FAILED( hr ) );

	pInfo->m_nWidth      = pDescs[nMode].Width;
	pInfo->m_nHeight     = pDescs[nMode].Height;
//	pInfo->m_Format      = ImageLoader::D3DFormatToImageFormat( pDescs[nMode].Format );
	pInfo->m_nRefreshRateNumerator = pDescs[nMode].RefreshRate.Numerator;
	pInfo->m_nRefreshRateDenominator = pDescs[nMode].RefreshRate.Denominator;
}


//-----------------------------------------------------------------------------
// Returns the current mode for an adapter
//-----------------------------------------------------------------------------
void CShaderDeviceMgrDx10::GetCurrentModeInfo( ShaderDisplayMode_t* pInfo, unsigned nAdapter ) const
{
	Assert( pInfo->m_nVersion == SHADER_DISPLAY_MODE_VERSION );
	LOCK_SHADERAPI();

	se::win::com::com_ptr<IDXGIOutput> pOutput{g_ShaderDeviceMgrDx10.GetAdapterOutput(nAdapter)};
	
	DXGI_OUTPUT_DESC outputDesc;
	HRESULT hr{pOutput->GetDesc(&outputDesc)};
	Assert( !FAILED(hr) );

	UINT width = static_cast<unsigned>(outputDesc.DesktopCoordinates.right - outputDesc.DesktopCoordinates.left);
	UINT height = static_cast<unsigned>(outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top);

	DXGI_MODE_DESC modeDesc{width, height, {}, DXGI_FORMAT_UNKNOWN, DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED, DXGI_MODE_SCALING_UNSPECIFIED};

	// FIXME: Implement!
	Assert(0);
	HDC dc = ::GetDC(NULL);
	modeDesc.RefreshRate.Numerator = ::GetDeviceCaps(dc, VREFRESH);
	modeDesc.RefreshRate.Denominator = 1;
	::ReleaseDC(NULL, dc);

	pInfo->m_nWidth = modeDesc.Width;
	pInfo->m_nHeight = modeDesc.Height;
	// pInfo->m_Format = ImageLoader::D3DFormatToImageFormat( closestModeDesc.Format );
	pInfo->m_nRefreshRateNumerator = modeDesc.RefreshRate.Numerator;
	pInfo->m_nRefreshRateDenominator = modeDesc.RefreshRate.Denominator;
}


//-----------------------------------------------------------------------------
// Initialization, shutdown
//-----------------------------------------------------------------------------
bool CShaderDeviceMgrDx10::SetAdapter( unsigned nAdapter, int nFlags )
{
  LOCK_SHADERAPI();
	/*
	if ( !g_pShaderDeviceDx10->Init() )
	{
	Warning( "Unable to initialize dx10 device!\n" );
	return false;
	}

	g_pMaterialSystemHardwareConfig = g_pShaderDeviceDx10;
	*/

  g_pShaderDeviceDx10->m_DriverType = (nFlags & MATERIAL_INIT_REFERENCE_RASTERIZER) ? 
		D3D10_DRIVER_TYPE_REFERENCE : D3D10_DRIVER_TYPE_HARDWARE;
  g_pShaderDeviceDx10->m_DisplayAdapter = Clamp(nAdapter, 0U, GetAdapterCount() - 1);

	if (!g_pShaderDeviceDx10->DetermineHardwareCaps())
	{
    return false;
  }

	// Modify the caps based on requested DXlevels
	int nForcedDXLevel = CommandLine()->ParmValue( "-dxlevel", 0 );
	
	if ( nForcedDXLevel > 0 )
	{
		nForcedDXLevel = MAX( nForcedDXLevel, ABSOLUTE_MINIMUM_DXLEVEL );
	}

	// Just use the actual caps if we can't use what was requested or if the default is requested
	if ( nForcedDXLevel <= 0 ) 
	{
		nForcedDXLevel = g_pHardwareConfig->ActualCaps().m_nDXSupportLevel;
	}
	nForcedDXLevel = g_pShaderDeviceMgr->GetClosestActualDXLevel( nForcedDXLevel );

	g_pHardwareConfig->SetupHardwareCaps( nForcedDXLevel, g_pHardwareConfig->ActualCaps() );

	g_pShaderDevice = g_pShaderDeviceDx10;

	return true;
}


//-----------------------------------------------------------------------------
// Sets the mode
//-----------------------------------------------------------------------------
CreateInterfaceFn CShaderDeviceMgrDx10::SetMode( void *hWnd, unsigned nAdapter, const ShaderDeviceInfo_t& mode )
{
	LOCK_SHADERAPI();

	Assert( nAdapter < GetAdapterCount() );

	const auto &actualCaps = m_Adapters[nAdapter].m_ActualCaps;
	int nDXLevel = mode.m_nDXLevel != 0 ? mode.m_nDXLevel : actualCaps.m_nDXSupportLevel;
	if ( m_bObeyDxCommandlineOverride )
	{
		nDXLevel = CommandLine()->ParmValue( "-dxlevel", nDXLevel );
		m_bObeyDxCommandlineOverride = false;
	}
	nDXLevel = GetClosestActualDXLevel( min( nDXLevel, actualCaps.m_nMaxDXSupportLevel ) );

	// TODO(dimhotepus): Handle Dx9 fallback.
	//if ( nDXLevel < 100 )
	//{
	//	// Fall back to the Dx9 implementations
	//	return g_pShaderDeviceMgrDx8->SetMode( hWnd, nAdapter, mode );
	//}

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
	if ( !g_pShaderDeviceDx10->InitDevice( hWnd, nAdapter, adjustedMode ) )
		return NULL;

	if ( !g_pShaderAPIDx10->OnDeviceInit() )
		return NULL;
	
	g_pShaderDeviceDx10->DetermineHardwareCaps();

	g_pShaderDevice = g_pShaderDeviceDx10;
	g_pShaderAPI = g_pShaderAPIDx10;
	g_pShaderShadow = g_pShaderShadowDx10;


	return ShaderInterfaceFactory;
}


//-----------------------------------------------------------------------------
//
// Device
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CShaderDeviceDx10::CShaderDeviceDx10()
	: m_DisplayAdapter(UINT_MAX),
	m_DriverType(D3D10_DRIVER_TYPE_NULL)
{
}

CShaderDeviceDx10::~CShaderDeviceDx10()
{
}


//-----------------------------------------------------------------------------
// Sets the mode
//-----------------------------------------------------------------------------
bool CShaderDeviceDx10::InitDevice( void *hWnd, unsigned nAdapter, const ShaderDeviceInfo_t& mode )
{
	// Make sure we've been shutdown previously
	if ( m_nAdapter != UINT_MAX )
	{
		Warning( "CShaderDeviceDx10::SetMode: Previous mode has not been shut down!\n" );
		return false;
	}

	LOCK_SHADERAPI();

	se::win::com::com_ptr<IDXGIAdapter1> pAdapter{g_ShaderDeviceMgrDx10.GetAdapter( nAdapter )};
	if ( !pAdapter ) return false;

	m_pOutput = g_ShaderDeviceMgrDx10.GetAdapterOutput( nAdapter );
	if ( !m_pOutput )	return false;

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory( &sd, sizeof(sd) );
	sd.BufferDesc.Width = mode.m_DisplayMode.m_nWidth;
	sd.BufferDesc.Height = mode.m_DisplayMode.m_nHeight;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = mode.m_DisplayMode.m_nRefreshRateNumerator;
	sd.BufferDesc.RefreshRate.Denominator = mode.m_DisplayMode.m_nRefreshRateDenominator;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = mode.m_nBackBufferCount;
	sd.OutputWindow = (HWND)hWnd;
	sd.Windowed = mode.m_bWindowed ? TRUE : FALSE;
	sd.Flags = mode.m_bWindowed ? 0 : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// NOTE: Having more than 1 back buffer disables MSAA!
	sd.SwapEffect = mode.m_nBackBufferCount > 1
		? DXGI_SWAP_EFFECT_SEQUENTIAL : DXGI_SWAP_EFFECT_DISCARD;

	// FIXME: Chicken + egg problem with SampleDesc.
	sd.SampleDesc.Count = mode.m_nAASamples ? mode.m_nAASamples : 1;
	sd.SampleDesc.Quality = mode.m_nAAQuality;

	// Return a NULL pointer instead of triggering an exception on memory exhaustion during invocations to Map.
	// Without this flag an exception will be raised on memory exhaustion. Only valid on Windows 7.
	UINT nDeviceFlags = D3D10_CREATE_DEVICE_ALLOW_NULL_FROM_MAP;
#ifdef _DEBUG
	nDeviceFlags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

	HRESULT hr = D3D10CreateDeviceAndSwapChain( pAdapter, m_DriverType, 
		NULL, nDeviceFlags, D3D10_SDK_VERSION, &sd, &m_pSwapChain, &m_pDevice );
	if ( FAILED( hr ) )	return false;

	// Create a render target view
	se::win::com::com_ptr<ID3D10Texture2D> pBackBuffer;
	hr = m_pSwapChain->GetBuffer( 0, IID_PPV_ARGS(&pBackBuffer) );
	if ( FAILED( hr ) )	return false;

	hr = m_pDevice->CreateRenderTargetView( pBackBuffer, NULL, &m_pRenderTargetView );
	if( FAILED( hr ) ) return false;

	ID3D10RenderTargetView* views[]{m_pRenderTargetView.GetInterfacePtr()}; 
	m_pDevice->OMSetRenderTargets( std::size(views), views, NULL );

	m_hWnd = hWnd;
	m_nAdapter = m_DisplayAdapter = nAdapter;

	// This is our current view.
	m_ViewHWnd = hWnd;
	GetWindowSize( m_nWindowWidth, m_nWindowHeight );

	g_pHardwareConfig->SetupHardwareCaps( mode, g_ShaderDeviceMgrDx10.GetHardwareCaps( nAdapter ) );

	return true;
}


//-----------------------------------------------------------------------------
// Shuts down the mode
//-----------------------------------------------------------------------------
void CShaderDeviceDx10::ShutdownDevice() 
{
	if ( m_pRenderTargetView )
	{
		m_pRenderTargetView.Release();
	}

	if ( m_pDevice )
	{
		m_pDevice.Release();
	}

	if ( m_pSwapChain )
	{
		m_pSwapChain.Release();
	}

	if ( m_pOutput )
	{
		m_pOutput.Release();
	}

	m_hWnd = NULL;
	m_nAdapter = UINT_MAX;
	m_DisplayAdapter = UINT_MAX;
}


//-----------------------------------------------------------------------------
// Are we using graphics?
//-----------------------------------------------------------------------------
bool CShaderDeviceDx10::IsUsingGraphics() const
{
	return !!m_pDevice;
}


//-----------------------------------------------------------------------------
// Returns the adapter
//-----------------------------------------------------------------------------
unsigned CShaderDeviceDx10::GetCurrentAdapter() const
{
	return m_DisplayAdapter;
}


//-----------------------------------------------------------------------------
// Get back buffer information
//-----------------------------------------------------------------------------
ImageFormat CShaderDeviceDx10::GetBackBufferFormat() const
{
	return IMAGE_FORMAT_RGB888;
}

void CShaderDeviceDx10::GetBackBufferDimensions( int& width, int& height ) const
{
	width = 1024;
	height = 768;
}


//-----------------------------------------------------------------------------
// Use this to spew information about the 3D layer 
//-----------------------------------------------------------------------------
void CShaderDeviceDx10::SpewDriverInfo() const
{
	Warning( "Dx10 Driver!\n" );
}



//-----------------------------------------------------------------------------
// Swap buffers
//-----------------------------------------------------------------------------
void CShaderDeviceDx10::Present()
{
	// FIXME: Deal with window occlusion, alt-tab, etc.
	HRESULT hr{m_pSwapChain->Present( 0, 0 )};
	if ( FAILED(hr) )
	{
		Assert( 0 );
	}
}


//-----------------------------------------------------------------------------
// Camma ramp
//-----------------------------------------------------------------------------
void CShaderDeviceDx10::SetHardwareGammaRamp( float fGamma, float fGammaTVRangeMin, float fGammaTVRangeMax, float fGammaTVExponent, bool bTVEnabled )
{
	DevMsg( "SetHardwareGammaRamp( %f )\n", fGamma );

	Assert( m_pOutput );
	if( !m_pOutput )
		return;

	float flMin = g_pHardwareConfig->Caps().m_flMinGammaControlPoint;
	float flMax = g_pHardwareConfig->Caps().m_flMaxGammaControlPoint;
	int nGammaPoints = g_pHardwareConfig->Caps().m_nGammaControlPointCount;

	DXGI_GAMMA_CONTROL gammaControl;
	gammaControl.Scale.Red = gammaControl.Scale.Green = gammaControl.Scale.Blue = 1.0f;
	gammaControl.Offset.Red = gammaControl.Offset.Green = gammaControl.Offset.Blue = 0.0f;
	float flOOCount = 1.0f / ( nGammaPoints - 1 );
	for ( int i = 0; i < nGammaPoints; i++ )
	{
		float flGamma22 = i * flOOCount;
		float flCorrection = powf( flGamma22, fGamma / 2.2f );
		flCorrection = clamp( flCorrection, flMin, flMax );

		gammaControl.GammaCurve[i].Red = flCorrection;
		gammaControl.GammaCurve[i].Green = flCorrection;
		gammaControl.GammaCurve[i].Blue = flCorrection;
	}

	HRESULT hr{m_pOutput->SetGammaControl( &gammaControl )};
	if ( FAILED(hr) )
	{
		Warning( "CShaderDeviceDx10::SetHardwareGammaRamp: Unable to set gamma controls!\n" );
	}
}


//-----------------------------------------------------------------------------
// Compiles all manner of shaders
//-----------------------------------------------------------------------------
IShaderBuffer* CShaderDeviceDx10::CompileShader( const char *pProgram, size_t nBufLen, const char *pShaderVersion )
{
	unsigned nCompileFlags = D3D10_SHADER_AVOID_FLOW_CONTROL;
	nCompileFlags |= D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY;

#ifdef _DEBUG
	nCompileFlags |= D3D10_SHADER_DEBUG | D3D10_SHADER_OPTIMIZATION_LEVEL0;
#else
	nCompileFlags |= D3D10_SHADER_OPTIMIZATION_LEVEL3;
#endif

	se::win::com::com_ptr<ID3DBlob> pCompiledShader, pErrorMessages;
	HRESULT hr = D3DCompile( pProgram, nBufLen, nullptr,
		nullptr, nullptr, "main", pShaderVersion, nCompileFlags, 0,
		&pCompiledShader, &pErrorMessages );

	if ( FAILED( hr ) )
	{
		if ( pErrorMessages )
		{
			const char *pErrorMessage = (const char *)pErrorMessages->GetBufferPointer();
			Warning( "Vertex shader compilation failed! Reported the following errors:\n%s\n", pErrorMessage );
		}
		return NULL;
	}

	// NOTE: This uses small block heap allocator; so I'm not going
	// to bother creating a memory pool.
	return new CShaderBuffer( pCompiledShader.Detach() );
}


//-----------------------------------------------------------------------------
// Release input layouts
//-----------------------------------------------------------------------------
void CShaderDeviceDx10::ReleaseInputLayouts( VertexShaderIndex_t nIndex )
{
	InputLayoutDict_t &dict = m_VertexShaderDict[nIndex].m_InputLayouts;
	unsigned short hCurr = dict.FirstInorder();
	while( hCurr != dict.InvalidIndex() )
	{
		if ( dict[hCurr].m_pInputLayout )
		{
			dict[hCurr].m_pInputLayout->Release();
			dict[hCurr].m_pInputLayout = NULL;
		}
		hCurr = dict.NextInorder( hCurr );
	}
}


//-----------------------------------------------------------------------------
// Create, destroy vertex shader
//-----------------------------------------------------------------------------
VertexShaderHandle_t CShaderDeviceDx10::CreateVertexShader( IShaderBuffer* pShaderBuffer )
{
	// Create the vertex shader
	ID3D10VertexShader *pShader = NULL;
	HRESULT hr = m_pDevice->CreateVertexShader( pShaderBuffer->GetBits(),
		pShaderBuffer->GetSize(), &pShader );

	if ( FAILED( hr ) || !pShader )
		return VERTEX_SHADER_HANDLE_INVALID;

	ID3D10ShaderReflection *pInfo;
	hr = D3D10ReflectShader( pShaderBuffer->GetBits(), pShaderBuffer->GetSize(), &pInfo );
	if ( FAILED( hr ) || !pInfo )
	{
		pShader->Release();
		return VERTEX_SHADER_HANDLE_INVALID;
	}

	// Insert the shader into the dictionary of shaders
	VertexShaderIndex_t i = m_VertexShaderDict.AddToTail( );
	VertexShader_t &dict = m_VertexShaderDict[i];
	dict.m_pShader = pShader;
	dict.m_pInfo = pInfo;
	dict.m_nByteCodeLen = pShaderBuffer->GetSize();
	dict.m_pByteCode = new unsigned char[ dict.m_nByteCodeLen ];
	memcpy( dict.m_pByteCode, pShaderBuffer->GetBits(), dict.m_nByteCodeLen );
	return (VertexShaderHandle_t)i;
}

void CShaderDeviceDx10::DestroyVertexShader( VertexShaderHandle_t hShader )
{
	if ( hShader == VERTEX_SHADER_HANDLE_INVALID )
		return;

	g_pShaderAPIDx10->Unbind( hShader );

	VertexShaderIndex_t i = (VertexShaderIndex_t)hShader;
	VertexShader_t &dict = m_VertexShaderDict[i];
	VerifyEquals( dict.m_pShader->Release(), 0 );
	VerifyEquals( dict.m_pInfo->Release(), 0 );
	delete[] dict.m_pByteCode;
	ReleaseInputLayouts( i );
	m_VertexShaderDict.Remove( i );
}


//-----------------------------------------------------------------------------
// Create, destroy geometry shader
//-----------------------------------------------------------------------------
GeometryShaderHandle_t CShaderDeviceDx10::CreateGeometryShader( IShaderBuffer* pShaderBuffer )
{
	// Create the geometry shader
	ID3D10GeometryShader *pShader = NULL;
	HRESULT hr = m_pDevice->CreateGeometryShader( pShaderBuffer->GetBits(),
		pShaderBuffer->GetSize(), &pShader );

	if ( FAILED( hr ) || !pShader )
		return GEOMETRY_SHADER_HANDLE_INVALID;

	ID3D10ShaderReflection *pInfo;
	hr = D3D10ReflectShader( pShaderBuffer->GetBits(), pShaderBuffer->GetSize(), &pInfo );
	if ( FAILED( hr ) || !pInfo )
	{
		pShader->Release();
		return GEOMETRY_SHADER_HANDLE_INVALID;
	}

	// Insert the shader into the dictionary of shaders
	GeometryShaderIndex_t i = m_GeometryShaderDict.AddToTail( );
	m_GeometryShaderDict[i].m_pShader = pShader;
	m_GeometryShaderDict[i].m_pInfo = pInfo;
	return (GeometryShaderHandle_t)i;
}

void CShaderDeviceDx10::DestroyGeometryShader( GeometryShaderHandle_t hShader )
{
	if ( hShader == GEOMETRY_SHADER_HANDLE_INVALID )
		return;

	g_pShaderAPIDx10->Unbind( hShader );

	GeometryShaderIndex_t i = (GeometryShaderIndex_t)hShader;
	VerifyEquals( m_GeometryShaderDict[ i ].m_pShader->Release(), 0 );
	VerifyEquals( m_GeometryShaderDict[ i ].m_pInfo->Release(), 0 );
	m_GeometryShaderDict.Remove( i );
}


//-----------------------------------------------------------------------------
// Create, destroy pixel shader
//-----------------------------------------------------------------------------
PixelShaderHandle_t CShaderDeviceDx10::CreatePixelShader( IShaderBuffer* pShaderBuffer )
{
	// Create the pixel shader
	ID3D10PixelShader *pShader = NULL;
	HRESULT hr = m_pDevice->CreatePixelShader( pShaderBuffer->GetBits(),
		pShaderBuffer->GetSize(), &pShader );

	if ( FAILED( hr ) || !pShader )
		return PIXEL_SHADER_HANDLE_INVALID;

	ID3D10ShaderReflection *pInfo;
	hr = D3D10ReflectShader( pShaderBuffer->GetBits(), pShaderBuffer->GetSize(), &pInfo );
	if ( FAILED( hr ) || !pInfo )
	{
		pShader->Release();
		return PIXEL_SHADER_HANDLE_INVALID;
	}

	// Insert the shader into the dictionary of shaders
	PixelShaderIndex_t i = m_PixelShaderDict.AddToTail( );
	m_PixelShaderDict[i].m_pShader = pShader;
	m_PixelShaderDict[i].m_pInfo = pInfo;
	return (PixelShaderHandle_t)i;
}

void CShaderDeviceDx10::DestroyPixelShader( PixelShaderHandle_t hShader )
{
	if ( hShader == PIXEL_SHADER_HANDLE_INVALID )
		return;

	g_pShaderAPIDx10->Unbind( hShader );

	PixelShaderIndex_t i = (PixelShaderIndex_t)hShader;
	VerifyEquals( m_PixelShaderDict[ i ].m_pShader->Release(), 0 );
	VerifyEquals( m_PixelShaderDict[ i ].m_pInfo->Release(), 0 );
	m_PixelShaderDict.Remove( i );
}


//-----------------------------------------------------------------------------
// Finds or creates an input layout for a given vertex shader + stream format
//-----------------------------------------------------------------------------
ID3D10InputLayout* CShaderDeviceDx10::GetInputLayout( VertexShaderHandle_t hShader, VertexFormat_t format )
{
	if ( hShader == VERTEX_SHADER_HANDLE_INVALID )
		return NULL;

	// FIXME: VertexFormat_t is not the appropriate way of specifying this
	// because it has no stream information
	InputLayout_t insert;
	insert.m_VertexFormat = format;

	VertexShaderIndex_t i = (VertexShaderIndex_t)hShader;
	InputLayoutDict_t &dict = m_VertexShaderDict[i].m_InputLayouts;
	unsigned short hIndex = dict.Find( insert );
	if ( hIndex != dict.InvalidIndex() )
		return dict[hIndex].m_pInputLayout;

	VertexShader_t &shader = m_VertexShaderDict[i];
	insert.m_pInputLayout = CreateInputLayout( format, shader.m_pInfo, shader.m_pByteCode, shader.m_nByteCodeLen );
	dict.Insert( insert );
	return insert.m_pInputLayout;
}


//-----------------------------------------------------------------------------
// Creates/destroys Mesh
//-----------------------------------------------------------------------------
IMesh* CShaderDeviceDx10::CreateStaticMesh( VertexFormat_t vertexFormat, const char *pBudgetGroup, IMaterial * pMaterial )
{
	LOCK_SHADERAPI();
	return NULL;
}

void CShaderDeviceDx10::DestroyStaticMesh( IMesh* pMesh )
{
	LOCK_SHADERAPI();
}


//-----------------------------------------------------------------------------
// Creates/destroys vertex buffers + index buffers
//-----------------------------------------------------------------------------
IVertexBuffer *CShaderDeviceDx10::CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup )
{
	LOCK_SHADERAPI();
	CVertexBufferDx10 *pVertexBuffer = new CVertexBufferDx10( type, fmt, nVertexCount, pBudgetGroup );
	return pVertexBuffer;
}

void CShaderDeviceDx10::DestroyVertexBuffer( IVertexBuffer *pVertexBuffer )
{
	LOCK_SHADERAPI();
	if ( pVertexBuffer )
	{
		CVertexBufferDx10 *pVertexBufferBase = assert_cast<CVertexBufferDx10*>( pVertexBuffer );
		g_pShaderAPIDx10->UnbindVertexBuffer( pVertexBufferBase->GetDx10Buffer() );
		delete pVertexBufferBase;
	}
}

IIndexBuffer *CShaderDeviceDx10::CreateIndexBuffer( ShaderBufferType_t type, MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup )
{
	LOCK_SHADERAPI();
	CIndexBufferDx10 *pIndexBuffer = new CIndexBufferDx10( type, fmt, nIndexCount, pBudgetGroup );
	return pIndexBuffer;
}

void CShaderDeviceDx10::DestroyIndexBuffer( IIndexBuffer *pIndexBuffer )
{
	LOCK_SHADERAPI();
	if ( pIndexBuffer )
	{
		CIndexBufferDx10 *pIndexBufferBase = assert_cast<CIndexBufferDx10*>( pIndexBuffer );
		g_pShaderAPIDx10->UnbindIndexBuffer( pIndexBufferBase->GetDx10Buffer() );
		delete pIndexBufferBase;
	}
}

IVertexBuffer *CShaderDeviceDx10::GetDynamicVertexBuffer( int nStreamID, VertexFormat_t vertexFormat, bool bBuffered )
{
	LOCK_SHADERAPI();
	return NULL;
}

IIndexBuffer *CShaderDeviceDx10::GetDynamicIndexBuffer( MaterialIndexFormat_t fmt, bool bBuffered )
{
	LOCK_SHADERAPI();
	return NULL;
}

const char *CShaderDeviceDx10::GetDisplayDeviceName()
{
	if( m_sDisplayDeviceName.IsEmpty() )
	{
		se::win::com::com_ptr<IDXGIOutput> pOutput{g_ShaderDeviceMgrDx10.GetAdapterOutput(m_DisplayAdapter)};
		if (pOutput)
		{
			DXGI_OUTPUT_DESC desc;
			HRESULT hr{pOutput->GetDesc(&desc)};
			if ( FAILED(hr) )
			{
				Assert( false );
				desc.DeviceName[0] = L'\0';
			}

			char deviceName[std::size(desc.DeviceName)];
			Q_UnicodeToUTF8(desc.DeviceName, deviceName, sizeof(deviceName));

			m_sDisplayDeviceName = deviceName;
		}
	}
	return m_sDisplayDeviceName.Get();
}

bool CShaderDeviceDx10::DetermineHardwareCaps() 
{
	HardwareCaps_t& actualCaps = g_pHardwareConfig->ActualCapsForEdit();
  se::win::com::com_ptr<IDXGIAdapter1> pAdapter{g_ShaderDeviceMgrDx10.GetAdapter(m_DisplayAdapter)};
  se::win::com::com_ptr<IDXGIOutput> pOutput{g_ShaderDeviceMgrDx10.GetAdapterOutput(m_DisplayAdapter)};
  if (!g_ShaderDeviceMgrDx10.ComputeCapsFromD3D(&actualCaps, pAdapter, pOutput))
		return false;

	// See if the file tells us otherwise
	g_ShaderDeviceMgrDx10.ReadDXSupportLevels( actualCaps );

	// Read dxsupport.cfg which has config overrides for particular cards.
	g_ShaderDeviceMgrDx10.ReadHardwareCaps( actualCaps, actualCaps.m_nMaxDXSupportLevel );

	// What's in "-shader" overrides dxsupport.cfg
	const char *pShaderParam = CommandLine()->ParmValue( "-shader" );
	if ( pShaderParam )
	{
		Q_strncpy( actualCaps.m_pShaderDLL, pShaderParam, sizeof( actualCaps.m_pShaderDLL ) );
	}

	return true;
}