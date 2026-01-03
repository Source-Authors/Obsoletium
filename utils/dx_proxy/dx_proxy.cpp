// Copyright Valve Corporation, All rights reserved.
//
// Proxy for D3DX routines

#include "winlite.h"

#include <vector>
#include <d3dcompiler.h>
#include <D3DX9Shader.h>

#ifdef DX_PROXY_EXPORTS
#define SE_DX_PROXY_API extern "C" __declspec(dllexport)
#else
#define SE_DX_PROXY_API extern "C" __declspec(dllimport)
#endif

// Aux function prototype
SE_DX_PROXY_API const char* WINAPI GetDllVersion();

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

// DX11_V00_PC
#ifdef DX11_V00_PC

#ifdef DX_PROXY_INC_CONFIG
#error "DX11_V00_PC: Multiple DX_PROXY configurations disallowed!"
#endif
#define DX_PROXY_INC_CONFIG
#pragma message("Compiling DX_PROXY for DX11_V00_PC")

#endif  // DX11_V00_PC

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
BOOL WINAPI DllMain(HMODULE module, DWORD reason, void*) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      // dimhotepus: No need to notify on thread creation.
      ::DisableThreadLibraryCalls(module);
      // Process is attaching - make sure it can find the dependencies
      return ExtractDependencies();
  }

  return TRUE;
}

// Obtain DLL version
SE_DX_PROXY_API const char* WINAPI GetDllVersionLong() {
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

#if defined(DX11_V00_PC) && defined(_DEBUG)
  return "{DX_PROXY for DX11_V00_PC DEBUG}";
#endif

#if defined(DX11_V00_PC) && defined(NDEBUG)
  return "{DX_PROXY for DX11_V00_PC RELEASE}";
#endif
}

SE_DX_PROXY_API const char* WINAPI GetDllVersion() {
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

#if defined(DX11_V00_PC) && defined(_DEBUG)
  return "DXPRX_DX11_V00_PC_d";
#endif

#if defined(DX11_V00_PC) && defined(NDEBUG)
  return "DXPRX_DX11_V00_PC_r";
#endif
}

#include "filememcache.h"
#include "dxincludeimpl.h"

#if defined(DX9_V00_PC) || defined(DX9_V30_PC)

namespace {

// DirectX 11 -> DirectX 9 wrappers.

class D3DXInclude : public ID3DXInclude {
 public:
  D3DXInclude(ID3DInclude* inner) noexcept : m_inner{inner} {}

  STDMETHODIMP Open(D3DXINCLUDE_TYPE IncludeType, LPCSTR pFileName,
                    LPCVOID pParentData, LPCVOID* ppData,
                    UINT* pBytes) override {
    return m_inner->Open(GetD3dIncludeType(IncludeType), pFileName, pParentData,
                         ppData, pBytes);
  }
  STDMETHODIMP Close(LPCVOID pData) override { return m_inner->Close(pData); }

 private:
  ID3DInclude* m_inner;

  static D3D_INCLUDE_TYPE GetD3dIncludeType(
      D3DXINCLUDE_TYPE dx_include) noexcept {
    switch (dx_include) {
      case D3DXINC_LOCAL:
        return D3D_INCLUDE_LOCAL;
      case D3DXINC_SYSTEM:
        return D3D_INCLUDE_SYSTEM;
      default:
        // Failed, return code which should cause failure later.
        return D3D_INCLUDE_FORCE_DWORD;
    }
  }
};

// Lifetime of ID3DXBuffer is tied to D3DBlob.  ID3DXBuffer counter is
// already 1.
class D3DBlob : public ID3DBlob {
 public:
  D3DBlob(ID3DXBuffer* buffer) noexcept : m_buffer{buffer} {}
  ~D3DBlob() noexcept = default;

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID iid, LPVOID* ppv) override {
    return m_buffer->QueryInterface(iid, ppv);
  }
  STDMETHODIMP_(ULONG) AddRef() override { return m_buffer->AddRef(); }
  STDMETHODIMP_(ULONG) Release() override {
    const ULONG rc = m_buffer->Release();
    if (rc == 0) delete this;
    return rc;
  }

  // ID3DXBuffer
  STDMETHODIMP_(LPVOID) GetBufferPointer() override {
    return m_buffer->GetBufferPointer();
  }
  STDMETHODIMP_(SIZE_T) GetBufferSize() override {
    return m_buffer->GetBufferSize();
  }

 private:
  ID3DXBuffer* m_buffer;
};

DWORD GetD3dxFlagsFromD3dOnes(UINT d3dFlags1, UINT d3dFlags2) noexcept {
  DWORD d3dx_flags{0};

  if (d3dFlags1 & D3DCOMPILE_DEBUG) {
    d3dx_flags |= D3DXSHADER_DEBUG;
  }

  if (d3dFlags1 & D3DCOMPILE_SKIP_VALIDATION) {
    d3dx_flags |= D3DXSHADER_SKIPVALIDATION;
  }

  if (d3dFlags1 & D3DCOMPILE_SKIP_OPTIMIZATION) {
    d3dx_flags |= D3DXSHADER_SKIPOPTIMIZATION;
  }

  if (d3dFlags1 & D3DCOMPILE_PACK_MATRIX_ROW_MAJOR) {
    d3dx_flags |= D3DXSHADER_PACKMATRIX_ROWMAJOR;
  }

  if (d3dFlags1 & D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR) {
    d3dx_flags |= D3DXSHADER_PACKMATRIX_COLUMNMAJOR;
  }

  if (d3dFlags1 & D3DCOMPILE_PARTIAL_PRECISION) {
    d3dx_flags |= D3DXSHADER_PARTIALPRECISION;
  }

  if (d3dFlags1 & D3DCOMPILE_FORCE_VS_SOFTWARE_NO_OPT) {
    // This flag was applicable only to Direct3D 9.
    d3dx_flags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
  }

  if (d3dFlags1 & D3DCOMPILE_FORCE_PS_SOFTWARE_NO_OPT) {
    // This flag was applicable only to Direct3D 9.
    d3dx_flags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
  }

  if (d3dFlags1 & D3DCOMPILE_NO_PRESHADER) {
    // This flag was only applicable to legacy Direct3D 9 and Direct3D 10
    // Effects (FX).
    d3dx_flags |= D3DXSHADER_NO_PRESHADER;
  }

  if (d3dFlags1 & D3DCOMPILE_AVOID_FLOW_CONTROL) {
    d3dx_flags |= D3DXSHADER_AVOID_FLOW_CONTROL;
  }

  if (d3dFlags1 & D3DCOMPILE_IEEE_STRICTNESS) {
    d3dx_flags |= D3DXSHADER_IEEE_STRICTNESS;
  }

  if (d3dFlags1 & D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY) {
    d3dx_flags |= D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY;
  }

  if (d3dFlags1 & D3DCOMPILE_OPTIMIZATION_LEVEL0) {
    d3dx_flags |= D3DXSHADER_OPTIMIZATION_LEVEL0;
  }

  if (d3dFlags1 & D3DCOMPILE_OPTIMIZATION_LEVEL1) {
    d3dx_flags |= D3DXSHADER_OPTIMIZATION_LEVEL1;
  }

  if (d3dFlags1 & D3DCOMPILE_OPTIMIZATION_LEVEL2) {
    d3dx_flags |= D3DXSHADER_OPTIMIZATION_LEVEL2;
  }

  if (d3dFlags1 & D3DCOMPILE_OPTIMIZATION_LEVEL3) {
    d3dx_flags |= D3DXSHADER_OPTIMIZATION_LEVEL3;
  }

  // These have no D3DX equivalents.
  // D3DCOMPILE_ENABLE_STRICTNESS
  // D3DCOMPILE_WARNINGS_ARE_ERRORS
  // D3DCOMPILE_RESOURCES_MAY_ALIAS
  // D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES
  // D3DCOMPILE_ALL_RESOURCES_BOUND
  // D3DCOMPILE_DEBUG_NAME_FOR_SOURCE
  // D3DCOMPILE_DEBUG_NAME_FOR_BINARY

  return d3dx_flags;
}

bool IsTargetEqual(const char* target, const char* expected) noexcept {
  return strcmp(target, expected) == 0;
}

}  // namespace

#endif  // defined(DX9_V00_PC) || defined(DX9_V30_PC)

// Proxied routines
SE_DX_PROXY_API HRESULT WINAPI Proxy_D3DCompileFromFile(
    LPCSTR pFileName, CONST D3D_SHADER_MACRO* pDefines, ID3DInclude* include,
    LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2,
    ID3DBlob** ppCode, ID3DBlob** ppErrorMsgs) {
  if (!pTarget || !ppCode) return E_POINTER;

  static se::dx_proxy::D3DIncludeImpl include_impl;

  if (!include) include = &include_impl;

  // Open the top-level file via our include interface.
  // It just caches shader file inside to reduce I/O.
  const void* data;
  UINT numBytes;

  HRESULT hr{
      include->Open(D3D_INCLUDE_LOCAL, pFileName, nullptr, &data, &numBytes)};
  if (FAILED(hr)) return hr;

  RunCodeAtScopeExit(include->Close(data));

#if defined(DX9_V00_PC) || defined(DX9_V30_PC)
  // The current HLSL shader D3DCompile* functions don't support legacy 1.x
  // pixel shaders.  The last version of HLSL to support these targets was D3DX9
  // in the October 2006 release of the DirectX SDK.
  if (!IsTargetEqual(pTarget, "ps_1_1") && !IsTargetEqual(pTarget, "ps_1_2") &&
      !IsTargetEqual(pTarget, "ps_1_3") && !IsTargetEqual(pTarget, "ps_1_4")) {
    hr = D3DCompile(data, numBytes, pFileName, pDefines, include, pEntrypoint,
                    pTarget, Flags1, Flags2, ppCode, ppErrorMsgs);
  } else {
    static_assert(sizeof(*pDefines) == sizeof(D3DXMACRO),
                  "Ensure D3D_SHADER_MACRO and D3DXMACRO are same size.");
    static_assert(alignof(decltype(pDefines)) == alignof(D3DXMACRO),
                  "Ensure D3D_SHADER_MACRO and D3DXMACRO are same alignment.");

    D3DXInclude d3dx_include_wrapper{include};
    ID3DXInclude* d3dx_include{
        include != nullptr && include != D3D_COMPILE_STANDARD_FILE_INCLUDE
            ? &d3dx_include_wrapper
            : nullptr};

    ID3DXBuffer *d3dx_shader{nullptr}, *d3dx_errors{nullptr};

    hr = D3DXCompileShader(static_cast<const char*>(data), numBytes,
                           reinterpret_cast<const D3DXMACRO*>(pDefines),
                           d3dx_include, pEntrypoint, pTarget,
                           GetD3dxFlagsFromD3dOnes(Flags1, Flags2),
                           &d3dx_shader, &d3dx_errors, nullptr);
    if (SUCCEEDED(hr)) {
      *ppCode = new D3DBlob{d3dx_shader};

      if (ppErrorMsgs) *ppErrorMsgs = new D3DBlob{d3dx_errors};
    } else {
      *ppCode = nullptr;

      if (ppErrorMsgs) *ppErrorMsgs = nullptr;
    }
  }
#else
  hr = D3DCompile(data, numBytes, pFileName, pDefines, include, pEntrypoint,
                  pTarget, Flags1, Flags2, ppCode, ppErrorMsgs);
#endif  // defined(DX9_V00_PC) || defined(DX9_V30_PC)

  return hr;
}
