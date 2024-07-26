// Copyright Valve Corporation, All rights reserved.
//
// Proxy for D3DX routines

#include "winlite.h"

#include <vector>
#include <d3dcompiler.h>

#ifdef DX_PROXY_EXPORTS
#define SRC_DX_PROXY_API extern "C" __declspec(dllexport)
#else
#define SRC_DX_PROXY_API extern "C" __declspec(dllexport)
#endif

// Aux function prototype
SRC_DX_PROXY_API const char* WINAPI GetDllVersion();

#ifdef _DEBUG
#define D3D_DEBUG_INFO 1
#endif

// DX9_V00_PC
//
// D3DX static library
#ifdef DX9_V00_PC

#ifdef DX_PROXY_INC_CONFIG
#error "DX9_V00_PC: Multiple DX_PROXY configurations disallowed!"
#endif
#define DX_PROXY_INC_CONFIG
#pragma message("Compiling DX_PROXY for DX9_V00_PC")

#endif  // DX9_V00_PC

// DX9_V30_PC
#ifdef DX9_V30_PC

#ifdef DX_PROXY_INC_CONFIG
#error "DX9_V30_PC: Multiple DX_PROXY configurations disallowed!"
#endif
#define DX_PROXY_INC_CONFIG
#pragma message("Compiling DX_PROXY for DX9_V30_PC")

#endif  // DX9_V30_PC

// DX10_V00_PC
#ifdef DX10_V00_PC

#ifdef DX_PROXY_INC_CONFIG
#error "DX10_V00_PC: Multiple DX_PROXY configurations disallowed!"
#endif
#define DX_PROXY_INC_CONFIG
#pragma message("Compiling DX_PROXY for DX10_V00_PC")

#endif  // DX10_V00_PC

//
// No DX configuration
#ifndef DX_PROXY_INC_CONFIG
#error "DX9_PC must be defined!"
#endif  // DX_PROXY_INC_CONFIG

// Retrieves all the additional required binaries from the resources and
// places them to a temporary location.  Then the binaries are mapped into
// the address space of the calling process.
static BOOL ExtractDependencies() { return TRUE; }

// DLL entry point: DllMain
BOOL WINAPI DllMain(HINSTANCE, DWORD reason, void*) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      // Process is attaching - make sure it can find the dependencies
      return ExtractDependencies();
  }

  return TRUE;
}

// Obtain DLL version
SRC_DX_PROXY_API const char* WINAPI GetDllVersionLong() {
#if defined(DX9_V00_PC) && defined(_DEBUG)
  return "{DX_PROXY for DX9_V00_PC DEBUG}";
#endif

#if defined(DX9_V00_PC) && defined(NDEBUG)
  return "{DX_PROXY for DX9_V00_PC RELEASE}";
#endif

#if defined(DX9_V30_PC) && defined(_DEBUG)
  return "{DX_PROXY for DX9_V30_PC DEBUG}";
#endif

#if defined(DX9_V30_PC) && defined(NDEBUG)
  return "{DX_PROXY for DX9_V30_PC RELEASE}";
#endif

#if defined(DX10_V00_PC) && defined(_DEBUG)
  return "{DX_PROXY for DX10_V00_PC DEBUG}";
#endif

#if defined(DX10_V00_PC) && defined(NDEBUG)
  return "{DX_PROXY for DX10_V00_PC RELEASE}";
#endif
}

SRC_DX_PROXY_API const char* WINAPI GetDllVersion() {
#if defined(DX9_V00_PC) && defined(_DEBUG)
  return "DXPRX_DX9_V00_PC_d";
#endif

#if defined(DX9_V00_PC) && defined(NDEBUG)
  return "DXPRX_DX9_V00_PC_r";
#endif

#if defined(DX9_V30_PC) && defined(_DEBUG)
  return "DXPRX_DX9_V30_PC_d";
#endif

#if defined(DX9_V30_PC) && defined(NDEBUG)
  return "DXPRX_DX9_V30_PC_r";
#endif

#if defined(DX10_V00_PC) && defined(_DEBUG)
  return "DXPRX_DX10_V00_PC_d";
#endif

#if defined(DX10_V00_PC) && defined(NDEBUG)
  return "DXPRX_DX10_V00_PC_r";
#endif
}

#include "filememcache.h"
#include "dxincludeimpl.h"

// Proxied routines
SRC_DX_PROXY_API HRESULT WINAPI Proxy_D3DCompileFromFile(
    LPCSTR pFileName, CONST D3D_SHADER_MACRO* pDefines, ID3DInclude* include,
    LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2,
    ID3DBlob** ppCode, ID3DBlob** ppErrorMsgs) {
  static se::dx_proxy::D3DIncludeImpl include_impl;

  if (!include) include = &include_impl;

  // Open the top-level file via our include interface.
  // It just caches shader file inside to reduce I/O.
  const void* data;
  UINT numBytes;

  HRESULT hr{
      include->Open(D3D_INCLUDE_LOCAL, pFileName, nullptr, &data, &numBytes)};
  if (FAILED(hr)) return hr;

  hr = D3DCompile(data, numBytes, pFileName, pDefines, include, pEntrypoint,
                  pTarget, Flags1, Flags2, ppCode, ppErrorMsgs);

  // Close the file
  include->Close(data);

  return hr;
}
