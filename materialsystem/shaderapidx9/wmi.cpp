//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: This method queries the Windows Management Instrumentation (WMI) interfaces 
// to determine the amount of video memory. On a discrete video card, this is 
// often close to the amount of dedicated video memory and usually does not take 
// into account the amount of shared system memory. 
// 
// See https://github.com/walbourn/directx-sdk-samples/blob/main/VideoMemory/VidMemViaWMI.cpp
//
//=============================================================================

#include "resource.h"

// Avoid conflicts with MSVC headers and memdbgon.h
#undef PROTECTED_THINGS_ENABLE
#include "tier0/basetypes.h"
#include "tier0/dbg.h"

#define _WIN32_DCOM
#include <comdef.h>
#include <wbemidl.h>

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#pragma comment(lib, "wbemuuid.lib")

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=nullptr; } }
#endif

uint64 GetVidMemBytes()
{
	static int bBeenHere = false;
	static uint64 nBytes = 0;

	if( bBeenHere )
	{
		return nBytes;
	}

	bBeenHere = true;

	// Initialize COM
	HRESULT hr = CoInitialize( nullptr );
	if ( FAILED(hr) ) [[unlikely]]
	{
		Warning( "Unable to get GPU VRAM size: COM initialization failure.\n" );
		return 0;
	}

	// Obtain the initial locator to WMI
	IWbemLocator *pLoc = nullptr;
	hr = CoCreateInstance( CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
		IID_IWbemLocator, (LPVOID *) &pLoc);
 
	if ( FAILED( hr ) || !pLoc ) [[unlikely]]
	{
		Warning( "Unable to get GPU VRAM size: Wbem locator failure.\n" );
		CoUninitialize();
		return 0;
	}

	// Connect to WMI through the IWbemLocator::ConnectServer method
	IWbemServices *pSvc = nullptr;
	// Connect to the root\cimv2 namespace with the current user and obtain pointer pSvc
	// to make IWbemServices calls.
	hr = pLoc->ConnectServer(
				_bstr_t(L"\\\\.\\root\\cimv2"), // Object path of WMI namespace
				nullptr,                    // User name. NULL = current user
				nullptr,                    // User password. NULL = current
				nullptr,                    // Locale. NULL indicates current
				0L,                         // Security flags.
				nullptr,                    // Authority (e.g. Kerberos)
				nullptr,                    // Context object 
				&pSvc                       // pointer to IWbemServices proxy
				);

	if ( FAILED( hr ) )
	{
		Warning( "Unable to get GPU VRAM size: WMI server failure.\n" );
		SAFE_RELEASE( pLoc );
		CoUninitialize();
		return 0;
	}

	// Set security levels on the proxy
	hr = CoSetProxyBlanket(
			pSvc,                        // Indicates the proxy to set
			RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
			RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
			nullptr,                     // Server principal name 
			RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
			RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
			nullptr,                     // client identity
			EOAC_NONE                    // proxy capabilities 
	);

	if ( FAILED( hr ) )
	{
		Warning( "Unable to get GPU VRAM size: Proxy security adjustment failure.\n" );
		SAFE_RELEASE( pSvc );
		SAFE_RELEASE( pLoc );
		CoUninitialize();
		return 0;
	}

	// Use the IWbemServices pointer to make requests of WMI

	IEnumWbemClassObject *pEnumerator = nullptr;
	hr = pSvc->ExecQuery( bstr_t("WQL"), bstr_t("SELECT AdapterRAM FROM Win32_VideoController"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator);

	if ( FAILED( hr ) || !pEnumerator )
	{
		Warning("Unable to get GPU VRAM size: Win32_VideoController query failure.\n");
		SAFE_RELEASE(pSvc);
		SAFE_RELEASE(pLoc);
		CoUninitialize();
		return 0;
	}


	// Get the data from the above query
	IWbemClassObject *pclsObj = nullptr;
	ULONG uReturn = 0;

	while ( true )
	{
		// 500 ms timeout, 3 retries.
		HRESULT hr = pEnumerator->Next( 500, 3, &pclsObj, &uReturn );
		if ( FAILED( hr ) || 0 == uReturn )
		{
				break;
		}

		variant_t adapterRamBytes;

		// Pluck a series of properties out of the query from Win32_VideoController

		hr = pclsObj->Get( L"AdapterRAM", 0, &adapterRamBytes, nullptr, nullptr );
		if ( SUCCEEDED( hr ) )
		{
			// Video RAM in bytes, AdapterRAM is returned as the I4 type so we read
			// it out as unsigned int, see
			// https://docs.microsoft.com/en-us/windows/win32/cimwin32prov/win32-videocontroller
			nBytes = static_cast<unsigned int>(adapterRamBytes);
		}

		SAFE_RELEASE(pclsObj);
	}

	// Cleanup
	SAFE_RELEASE(pSvc);
	SAFE_RELEASE(pLoc);
	SAFE_RELEASE(pEnumerator);
	SAFE_RELEASE(pclsObj);
	CoUninitialize();

	return nBytes;
}
