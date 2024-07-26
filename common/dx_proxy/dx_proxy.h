// Copyright Valve Corporation, All rights reserved.
//
// Make dynamic loading of dx_proxy.dll and methods acquisition easier.

#ifndef SRC_COMMON_DX_PROXY_H_
#define SRC_COMMON_DX_PROXY_H_

namespace se::dx_proxy {

/**
 * @brief Uses a lazy-load technique to load the dx_proxy.dll module and acquire
 * the function pointers.  The dx_proxy.dll module is automatically unloaded
 * during destruction.
 */
class DxProxyModule {
 public:
  /// Construction
  DxProxyModule();
  /// Destruction
  ~DxProxyModule();

  DxProxyModule(const DxProxyModule&) = delete;
  DxProxyModule& operator=(const DxProxyModule&) = delete;

  /// Loads the module and acquires function pointers, returns if the module was
  /// loaded successfully.  If the module was already loaded the call has no
  /// effect and returns TRUE.
  BOOL Load();
  /// Frees the loaded module.
  void Free();

 private:
  enum class Function { D3DCompileFromFile = 0, Total };

  //!< The handle of the loaded dx_proxy.dll
  HMODULE m_hModule;

  //!< The array of loaded function pointers
  FARPROC m_arrFuncs[to_underlying(Function::Total)];

  // Requested function names array
  static inline const char* arrFuncNames[to_underlying(Function::Total)] = {
      "Proxy_D3DCompileFromFile"};

  /// Interface functions calling into DirectX proxy
 public:
  HRESULT D3DCompileFromFile(LPCSTR pFileName, const D3D_SHADER_MACRO* pDefines,
                             ID3DInclude* pInclude, LPCSTR pEntrypoint,
                             LPCSTR pTarget, UINT Flags1, UINT Flags2,
                             ID3DBlob** ppCode, ID3DBlob** ppErrorMsgs);
};

inline DxProxyModule::DxProxyModule() {
  m_hModule = nullptr;
  memset(m_arrFuncs, 0, sizeof(m_arrFuncs));
}

inline DxProxyModule::~DxProxyModule() { Free(); }

inline BOOL DxProxyModule::Load() {
  if (m_hModule == nullptr &&
      (m_hModule = ::LoadLibrary("dx_proxy.dll")) != nullptr) {
    // Acquire the functions
    for (ptrdiff_t k{0}; k < ssize(m_arrFuncs); ++k) {
      m_arrFuncs[k] = ::GetProcAddress(m_hModule, arrFuncNames[k]);
    }
  }

  return !!m_hModule;
}

inline void DxProxyModule::Free() {
  if (m_hModule) {
    ::FreeLibrary(m_hModule);

    m_hModule = nullptr;

    memset(m_arrFuncs, 0, sizeof(m_arrFuncs));
  }
}

inline HRESULT DxProxyModule::D3DCompileFromFile(
    LPCSTR pFileName, const D3D_SHADER_MACRO* pDefines, ID3DInclude* pInclude,
    LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2,
    ID3DBlob** ppCode, ID3DBlob** ppErrorMsgs) {
  if (!Load()) return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 1);

  if (!m_arrFuncs[to_underlying(Function::D3DCompileFromFile)])
    return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 2);

  HRESULT WINAPI D3DCompileFromFileProxy(
      LPCSTR pFileName, CONST D3D_SHADER_MACRO * pDefines,
      ID3DInclude * pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1,
      UINT Flags2, ID3DBlob * *ppCode, ID3DBlob * *ppErrorMsgs);

  using D3DCompileFromFileProxyFunction = decltype(&D3DCompileFromFileProxy);
  const auto proxy = reinterpret_cast<D3DCompileFromFileProxyFunction>(
      m_arrFuncs[to_underlying(Function::D3DCompileFromFile)]);

  return proxy(pFileName, pDefines, pInclude, pEntrypoint, pTarget, Flags1,
               Flags2, ppCode, ppErrorMsgs);
}

}  // namespace se::dx_proxy

#endif  // !SRC_COMMON_DX_PROXY_H_
