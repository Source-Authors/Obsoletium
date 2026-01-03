// Copyright Valve Corporation, All rights reserved.

#ifndef SE_MATERIALSYSTEM_SHADERAPIDX11_SHADER_DEVICE_DX11_H_
#define SE_MATERIALSYSTEM_SHADERAPIDX11_SHADER_DEVICE_DX11_H_

#include "tier1/utlvector.h"
#include "tier1/utlrbtree.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlstring.h"

#include "shaderdevicebase.h"

#define NOMINMAX
#include <string_view>
#include <d3d11_4.h>
#include <d3d11shader.h>
#include <dxgi1_6.h>
#include "com_ptr.h"

namespace se::shaderapidx11 {

// Shader device manager on DirectX 11 hardware.
class CShaderDeviceMgrDx11 final : public CShaderDeviceMgrBase {
  using BaseClass = CShaderDeviceMgrBase;

 public:
  CShaderDeviceMgrDx11();
  ~CShaderDeviceMgrDx11();

  // Methods of IAppSystem
  bool Connect(CreateInterfaceFn factory) override;
  void Disconnect() override;
  InitReturnVal_t Init() override;
  void Shutdown() override;

  // Methods of IShaderDeviceMgr
  unsigned GetAdapterCount() const override;
  void GetAdapterInfo(unsigned adapter_no,
                      MaterialAdapterInfo_t &info) const override;
  unsigned GetModeCount(unsigned adapter_no) const override;
  void GetModeInfo(ShaderDisplayMode_t *info, unsigned adapter_no,
                   unsigned mode_no) const override;
  void GetCurrentModeInfo(ShaderDisplayMode_t *info,
                          unsigned adapter_no) const override;
  bool SetAdapter(unsigned adapter_no, int flags) override;

  CreateInterfaceFn SetMode(void *hwnd, unsigned adapter_no,
                            const ShaderDeviceInfo_t &mode);

 private:
  // Initialize adapter information
  bool InitGpuInfo();

  // Determines hardware caps from D3D
  bool ComputeCapsFromD3D(HardwareCaps_t *caps, IDXGIAdapter4 *adapter,
                          IDXGIOutput6 *output);

  // Returns the amount of video memory in bytes for a particular adapter
  size_t GetVidMemBytes(unsigned adapter_no) const;

  // Returns the appropriate adapter output to use
  win::com::com_ptr<IDXGIOutput6> GetAdapterOutput(unsigned adapter_no) const;

  // Returns the adapter interface for a particular adapter
  win::com::com_ptr<IDXGIAdapter4> GetAdapter(unsigned adapter_no) const;

  // Used to enumerate adapters, attach to windows
  win::com::com_ptr<IDXGIFactory6> m_dxgi_factory;

  bool m_use_dx_level_from_command_line : 1;

  friend class CShaderDeviceDx11;

  // Enum adapters in order for xGPU to iGPU.
  constexpr inline static DXGI_GPU_PREFERENCE KDxgiGpuPreference{
      DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE};
  // Desired resource data format.
  constexpr inline static DXGI_FORMAT kDxgiAdapterResourceFormat{
      DXGI_FORMAT_B8G8R8A8_UNORM};
  // desired scanline order and/or scaling.
  constexpr inline static UINT kDxgiAdapterEnumFlags{
      DXGI_ENUM_MODES_INTERLACED};
};

// Shader device on DirectX 11 hardware.
class CShaderDeviceDx11 final : public CShaderDeviceBase {
 public:
  CShaderDeviceDx11();
  ~CShaderDeviceDx11();

 public:
  // Methods of IShaderDevice
  bool IsUsingGraphics() const override;
  unsigned GetCurrentAdapter() const override;
  ImageFormat GetBackBufferFormat() const override;
  void GetBackBufferDimensions(int &width, int &height) const override;
  void SpewDriverInfo() const override;
  void Present() override;
  IShaderBuffer *CompileShader(const char *pProgram, size_t nBufLen,
                               const char *pShaderVersion) override;
  VertexShaderHandle_t CreateVertexShader(IShaderBuffer *pShader) override;
  void DestroyVertexShader(VertexShaderHandle_t hShader) override;
  GeometryShaderHandle_t CreateGeometryShader(
      IShaderBuffer *pShaderBuffer) override;
  void DestroyGeometryShader(GeometryShaderHandle_t hShader) override;
  PixelShaderHandle_t CreatePixelShader(IShaderBuffer *pShaderBuffer) override;
  void DestroyPixelShader(PixelShaderHandle_t hShader) override;
  void ReleaseResources() override {}
  void ReacquireResources() override {}
  IMesh *CreateStaticMesh(VertexFormat_t format,
                          const char *pTextureBudgetGroup,
                          IMaterial *pMaterial) override;
  void DestroyStaticMesh(IMesh *mesh) override;
  IVertexBuffer *CreateVertexBuffer(ShaderBufferType_t type, VertexFormat_t fmt,
                                    int nVertexCount,
                                    const char *pTextureBudgetGroup) override;
  void DestroyVertexBuffer(IVertexBuffer *pVertexBuffer) override;
  IIndexBuffer *CreateIndexBuffer(ShaderBufferType_t type,
                                  MaterialIndexFormat_t fmt, int nIndexCount,
                                  const char *pTextureBudgetGroup) override;
  void DestroyIndexBuffer(IIndexBuffer *pIndexBuffer) override;
  IVertexBuffer *GetDynamicVertexBuffer(int nStreamID,
                                        VertexFormat_t vertexFormat,
                                        bool bBuffered = true) override;
  IIndexBuffer *GetDynamicIndexBuffer(MaterialIndexFormat_t fmt,
                                      bool bBuffered = true) override;
  void SetHardwareGammaRamp(float fGamma, float fGammaTVRangeMin,
                            float fGammaTVRangeMax, float fGammaTVExponent,
                            bool bTVEnabled) override;

  // A special path used to tick the front buffer while loading on the 360
  void EnableNonInteractiveMode(MaterialNonInteractiveMode_t,
                                ShaderNonInteractiveInfo_t *) override {}
  void RefreshFrontBufferNonInteractive() override {}
  void HandleThreadEvent(uint32) override {}

  const char *GetDisplayDeviceName() override;

  HullShaderHandle_t CreateHullShader(IShaderBuffer *pShaderBuffer) override;
  void DestroyHullShader(HullShaderHandle_t hShader) override;
  DomainShaderHandle_t CreateDomainShader(
      IShaderBuffer *pShaderBuffer) override;
  void DestroyDomainShader(DomainShaderHandle_t hShader) override;
  ComputeShaderHandle_t CreateComputeShader(
      IShaderBuffer *pShaderBuffer) override;
  void DestroyComputeShader(ComputeShaderHandle_t hShader) override;

 public:
  // Methods of CShaderDeviceBase
  bool InitDevice(void *hwnd, unsigned nAdapter,
                  const ShaderDeviceInfo_t &mode) override;
  void ShutdownDevice() override;
  bool IsDeactivated() const override { return false; }
  bool DetermineHardwareCaps();

  // Other public methods
  ID3D11VertexShader *GetVertexShader(VertexShaderHandle_t hShader) const;
  ID3D11GeometryShader *GetGeometryShader(GeometryShaderHandle_t hShader) const;
  ID3D11PixelShader *GetPixelShader(PixelShaderHandle_t hShader) const;
  ID3D11HullShader *GetHullShader(HullShaderHandle_t hShader) const;
  ID3D11DomainShader *GetDomainShader(DomainShaderHandle_t hShader) const;
  ID3D11ComputeShader *GetComputeShader(ComputeShaderHandle_t hShader) const;
  ID3D11InputLayout *GetInputLayout(VertexShaderHandle_t hShader,
                                    VertexFormat_t format);

  inline win::com::com_ptr<ID3D11Device5> D3D11Device() {
    return m_d3d11_device;
  }

  inline win::com::com_ptr<ID3D11DeviceContext4> D3D11DeviceContext() {
    return m_d3d11_immediate_context;
  }

  inline win::com::com_ptr<ID3D11RenderTargetView1> D3D11RenderTargetView() {
    return m_d3d11_render_target_view;
  }

  void SetAdapterAndDriver(unsigned adapter_no, D3D_DRIVER_TYPE driver_type) {
    m_dxgi_adapter_index = adapter_no;
    m_d3d_driver_type = driver_type;
  }

 private:
  struct InputLayout_t {
    se::win::com::com_ptr<ID3D11InputLayout> m_pInputLayout;
    VertexFormat_t m_VertexFormat;
  };

  using InputLayoutDict_t = CUtlRBTree<InputLayout_t, unsigned short>;

  static bool InputLayoutLessFunc(const InputLayout_t &lhs,
                                  const InputLayout_t &rhs) {
    return lhs.m_VertexFormat < rhs.m_VertexFormat;
  }

  template <typename TShader>
  struct BaseShader_t {
    using interface_concept = se::win::com::com_interface_concept<TShader>;

    se::win::com::com_ptr<TShader> m_pShader;
    se::win::com::com_ptr<ID3D11ShaderReflection> m_pInfo;
  };

  struct VertexShader_t : public BaseShader_t<ID3D11VertexShader> {
    unsigned char *m_pByteCode;
    size_t m_nByteCodeLen;
    InputLayoutDict_t m_InputLayouts;

    VertexShader_t()
        : m_pByteCode{nullptr},
          m_nByteCodeLen{0},
          m_InputLayouts(0, 0, InputLayoutLessFunc) {}
  };

  struct GeometryShader_t : public BaseShader_t<ID3D11GeometryShader> {};

  struct PixelShader_t : public BaseShader_t<ID3D11PixelShader> {};

  struct HullShader_t : public BaseShader_t<ID3D11HullShader> {};

  struct DomainShader_t : public BaseShader_t<ID3D11DomainShader> {};

  struct ComputeShader_t : public BaseShader_t<ID3D11ComputeShader> {};

  using VertexShaderIndex_t = CUtlFixedLinkedList<VertexShader_t>::IndexType_t;
  using GeometryShaderIndex_t =
      CUtlFixedLinkedList<GeometryShader_t>::IndexType_t;
  using PixelShaderIndex_t = CUtlFixedLinkedList<PixelShader_t>::IndexType_t;
  using HullShaderIndex_t = CUtlFixedLinkedList<HullShader_t>::IndexType_t;
  using DomainShaderIndex_t = CUtlFixedLinkedList<DomainShader_t>::IndexType_t;
  using ComputeShaderIndex_t =
      CUtlFixedLinkedList<ComputeShader_t>::IndexType_t;

  void ReleaseInputLayouts(VertexShaderIndex_t nIndex);

  win::com::com_ptr<IDXGIOutput6> m_dxgi_output;
  win::com::com_ptr<ID3D11Device5> m_d3d11_device;
  win::com::com_ptr<ID3D11DeviceContext4> m_d3d11_immediate_context;
  win::com::com_ptr<IDXGISwapChain4> m_dxgi_swap_chain;
  win::com::com_ptr<ID3D11RenderTargetView1> m_d3d11_render_target_view;

  CUtlFixedLinkedList<VertexShader_t> m_VertexShaderDict;
  CUtlFixedLinkedList<GeometryShader_t> m_GeometryShaderDict;
  CUtlFixedLinkedList<PixelShader_t> m_PixelShaderDict;
  CUtlFixedLinkedList<HullShader_t> m_HullShaderDict;
  CUtlFixedLinkedList<DomainShader_t> m_DomainShaderDict;
  CUtlFixedLinkedList<ComputeShader_t> m_ComputeShaderDict;

  // Current display device name.
  CUtlString m_display_device_name;
  // DXGI swap chaing description.
  DXGI_SWAP_CHAIN_DESC m_dxgi_swap_chain_desc;

  // Current display adapter index.
  UINT m_dxgi_adapter_index;
  // DirectX driver type.
  D3D_DRIVER_TYPE m_d3d_driver_type;
  // Feature level supported by the DirectX 11 device.
  D3D_FEATURE_LEVEL m_d3d_feature_level;

  // Present sync interval.
  unsigned m_present_sync_interval;
  // Present flags.
  unsigned m_present_flags;
  // Rendering is occluded by another window.
  bool m_is_rendering_occluded;
};

// Inline methods of CShaderDeviceDx11
inline ID3D11VertexShader *CShaderDeviceDx11::GetVertexShader(
    VertexShaderHandle_t hShader) const {
  if (hShader != VERTEX_SHADER_HANDLE_INVALID)
    return m_VertexShaderDict[(VertexShaderIndex_t)hShader].m_pShader;
  return NULL;
}

inline ID3D11GeometryShader *CShaderDeviceDx11::GetGeometryShader(
    GeometryShaderHandle_t hShader) const {
  if (hShader != GEOMETRY_SHADER_HANDLE_INVALID)
    return m_GeometryShaderDict[(GeometryShaderIndex_t)hShader].m_pShader;
  return NULL;
}

inline ID3D11PixelShader *CShaderDeviceDx11::GetPixelShader(
    PixelShaderHandle_t hShader) const {
  if (hShader != PIXEL_SHADER_HANDLE_INVALID)
    return m_PixelShaderDict[(PixelShaderIndex_t)hShader].m_pShader;
  return NULL;
}

inline ID3D11HullShader *CShaderDeviceDx11::GetHullShader(
    HullShaderHandle_t hShader) const {
  if (hShader != HULL_SHADER_HANDLE_INVALID)
    return m_HullShaderDict[(HullShaderIndex_t)hShader].m_pShader;
  return NULL;
}

inline ID3D11DomainShader *CShaderDeviceDx11::GetDomainShader(
    DomainShaderHandle_t hShader) const {
  if (hShader != DOMAIN_SHADER_HANDLE_INVALID)
    return m_DomainShaderDict[(DomainShaderIndex_t)hShader].m_pShader;
  return NULL;
}

inline ID3D11ComputeShader *CShaderDeviceDx11::GetComputeShader(
    ComputeShaderHandle_t hShader) const {
  if (hShader != COMPUTE_SHADER_HANDLE_INVALID)
    return m_ComputeShaderDict[(ComputeShaderIndex_t)hShader].m_pShader;
  return NULL;
}

// Singleton
extern CShaderDeviceDx11 *g_pShaderDeviceDx11;

// Attach name to device child for debug layer.
inline void SetDebugName(ID3D11DeviceChild *child, std::string_view name) {
#ifdef DEBUG
  HRESULT hr{child->SetPrivateData(WKPDID_D3DDebugObjectName,
                                   static_cast<unsigned>(name.size()),
                                   name.data())};
  Assert(SUCCEEDED(hr));
#endif
}

// Attach name to device child for debug layer.
template <size_t size>
inline void SetDebugName(ID3D11DeviceChild *child, const char (&name)[size]) {
#ifdef DEBUG
  SetDebugName(child, std::string_view{name, size});
#endif
}

}  // namespace se::shaderapidx11

#endif  // !SE_MATERIALSYSTEM_SHADERAPIDX11_SHADER_DEVICE_DX11_H_
