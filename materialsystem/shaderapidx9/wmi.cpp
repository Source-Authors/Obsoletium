//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: This method queries the DXGI interfaces to determine the amount of
// video memory. On a discrete video card, this is often close to the amount of
// dedicated video memory.
//
//=============================================================================

#include "resource.h"

// Avoid conflicts with MSVC headers and memdbgon.h
#undef PROTECTED_THINGS_ENABLE
#include "tier0/dbg.h"

#include <dxgi.h>
#include "com_ptr.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "comsuppw.lib")

size_t GetVidMemBytes( unsigned nAdapter )
{
  static unsigned nBeenAdapter = UINT_MAX;
  static size_t nBytes = 0;

	if ( nBeenAdapter != UINT_MAX ) return nBytes;

	nBeenAdapter = nAdapter;
	
	// Initialize COM
	HRESULT hr{CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY )};
	if ( FAILED(hr) )
	{
		Warning( "Unable to get GPU VRAM size: COM initialization failure.\n" );
		return 0;
	}

	// Windows 7+.
	se::win::com::com_ptr<IDXGIFactory1> pDXGIFactory;
	hr = CreateDXGIFactory1( IID_PPV_ARGS(&pDXGIFactory) );
	if ( FAILED( hr ) )
	{
		Warning( "Failed to create the DXGI Factory #1!\n" );
		return 0;
	}

	se::win::com::com_ptr<IDXGIAdapter1> pAdapter;
	hr = pDXGIFactory->EnumAdapters1( nAdapter, &pAdapter );
	if ( FAILED( hr ) )
	{
		Warning( "Failed to enum DXGI adapters!\n" );
		return 0;
	}

	DXGI_ADAPTER_DESC1 desc;
	hr = pAdapter->GetDesc1( &desc );
	if ( FAILED( hr ) )
	{
		Warning( "Failed to get DXGI adapter #%u desc!\n", nAdapter );
		return 0;
	}

	nBytes = SUCCEEDED(hr) ? desc.DedicatedVideoMemory : 0;

	// Cleanup
	CoUninitialize();

	return nBytes;
}
