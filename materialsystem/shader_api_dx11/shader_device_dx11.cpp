// Copyright Valve Corporation, All rights reserved.

#include "shader_device_dx11.h"

#include "shader_api_dx11.h"
#include "shader_api_dx11_global.h"
#include "shader_shadow_dx11.h"
#include "mesh_dx11.h"
#include "input_layout_dx11.h"

#include "shaderapibase.h"
#include "shaderapi/ishaderutil.h"

#include "tier0/icommandline.h"
#include "tier1/KeyValues.h"
#include "tier2/tier2.h"

#include <d3dcompiler.h>

#ifndef NDEBUG
#include <dxgi1_6.h>
#include <dxgidebug.h>
#endif

#include "windows/com_error_category.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

// For testing Fast Clip
ConVar mat_fastclip("mat_fastclip", "0", FCVAR_CHEAT);

// Explicit instantiation of shader buffer implementation
template class CShaderBuffer<ID3DBlob>;

namespace {

// See https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-error
[[nodiscard]] const char *GetDXGIErrorDescription(HRESULT error) {
  switch (error) {
    case DXGI_ERROR_ACCESS_DENIED:
      return "You tried to use a resource to which you did not have the "
             "required access privileges.\nThis error is most typically caused "
             "when you write to a shared resource with read-only access.";
    case DXGI_ERROR_ACCESS_LOST:
      return "The desktop duplication interface is invalid.\nThe desktop "
             "duplication interface typically becomes invalid when "
             "a different type of image is displayed on the desktop.";
    case DXGI_ERROR_ALREADY_EXISTS:
      return "The desired element already exists.\nThis is returned by "
             "DXGIDeclareAdapterRemovalSupport if it is not the first time "
             "that the function is called.";
    case DXGI_ERROR_CANNOT_PROTECT_CONTENT:
      return "DXGI can't provide content protection on the swap chain.\n"
             "This error is typically caused by an older driver, or when you "
             "use a swap chain that is incompatible with content protection.";
    case DXGI_ERROR_DEVICE_HUNG:
      return "The application's device failed due to badly formed commands "
             "sent by the application.\nThis is a design-time issue that "
             "should be investigated and fixed.";
    case DXGI_ERROR_DEVICE_REMOVED:
      return "The video card has been physically removed from the system, or a "
             "driver upgrade for the video card has occurred.\nThe application "
             "should destroy and recreate the device. For help debugging the "
             "problem, call ID3D10Device::GetDeviceRemovedReason.";
    case DXGI_ERROR_DEVICE_RESET:
      return "The device failed due to a badly formed command.\nThis is a "
             "run-time issue; The application should destroy and recreate the "
             "device.";
    case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
      return "The driver encountered a problem and was put into the device "
             "removed state.";
    case DXGI_ERROR_FRAME_STATISTICS_DISJOINT:
      return "An event (for example, a power cycle) interrupted the gathering "
             "of presentation statistics.";
    case DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE:
      return "The application attempted to acquire exclusive ownership of an "
             "output, but failed because some other application (or device "
             "within the application) already acquired ownership.";
    case DXGI_ERROR_INVALID_CALL:
      return "The application provided invalid parameter data; this must be "
             "debugged and fixed before the application is released.";
    case DXGI_ERROR_MORE_DATA:
      return "The buffer supplied by the application is not big enough to hold "
             "the requested data.";
    case DXGI_ERROR_NAME_ALREADY_EXISTS:
      return "The supplied name of a resource in a call to "
             "IDXGIResource1::CreateSharedHandle is already associated with "
             "some other resource.";
    case DXGI_ERROR_NONEXCLUSIVE:
      return "A global counter resource is in use, and the Direct3D device "
             "can't currently use the counter resource.";
    case DXGI_ERROR_NOT_CURRENTLY_AVAILABLE:
      return "The resource or request is not currently available, but it might "
             "become available later.";
    case DXGI_ERROR_NOT_FOUND:
      return "When calling IDXGIObject::GetPrivateData, the GUID passed in is "
             "not recognized as one previously passed to "
             "IDXGIObject::SetPrivateData or "
             "IDXGIObject::SetPrivateDataInterface. When calling "
             "IDXGIFactory::EnumAdapters or IDXGIAdapter::EnumOutputs, the "
             "enumerated ordinal is out of range.";
    case DXGI_ERROR_REMOTE_CLIENT_DISCONNECTED:
      return "DXGI_ERROR_REMOTE_CLIENT_DISCONNECTED";
    case DXGI_ERROR_REMOTE_OUTOFMEMORY:
      return "DXGI_ERROR_REMOTE_OUTOFMEMORY";
    case DXGI_ERROR_RESTRICT_TO_OUTPUT_STALE:
      return "The DXGI output(monitor) to which the swap chain content was "
             "restricted is now disconnected or changed.";
    case DXGI_ERROR_SDK_COMPONENT_MISSING:
      return "The operation depends on an SDK component that is missing or "
             "mismatched";
    case DXGI_ERROR_SESSION_DISCONNECTED:
      return "The Remote Desktop Services session is currently disconnected.";
    case DXGI_ERROR_UNSUPPORTED:
      return "The requested functionality is not supported by the device or "
             "the driver.";
    case DXGI_ERROR_WAIT_TIMEOUT:
      return "The time-out interval elapsed before the next desktop frame was "
             "available.";
    case DXGI_ERROR_WAS_STILL_DRAWING:
      return "The GPU was busy at the moment when a call was made to perform "
             "an operation, and did not execute or schedule the operation.";
    case S_OK:
      return "The method succeeded without an error.";
  }

  return "Unknown DXGI error.";
}

[[nodiscard]] const char *GetD3DFeatureLevelDescription(
    D3D_FEATURE_LEVEL feature_level) {
  switch (feature_level) {
    case D3D_FEATURE_LEVEL_1_0_GENERIC:
      return "Generic";
    case D3D_FEATURE_LEVEL_1_0_CORE:
      return "Core";
    case D3D_FEATURE_LEVEL_9_1:
      return "9.1";
    case D3D_FEATURE_LEVEL_9_2:
      return "9.2";
    case D3D_FEATURE_LEVEL_9_3:
      return "9.3";
    case D3D_FEATURE_LEVEL_10_0:
      return "10.0";
    case D3D_FEATURE_LEVEL_10_1:
      return "10.1";
    case D3D_FEATURE_LEVEL_11_0:
      return "11.0";
    case D3D_FEATURE_LEVEL_11_1:
      return "11.1";
    case D3D_FEATURE_LEVEL_12_0:
      return "12.0";
    case D3D_FEATURE_LEVEL_12_1:
      return "12.1";
    case D3D_FEATURE_LEVEL_12_2:
      return "12.2";
    default:
      AssertMsg(false, "Unknown D3D feature level 0x%x.", feature_level);
      return "Unknown";
  }
}

se::shaderapidx11::CShaderDeviceMgrDx11 g_ShaderDeviceMgrDx11;

}  // namespace

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CShaderDeviceMgrDx11, IShaderDeviceMgr,
                                  SHADER_DEVICE_MGR_INTERFACE_VERSION,
                                  g_ShaderDeviceMgrDx11)

namespace {

se::shaderapidx11::CShaderDeviceDx11 g_ShaderDeviceDx11;

}  // namespace

namespace se::shaderapidx11 {

CShaderDeviceDx11 *g_pShaderDeviceDx11 = &g_ShaderDeviceDx11;

}  // namespace se::shaderapidx11

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CShaderDeviceDx11, IShaderDevice,
                                  SHADER_DEVICE_INTERFACE_VERSION,
                                  g_ShaderDeviceDx11)

namespace se::shaderapidx11 {

CShaderDeviceMgrDx11::CShaderDeviceMgrDx11()
    : m_use_dx_level_from_command_line{true} {}

CShaderDeviceMgrDx11::~CShaderDeviceMgrDx11() {}

bool CShaderDeviceMgrDx11::Connect(CreateInterfaceFn factory) {
  LOCK_SHADERAPI();

  if (!BaseClass::Connect(factory)) return false;

  win::com::com_ptr<IDXGIFactory1> dxgi_factory;
  HRESULT hr{CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory))};
  if (FAILED(hr)) {
    Warning("CreateDXGIFactory1 failed w/e %s.\n",
            se::win::com::com_error_category().message(hr).c_str());
    return false;
  }

  hr = dxgi_factory.QueryInterface(IID_PPV_ARGS(&m_dxgi_factory));
  if (FAILED(hr)) {
    Warning("IDXGIFactory1::QueryInterface(IDXGIFactory6) failed w/e %s.\n",
            se::win::com::com_error_category().message(hr).c_str());
    return false;
  }

  if (!InitGpuInfo()) {
    Warning("No suitable GPU device. Do you have a GPU?\n");
    return false;
  }

  return true;
}

void CShaderDeviceMgrDx11::Disconnect() {
  LOCK_SHADERAPI();

  if (m_dxgi_factory) m_dxgi_factory.Release();

  BaseClass::Disconnect();
}

InitReturnVal_t CShaderDeviceMgrDx11::Init() {
  LOCK_SHADERAPI();

  return INIT_OK;
}

void CShaderDeviceMgrDx11::Shutdown() {
  LOCK_SHADERAPI();

  if (g_pShaderDevice) {
    g_pShaderDevice->ShutdownDevice();
    g_pShaderDevice = nullptr;
  }
}

// Initialize adapter information.
bool CShaderDeviceMgrDx11::InitGpuInfo() {
  m_Adapters.RemoveAll();

  bool has_any_adapter = false;
  se::win::com::com_ptr<IDXGIAdapter4> adapter;

  for (UINT adapter_idx{0}; m_dxgi_factory->EnumAdapterByGpuPreference(
                                adapter_idx, KDxgiGpuPreference,
                                IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
       ++adapter_idx) {
    AdapterInfo_t &info{m_Adapters[m_Adapters.AddToTail()]};
#ifdef _DEBUG
    memset(&info.m_ActualCaps, 0xDD, sizeof(info.m_ActualCaps));
#endif

    se::win::com::com_ptr<IDXGIOutput6> output{GetAdapterOutput(adapter_idx)};

    info.m_ActualCaps.m_bDeviceOk =
        ComputeCapsFromD3D(&info.m_ActualCaps, adapter, output);
    if (!info.m_ActualCaps.m_bDeviceOk) continue;

    ReadDXSupportLevels(info.m_ActualCaps);

    // Read dxsupport.cfg which has config overrides for particular cards.
    ReadHardwareCaps(info.m_ActualCaps, info.m_ActualCaps.m_nMaxDXSupportLevel);

    // What's in "-shader" overrides dxsupport.cfg
    const char *override = CommandLine()->ParmValue("-shader");
    if (override) V_strcpy_safe(info.m_ActualCaps.m_pShaderDLL, override);

    has_any_adapter = true;
  }

  return has_any_adapter;
}

// Determines hardware caps from D3D.
bool CShaderDeviceMgrDx11::ComputeCapsFromD3D(HardwareCaps_t *caps,
                                              IDXGIAdapter4 *adapter,
                                              IDXGIOutput6 *output) {
  DXGI_ADAPTER_DESC3 desc;
  HRESULT hr{adapter->GetDesc3(&desc)};
  Assert(SUCCEEDED(hr));
  if (FAILED(hr) || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) return false;

  BOOL is_allow_tearing{FALSE};
  hr = m_dxgi_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                           &is_allow_tearing,
                                           sizeof(is_allow_tearing));
  Assert(SUCCEEDED(hr));
  if (FAILED(hr)) return false;

  // Dx11 settings
  // NOTE: We'll need to have different settings for dx11.1 and dx11
  Q_UnicodeToUTF8(desc.Description, caps->m_pDriverName,
                  MATERIAL_ADAPTER_NAME_LENGTH);
  caps->m_VendorID = static_cast<VendorId>(desc.VendorId);
  caps->m_DeviceID = desc.DeviceId;
  caps->m_SubSysID = desc.SubSysId;
  caps->m_Revision = desc.Revision;
  caps->m_nDXSupportLevel = 110;
  caps->m_nMaxDXSupportLevel = 110;

  caps->m_NumSamplers = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
  caps->m_NumTextureStages = 0;
  caps->m_HasSetDeviceGammaRamp = true;
  caps->m_bSoftwareVertexProcessing = false;
  caps->m_SupportsVertexShaders = true;
  caps->m_SupportsVertexShaders_2_0 = false;
  caps->m_SupportsPixelShaders = true;
  caps->m_SupportsPixelShaders_1_4 = false;
  caps->m_SupportsPixelShaders_2_0 = false;
  caps->m_SupportsPixelShaders_2_b = false;
  caps->m_SupportsShaderModel_3_0 = false;
  caps->m_SupportsShaderModel_4_0 = false;
  caps->m_SupportsShaderModel_5_0 = true;
  caps->m_SupportsShaderModel_5_1 = false;
  caps->m_SupportsShaderModel_6_0 = false;
  caps->m_SupportsHullShaders = true;
  caps->m_SupportsDomainShaders = true;
  caps->m_SupportsComputeShaders = true;
  caps->m_SupportsCompressedTextures = COMPRESSED_TEXTURES_ON;
  caps->m_SupportsCompressedVertices = VERTEX_COMPRESSION_ON;
  caps->m_bSupportsAnisotropicFiltering = true;
  caps->m_bSupportsMagAnisotropicFiltering = true;
  caps->m_bSupportsVertexTextures = true;
  caps->m_nMaxAnisotropy = D3D11_REQ_MAXANISOTROPY;
  caps->m_MaxTextureWidth = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
  caps->m_MaxTextureHeight = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
  caps->m_MaxTextureDepth = D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
  // Assume D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION : 1
  caps->m_MaxTextureAspectRatio = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
  caps->m_MaxPrimitiveCount = (1ULL << D3D11_IA_PRIMITIVE_ID_BIT_COUNT) - 1;
  caps->m_ZBiasAndSlopeScaledDepthBiasSupported = true;
  caps->m_SupportsMipmapping = true;
  caps->m_SupportsOverbright = true;
  caps->m_SupportsCubeMaps = true;
  caps->m_NumPixelShaderConstants =
      std::min(D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT,
               D3D11_REQ_IMMEDIATE_CONSTANT_BUFFER_ELEMENT_COUNT);
  caps->m_NumVertexShaderConstants =
      std::min(D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT,
               D3D11_REQ_IMMEDIATE_CONSTANT_BUFFER_ELEMENT_COUNT);
  caps->m_TextureMemorySize = desc.DedicatedVideoMemory;
  caps->m_MaxNumLights = 4;
  caps->m_SupportsHardwareLighting = false;
  caps->m_MaxBlendMatrices = 0;
  caps->m_MaxBlendMatrixIndices = 0;
  caps->m_MaxVertexShaderBlendMatrices = 53;  // FIXME
  caps->m_SupportsMipmappedCubemaps = true;
  caps->m_SupportsNonPow2Textures = true;
  caps->m_PreferDynamicTextures = false;
  caps->m_HasProjectedBumpEnv = true;
  caps->m_MaxUserClipPlanes = MAXUSERCLIPPLANES;
  caps->m_HDRType = HDR_TYPE_FLOAT;
  caps->m_SupportsSRGB = true;
  caps->m_FakeSRGBWrite = true;
  caps->m_CanDoSRGBReadFromRTs = true;
  caps->m_bSupportsSpheremapping = true;
  caps->m_UseFastClipping = false;
  caps->m_pShaderDLL[0] = 0;
  caps->m_bNeedsATICentroidHack = false;
  caps->m_bColorOnSecondStream = true;
  caps->m_bSupportsStreamOffset = true;
  caps->m_bFogColorSpecifiedInLinearSpace = desc.VendorId == VENDORID_NVIDIA;
  caps->m_nVertexTextureCount = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
  caps->m_nMaxVertexTextureDimension = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
  // https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-blend-state#alpha-to-coverage
  caps->m_bSupportsAlphaToCoverage = true;
  caps->m_bSupportsShadowDepthTextures = true;
  caps->m_bSupportsFetch4 = desc.VendorId == VENDORID_ATI;
  caps->m_bSupportsBorderColor = true;
  caps->m_ShadowDepthTextureFormat = IMAGE_FORMAT_UNKNOWN;
  caps->m_nMaxViewports =
      D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
  caps->m_bCanStretchRectFromTextures = true;

  DXGI_GAMMA_CONTROL_CAPABILITIES gammaCaps;
  // Calling this method is only supported while in full-screen mode.
  if (SUCCEEDED(output->GetGammaControlCapabilities(&gammaCaps))) {
    caps->m_flMinGammaControlPoint = gammaCaps.MinConvertedValue;
    caps->m_flMaxGammaControlPoint = gammaCaps.MaxConvertedValue;
    caps->m_nGammaControlPointCount = gammaCaps.NumGammaControlPoints;
  } else {
    caps->m_flMinGammaControlPoint = 0.0f;
    caps->m_flMaxGammaControlPoint = 65535.0f;
    caps->m_nGammaControlPointCount = 256;
  }

  caps->m_bPresentAllowTearing = is_allow_tearing != FALSE;

  return true;
}

// Gets the number of adapters...
unsigned CShaderDeviceMgrDx11::GetAdapterCount() const {
  return static_cast<unsigned>(m_Adapters.Count());
}

// Returns info about each adapter.
void CShaderDeviceMgrDx11::GetAdapterInfo(unsigned adapter_no,
                                          MaterialAdapterInfo_t &info) const {
  LOCK_SHADERAPI();

  Assert((intp)adapter_no < m_Adapters.Count());

  const HardwareCaps_t &caps = m_Adapters[adapter_no].m_ActualCaps;
  memcpy(&info, &caps, sizeof(MaterialAdapterInfo_t));
}

// Returns the adapter interface for a particular adapter.
se::win::com::com_ptr<IDXGIAdapter4> CShaderDeviceMgrDx11::GetAdapter(
    unsigned adapter_no) const {
  LOCK_SHADERAPI();

  Assert(m_dxgi_factory && adapter_no < GetAdapterCount());

  se::win::com::com_ptr<IDXGIAdapter4> adapter;
  const HRESULT hr{m_dxgi_factory->EnumAdapterByGpuPreference(
      adapter_no, KDxgiGpuPreference, IID_PPV_ARGS(&adapter))};

  Assert(!FAILED(hr));

  return SUCCEEDED(hr) ? adapter : se::win::com::com_ptr<IDXGIAdapter4>{};
}

// Returns the amount of video memory in bytes for a particular adapter.
size_t CShaderDeviceMgrDx11::GetVidMemBytes(unsigned adapter_no) const {
  LOCK_SHADERAPI();

  se::win::com::com_ptr<IDXGIAdapter4> adapter{GetAdapter(adapter_no)};
  if (!adapter) return 0;

  DXGI_ADAPTER_DESC1 desc;
  const HRESULT hr{adapter->GetDesc1(&desc)};

  Assert(!FAILED(hr));

  return SUCCEEDED(hr) ? desc.DedicatedVideoMemory : 0;
}

// Returns the appropriate adapter output to use
se::win::com::com_ptr<IDXGIOutput6> CShaderDeviceMgrDx11::GetAdapterOutput(
    unsigned adapter_no) const {
  LOCK_SHADERAPI();

  se::win::com::com_ptr<IDXGIAdapter4> adapter{GetAdapter(adapter_no)};
  if (!adapter) return {};

  DXGI_OUTPUT_DESC desc;
  se::win::com::com_ptr<IDXGIOutput> output;

  for (UINT i{0}; adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND;
       ++i) {
    HRESULT hr{output->GetDesc(&desc)};
    if (FAILED(hr)) continue;

    // FIXME: Is this what I want? Or should I be looking at other fields,
    // like DXGI_MODE_ROTATION_IDENTITY?
    if (!desc.AttachedToDesktop) continue;

    se::win::com::com_ptr<IDXGIOutput6> output6;
    hr = output.QueryInterface(IID_PPV_ARGS(&output6));
    if (FAILED(hr)) continue;

    return output6;
  }

  return {};
}

// Returns the number of modes.
unsigned CShaderDeviceMgrDx11::GetModeCount(unsigned adapter_no) const {
  LOCK_SHADERAPI();
  Assert(m_dxgi_factory && (adapter_no < GetAdapterCount()));

  se::win::com::com_ptr<IDXGIOutput6> output{GetAdapterOutput(adapter_no)};
  if (!output) return 0;

  constexpr DXGI_FORMAT format = kDxgiAdapterResourceFormat;
  constexpr UINT flags = kDxgiAdapterEnumFlags;
  UINT modesCount = 0;

  // get the number of available display mode for the given format and
  // scanline order
  HRESULT hr = output->GetDisplayModeList1(format, flags, &modesCount, nullptr);
  Assert(!FAILED(hr));

  return SUCCEEDED(hr) ? modesCount : 0;
}

// Returns mode information..
void CShaderDeviceMgrDx11::GetModeInfo(ShaderDisplayMode_t *info,
                                       unsigned adapter_no,
                                       unsigned mode_no) const {
  // Default error state
  info->m_nWidth = info->m_nHeight = 0;
  info->m_Format = IMAGE_FORMAT_UNKNOWN;
  info->m_nRefreshRateNumerator = info->m_nRefreshRateDenominator = 0;

  LOCK_SHADERAPI();
  Assert(m_dxgi_factory && (adapter_no < GetAdapterCount()));

  se::win::com::com_ptr<IDXGIOutput6> output{GetAdapterOutput(adapter_no)};
  if (!output) return;

  constexpr DXGI_FORMAT format = kDxgiAdapterResourceFormat;
  constexpr UINT flags = kDxgiAdapterEnumFlags;
  UINT modesCount = 0;

  // get the number of available display mode for the given format and
  // scanline order
  HRESULT hr{output->GetDisplayModeList1(format, flags, &modesCount, 0)};
  Assert(!FAILED(hr));

  if (mode_no >= modesCount) return;

  auto *mode_descs = stackallocT(DXGI_MODE_DESC1, modesCount);
  hr = output->GetDisplayModeList1(format, flags, &modesCount, mode_descs);
  Assert(!FAILED(hr));

  info->m_nWidth = mode_descs[mode_no].Width;
  info->m_nHeight = mode_descs[mode_no].Height;
  info->m_Format =
      ImageLoader::DxgiFormatToImageFormat(mode_descs[mode_no].Format);
  info->m_nRefreshRateNumerator = mode_descs[mode_no].RefreshRate.Numerator;
  info->m_nRefreshRateDenominator = mode_descs[mode_no].RefreshRate.Denominator;
}

// Returns the current mode for an adapter.
void CShaderDeviceMgrDx11::GetCurrentModeInfo(ShaderDisplayMode_t *info,
                                              unsigned adapter_no) const {
  // Default error state
  info->m_nWidth = info->m_nHeight = 0;
  info->m_Format = IMAGE_FORMAT_UNKNOWN;
  info->m_nRefreshRateNumerator = info->m_nRefreshRateDenominator = 0;

  Assert(info->m_nVersion == SHADER_DISPLAY_MODE_VERSION);
  LOCK_SHADERAPI();

  Assert(m_dxgi_factory && (adapter_no < GetAdapterCount()));

  se::win::com::com_ptr<IDXGIOutput6> output{GetAdapterOutput(adapter_no)};
  if (!output) return;

  DXGI_OUTPUT_DESC1 output_desc;
  HRESULT hr{output->GetDesc1(&output_desc)};
  Assert(!FAILED(hr));

  const auto output_width =
      static_cast<unsigned>(output_desc.DesktopCoordinates.right -
                            output_desc.DesktopCoordinates.left);
  const auto output_height =
      static_cast<unsigned>(output_desc.DesktopCoordinates.bottom -
                            output_desc.DesktopCoordinates.top);

  constexpr DXGI_FORMAT format = kDxgiAdapterResourceFormat;
  constexpr UINT flags = kDxgiAdapterEnumFlags;
  UINT modesCount = 0;

  // get the number of available display mode for the given format and
  // scanline order
  hr = output->GetDisplayModeList(format, flags, &modesCount, 0);
  Assert(!FAILED(hr));

  auto *mode_descs = stackallocT(DXGI_MODE_DESC, modesCount);
  hr = output->GetDisplayModeList(format, flags, &modesCount, mode_descs);
  Assert(!FAILED(hr));

  info->m_nRefreshRateNumerator = 0;

  for (unsigned nMode{0}; nMode < modesCount; ++nMode) {
    const auto &mode_desc = mode_descs[nMode];

    if (mode_desc.Width == output_width && mode_desc.Height == output_height &&
        mode_desc.RefreshRate.Numerator >=
            static_cast<uint>(info->m_nRefreshRateNumerator)) {
      info->m_nWidth = mode_desc.Width;
      info->m_nHeight = mode_desc.Height;
      info->m_Format = ImageLoader::DxgiFormatToImageFormat(mode_desc.Format);
      info->m_nRefreshRateNumerator = mode_desc.RefreshRate.Numerator;
      info->m_nRefreshRateDenominator = mode_desc.RefreshRate.Denominator;
    }
  }
}

bool CShaderDeviceMgrDx11::SetAdapter(unsigned adapter_no, int flags) {
  LOCK_SHADERAPI();

  // TODO(dimhotepus)Use D3D_DRIVER_TYPE_UNKNOWN as it is the only supported
  // when external adapter passed to D3D11CreateDeviceAndSwapChain.
  g_pShaderDeviceDx11->SetAdapterAndDriver(
      std::clamp(adapter_no, 0U, GetAdapterCount() - 1),
      (flags & MATERIAL_INIT_REFERENCE_RASTERIZER) ? D3D_DRIVER_TYPE_REFERENCE
                                                   : D3D_DRIVER_TYPE_UNKNOWN);

  if (!g_pShaderDeviceDx11->DetermineHardwareCaps()) return false;

  // Modify the caps based on requested DXlevels
  int forced_dx_level = CommandLine()->ParmValue("-dxlevel", 0);
  if (forced_dx_level > 0) {
    forced_dx_level = max(forced_dx_level, ABSOLUTE_MINIMUM_DXLEVEL);
  }

  // Just use the actual caps if we can't use what was requested or if the
  // default is requested
  if (forced_dx_level <= 0) {
    forced_dx_level = g_pHardwareConfig->ActualCaps().m_nDXSupportLevel;
  }
  forced_dx_level =
      g_pShaderDeviceMgr->GetClosestActualDXLevel(forced_dx_level);

  g_pHardwareConfig->SetupHardwareCaps(forced_dx_level,
                                       g_pHardwareConfig->ActualCaps());

  g_pShaderDevice = g_pShaderDeviceDx11;

  return true;
}

// Sets the mode
CreateInterfaceFn CShaderDeviceMgrDx11::SetMode(
    void *hwnd, unsigned adapter_no, const ShaderDeviceInfo_t &mode) {
  LOCK_SHADERAPI();

  Assert(adapter_no < GetAdapterCount());

  const auto &actual_caps = m_Adapters[adapter_no].m_ActualCaps;
  int dx_level =
      mode.m_nDXLevel != 0 ? mode.m_nDXLevel : actual_caps.m_nDXSupportLevel;
  if (m_use_dx_level_from_command_line) {
    dx_level = CommandLine()->ParmValue("-dxlevel", dx_level);
    m_use_dx_level_from_command_line = false;
  }
  dx_level =
      GetClosestActualDXLevel(min(dx_level, actual_caps.m_nMaxDXSupportLevel));

  if (g_pShaderAPI) {
    g_pShaderAPI->OnDeviceShutdown();
    g_pShaderAPI = nullptr;
  }

  if (g_pShaderDevice) {
    g_pShaderDevice->ShutdownDevice();
    g_pShaderDevice = nullptr;
  }

  g_pShaderShadow = nullptr;

  ShaderDeviceInfo_t set_mode = mode;
  set_mode.m_nDXLevel = dx_level;

  if (!g_pShaderDeviceDx11->InitDevice(hwnd, adapter_no, set_mode)) {
    return nullptr;
  }

  if (!g_pShaderAPIDx11->OnDeviceInit()) return nullptr;

  g_pShaderDeviceDx11->DetermineHardwareCaps();

  g_pShaderDevice = g_pShaderDeviceDx11;
  g_pShaderAPI = g_pShaderAPIDx11;
  g_pShaderShadow = g_pShaderShadowDx11;

  return ShaderInterfaceFactory;
}

CShaderDeviceDx11::CShaderDeviceDx11()
    : m_dxgi_adapter_index(UINT_MAX),
      m_d3d_driver_type(D3D_DRIVER_TYPE_NULL),
      m_d3d_feature_level{D3D_FEATURE_LEVEL_1_0_GENERIC},
      m_present_sync_interval(0),
      m_present_flags(0),
      m_is_rendering_occluded{false} {
  m_dxgi_swap_chain_desc.BufferDesc.Width = UINT_MAX;
  m_dxgi_swap_chain_desc.BufferDesc.Height = UINT_MAX;
  m_dxgi_swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_UNKNOWN;
}

CShaderDeviceDx11::~CShaderDeviceDx11() {}

// Sets the mode
bool CShaderDeviceDx11::InitDevice(void *hwnd, unsigned adapter_no,
                                   const ShaderDeviceInfo_t &mode) {
  // Make sure we've been shutdown previously
  if (m_nAdapter != UINT_MAX) {
    Warning(
        "CShaderDeviceDx11::SetMode: Previous mode %u has not been shut "
        "down!\n",
        m_nAdapter);
    return false;
  }

  LOCK_SHADERAPI();

  const HardwareCaps_t &hardware_caps{
      g_ShaderDeviceMgrDx11.GetHardwareCaps(adapter_no)};
  const bool is_present_allow_tearing{hardware_caps.m_bPresentAllowTearing};

  se::win::com::com_ptr<IDXGIAdapter4> adapter{
      g_ShaderDeviceMgrDx11.GetAdapter(adapter_no)};
  if (!adapter) return false;

  m_dxgi_output = g_ShaderDeviceMgrDx11.GetAdapterOutput(adapter_no);
  if (!m_dxgi_output) return false;

  BitwiseClear(m_dxgi_swap_chain_desc);
  m_dxgi_swap_chain_desc.BufferDesc.Width = mode.m_DisplayMode.m_nWidth;
  m_dxgi_swap_chain_desc.BufferDesc.Height = mode.m_DisplayMode.m_nHeight;
  m_dxgi_swap_chain_desc.BufferDesc.Format =
      ImageLoader::ImageFormatToDxgiFormat(mode.m_DisplayMode.m_Format);
  m_dxgi_swap_chain_desc.BufferDesc.RefreshRate.Numerator =
      mode.m_DisplayMode.m_nRefreshRateNumerator;
  m_dxgi_swap_chain_desc.BufferDesc.RefreshRate.Denominator =
      mode.m_DisplayMode.m_nRefreshRateDenominator;
  m_dxgi_swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  m_dxgi_swap_chain_desc.BufferCount = mode.m_nBackBufferCount;
  m_dxgi_swap_chain_desc.OutputWindow = static_cast<HWND>(hwnd);
  m_dxgi_swap_chain_desc.Windowed = mode.m_bWindowed ? TRUE : FALSE;
  // It is recommended to always use the tearing flag when it is supported.
  m_dxgi_swap_chain_desc.Flags =
      (is_present_allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0) |
      (mode.m_bWindowed ? 0 : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);

  // DXGI_SWAP_EFFECT_FLIP_* requires 2+ back buffers.
  Assert(m_dxgi_swap_chain_desc.BufferCount >= 2);
  m_dxgi_swap_chain_desc.SwapEffect =
      mode.m_bUsingPartialPresentation
          // Use this flag to specify the flip presentation model and to specify
          // that DXGI persist the contents of the back buffer after you call
          // IDXGISwapChain1::Present1.  This flag cannot be used with
          // multisampling.
          ? DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL
          // Use this flag to specify the flip presentation model and to specify
          // that DXGI discard the contents of the back buffer after you call
          // IDXGISwapChain1::Present1.  This flag cannot be used with
          // multisampling and partial presentation.
          : DXGI_SWAP_EFFECT_FLIP_DISCARD;

  // DXGI_SWAP_EFFECT_FLIP_* support Count == 1 and Quality == 0.
  m_dxgi_swap_chain_desc.SampleDesc.Count = 1;
  m_dxgi_swap_chain_desc.SampleDesc.Quality = 0;

  m_present_flags = m_dxgi_swap_chain_desc.Windowed && is_present_allow_tearing
                        ? DXGI_PRESENT_ALLOW_TEARING
                        : 0;
  if (m_present_flags & DXGI_PRESENT_ALLOW_TEARING) {
    // The sync interval passed in to Present (or Present1) must be 0
    // when allow tearing.
    m_present_sync_interval = 0;
  }

  UINT device_flags{0};
#if defined(DEBUG) || defined(_DEBUG)
  device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

  win::com::com_ptr<ID3D11Device> device;
  win::com::com_ptr<ID3D11DeviceContext> deviceContext;
  win::com::com_ptr<IDXGISwapChain> swapChain;

  const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_1,
                                   D3D_FEATURE_LEVEL_11_0};
  HRESULT hr{D3D11CreateDeviceAndSwapChain(
      adapter, m_d3d_driver_type, nullptr, device_flags, levels,
      std::size(levels), D3D11_SDK_VERSION, &m_dxgi_swap_chain_desc, &swapChain,
      &device, &m_d3d_feature_level, &deviceContext)};
  if (FAILED(hr)) {
    Warning(
        "Unable to create DirectX 11 render device and swap chain (hr "
        "0x%x).",
        hr);
    return false;
  }

  // The DXGI_PRESENT_ALLOW_TEARING flag cannot be used in an
  // application that is currently in full screen exclusive mode (set by
  // calling SetFullscreenState(TRUE)).  It can only be used in windowed
  // mode.  To use this flag in full screen Win32 apps, the application
  // should present to a fullscreen borderless window and disable
  // automatic ALT+ENTER fullscreen switching using
  // IDXGIFactory::MakeWindowAssociation.
  if (m_present_flags & DXGI_PRESENT_ALLOW_TEARING) {
    se::win::com::com_ptr<IDXGIFactory1> factory;

    hr = swapChain->GetParent(IID_PPV_ARGS(&factory));
    Assert(SUCCEEDED(hr));

    // You must call the MakeWindowAssociation method on the factory
    // object associated with the target HWND swap chain.
    hr = factory->MakeWindowAssociation(m_dxgi_swap_chain_desc.OutputWindow,
                                        DXGI_MWA_NO_ALT_ENTER);
    Assert(SUCCEEDED(hr));
  }

  hr = device.QueryInterface(IID_PPV_ARGS(&m_d3d11_device));
  if (FAILED(hr)) {
    Warning("Unable to query ID3D11Device5 interface (hr 0x%x).", hr);
    return false;
  }

  hr = deviceContext.QueryInterface(IID_PPV_ARGS(&m_d3d11_immediate_context));
  if (FAILED(hr)) {
    Warning("Unable to query ID3D11DeviceContext4 interface (hr 0x%x).", hr);
    return false;
  }

  hr = swapChain.QueryInterface(IID_PPV_ARGS(&m_dxgi_swap_chain));
  if (FAILED(hr)) {
    Warning("Unable to query IDXGISwapChain4 interface (hr 0x%x).", hr);
    return false;
  }

  Msg("Created DirectX 11 device (FL %s).\n",
      GetD3DFeatureLevelDescription(m_d3d_feature_level));

// https://walbourn.github.io/dxgi-debug-device/
#ifndef NDEBUG
  se::win::com::com_ptr<IDXGIInfoQueue> dxgi_info_queue;
  if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_info_queue)))) {
    hr = dxgi_info_queue->SetBreakOnSeverity(
        DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, true);
    Assert(SUCCEEDED(hr));
    hr = dxgi_info_queue->SetBreakOnSeverity(
        DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
    Assert(SUCCEEDED(hr));
    hr = dxgi_info_queue->SetBreakOnSeverity(
        DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
    Assert(SUCCEEDED(hr));
  }

  se::win::com::com_ptr<ID3D11Debug> d3d11_debug;
  hr = m_d3d11_device.QueryInterface(IID_PPV_ARGS(&d3d11_debug));
  if (SUCCEEDED(hr)) {
    se::win::com::com_ptr<ID3D11InfoQueue> d3d11_info_queue;
    hr = d3d11_debug.QueryInterface(IID_PPV_ARGS(&d3d11_info_queue));
    if (SUCCEEDED(hr)) {
#ifdef _DEBUG
      hr = d3d11_info_queue->SetBreakOnSeverity(
          D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
      Assert(SUCCEEDED(hr));
      hr = d3d11_info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR,
                                                true);
      Assert(SUCCEEDED(hr));
      hr = d3d11_info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING,
                                                true);
      Assert(SUCCEEDED(hr));
#endif

      D3D11_MESSAGE_ID hide_ids[] = {
          D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS,
          // TODO: Add more message IDs here as needed
      };

      D3D11_INFO_QUEUE_FILTER filter = {};
      filter.DenyList.NumIDs = std::size(hide_ids);
      filter.DenyList.pIDList = hide_ids;

      hr = d3d11_info_queue->AddStorageFilterEntries(&filter);
      Assert(SUCCEEDED(hr));
    }
  }
#endif

  // Create a render target view
  se::win::com::com_ptr<ID3D11Texture2D> back_buffer_0;
  hr = m_dxgi_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer_0));

  if (FAILED(hr)) return false;
  SetDebugName(back_buffer_0, "Back Buffer 0");

  if (mode.m_nBackBufferCount > 1) {
    Assert(mode.m_nBackBufferCount == 2);

    se::win::com::com_ptr<ID3D11Texture2D> back_buffer_1;
    hr = m_dxgi_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer_1));

    if (FAILED(hr)) return false;
    SetDebugName(back_buffer_1, "Back Buffer 1");
  }

  win::com::com_ptr<ID3D11RenderTargetView> render_target_view;
  hr = m_d3d11_device->CreateRenderTargetView(back_buffer_0, nullptr,
                                              &render_target_view);
  if (FAILED(hr)) return false;

  hr = render_target_view.QueryInterface(
      IID_PPV_ARGS(&m_d3d11_render_target_view));
  if (FAILED(hr)) return false;
  SetDebugName(m_d3d11_render_target_view, "Render Target View");

  ID3D11RenderTargetView *views[]{m_d3d11_render_target_view.GetInterfacePtr()};
  m_d3d11_immediate_context->OMSetRenderTargets(std::size(views), views,
                                                nullptr);

  m_hWnd = hwnd;
  m_nAdapter = m_dxgi_adapter_index = adapter_no;

  // This is our current view.
  m_ViewHWnd = hwnd;
  GetWindowSize(m_nWindowWidth, m_nWindowHeight);

  g_pHardwareConfig->SetupHardwareCaps(
      mode, g_ShaderDeviceMgrDx11.GetHardwareCaps(adapter_no));

  return true;
}

// Shuts down the mode
void CShaderDeviceDx11::ShutdownDevice() {
  if (m_d3d11_immediate_context) m_d3d11_immediate_context->ClearState();

  if (m_d3d11_render_target_view) m_d3d11_render_target_view.Release();
  if (m_d3d11_immediate_context) m_d3d11_immediate_context.Release();
  if (m_d3d11_device) m_d3d11_device.Release();
  if (m_dxgi_swap_chain) m_dxgi_swap_chain.Release();
  if (m_dxgi_output) m_dxgi_output.Release();

  m_hWnd = nullptr;
  m_nAdapter = UINT_MAX;
  m_dxgi_adapter_index = UINT_MAX;
}

// Are we using graphics?
bool CShaderDeviceDx11::IsUsingGraphics() const { return !!m_d3d11_device; }

unsigned CShaderDeviceDx11::GetCurrentAdapter() const {
  return m_dxgi_adapter_index;
}

ImageFormat CShaderDeviceDx11::GetBackBufferFormat() const {
  return ImageLoader::DxgiFormatToImageFormat(
      m_dxgi_swap_chain_desc.BufferDesc.Format);
}

void CShaderDeviceDx11::GetBackBufferDimensions(int &width, int &height) const {
  width = m_dxgi_swap_chain_desc.BufferDesc.Width;
  height = m_dxgi_swap_chain_desc.BufferDesc.Height;
}

// Use this to spew information about the 3D layer.
void CShaderDeviceDx11::SpewDriverInfo() const {
  LOCK_SHADERAPI();

  MaterialAdapterInfo_t adapter;
  g_pShaderDeviceMgr->GetAdapterInfo(GetCurrentAdapter(), adapter);

  Warning("Shader API Driver Info:\n\nDriver : %s Version : %u.%u\n",
          adapter.m_pDriverName, adapter.m_nDriverVersionHigh,
          adapter.m_nDriverVersionLow);
  Warning("GPU version %u.%u.%u.%u\n\n", adapter.m_VendorID, adapter.m_DeviceID,
          adapter.m_SubSysID, adapter.m_Revision);

  ShaderDisplayMode_t mode;
  g_pShaderDeviceMgr->GetCurrentModeInfo(&mode, m_nAdapter);
  Warning("Display mode : %d x %d (%s)\n", mode.m_nWidth, mode.m_nHeight,
          ImageLoader::GetName(mode.m_Format));

  Warning("Size of Texture Memory : %zu KiB\n",
          g_pHardwareConfig->Caps().m_TextureMemorySize / 1024);

  Warning("vendor id: 0x%x\n", g_pHardwareConfig->ActualCaps().m_VendorID);
  Warning("device id: 0x%x\n", g_pHardwareConfig->ActualCaps().m_DeviceID);

  Warning("SHADERAPI CAPS:\n");
  Warning("m_NumSamplers: %d\n", g_pHardwareConfig->Caps().m_NumSamplers);
  Warning("m_NumTextureStages: %d\n",
          g_pHardwareConfig->Caps().m_NumTextureStages);
  Warning("m_HasSetDeviceGammaRamp: %s\n",
          g_pHardwareConfig->Caps().m_HasSetDeviceGammaRamp ? "yes" : "no");
  Warning("m_SupportsVertexShaders (1.1): %s\n",
          g_pHardwareConfig->Caps().m_SupportsVertexShaders ? "yes" : "no");
  Warning("m_SupportsVertexShaders_2_0: %s\n",
          g_pHardwareConfig->Caps().m_SupportsVertexShaders_2_0 ? "yes" : "no");
  Warning("m_SupportsPixelShaders (1.1): %s\n",
          g_pHardwareConfig->Caps().m_SupportsPixelShaders ? "yes" : "no");
  Warning("m_SupportsPixelShaders_1_4: %s\n",
          g_pHardwareConfig->Caps().m_SupportsPixelShaders_1_4 ? "yes" : "no");
  Warning("m_SupportsPixelShaders_2_0: %s\n",
          g_pHardwareConfig->Caps().m_SupportsPixelShaders_2_0 ? "yes" : "no");
  Warning("m_SupportsPixelShaders_2_b: %s\n",
          g_pHardwareConfig->Caps().m_SupportsPixelShaders_2_b ? "yes" : "no");
  Warning("m_SupportsHullShaders: %s\n",
          g_pHardwareConfig->Caps().m_SupportsHullShaders ? "yes" : "no");
  Warning("m_SupportsDomainShaders: %s\n",
          g_pHardwareConfig->Caps().m_SupportsDomainShaders ? "yes" : "no");
  Warning("m_SupportsComputeShaders: %s\n",
          g_pHardwareConfig->Caps().m_SupportsComputeShaders ? "yes" : "no");
  Warning("m_SupportsShaderModel_3_0: %s\n",
          g_pHardwareConfig->Caps().m_SupportsShaderModel_3_0 ? "yes" : "no");
  Warning("m_SupportsShaderModel_4_0: %s\n",
          g_pHardwareConfig->Caps().m_SupportsShaderModel_4_0 ? "yes" : "no");
  Warning("m_SupportsShaderModel_5_0: %s\n",
          g_pHardwareConfig->Caps().m_SupportsShaderModel_5_0 ? "yes" : "no");
  Warning("m_SupportsShaderModel_5_1: %s\n",
          g_pHardwareConfig->Caps().m_SupportsShaderModel_5_1 ? "yes" : "no");
  Warning("m_SupportsShaderModel_6_0: %s\n",
          g_pHardwareConfig->Caps().m_SupportsShaderModel_6_0 ? "yes" : "no");

  switch (g_pHardwareConfig->Caps().m_SupportsCompressedTextures) {
    case COMPRESSED_TEXTURES_ON:
      Warning("m_SupportsCompressedTextures: COMPRESSED_TEXTURES_ON\n");
      break;
    case COMPRESSED_TEXTURES_OFF:
      Warning("m_SupportsCompressedTextures: COMPRESSED_TEXTURES_OFF\n");
      break;
    case COMPRESSED_TEXTURES_NOT_INITIALIZED:
      Warning(
          "m_SupportsCompressedTextures: "
          "COMPRESSED_TEXTURES_NOT_INITIALIZED\n");
      break;
    default:
      Assert(0);
      break;
  }
  Warning("m_SupportsCompressedVertices: %d\n",
          g_pHardwareConfig->Caps().m_SupportsCompressedVertices);
  Warning(
      "m_bSupportsAnisotropicFiltering: %s\n",
      g_pHardwareConfig->Caps().m_bSupportsAnisotropicFiltering ? "yes" : "no");
  Warning("m_nMaxAnisotropy: %d\n", g_pHardwareConfig->Caps().m_nMaxAnisotropy);
  Warning("m_MaxTextureWidth: %d\n",
          g_pHardwareConfig->Caps().m_MaxTextureWidth);
  Warning("m_MaxTextureHeight: %d\n",
          g_pHardwareConfig->Caps().m_MaxTextureHeight);
  Warning("m_MaxTextureAspectRatio: %d\n",
          g_pHardwareConfig->Caps().m_MaxTextureAspectRatio);
  Warning("m_MaxPrimitiveCount: %u\n",
          g_pHardwareConfig->Caps().m_MaxPrimitiveCount);
  Warning("m_ZBiasAndSlopeScaledDepthBiasSupported: %s\n",
          g_pHardwareConfig->Caps().m_ZBiasAndSlopeScaledDepthBiasSupported
              ? "yes"
              : "no");
  Warning("m_SupportsMipmapping: %s\n",
          g_pHardwareConfig->Caps().m_SupportsMipmapping ? "yes" : "no");
  Warning("m_SupportsOverbright: %s\n",
          g_pHardwareConfig->Caps().m_SupportsOverbright ? "yes" : "no");
  Warning("m_SupportsCubeMaps: %s\n",
          g_pHardwareConfig->Caps().m_SupportsCubeMaps ? "yes" : "no");
  Warning("m_NumPixelShaderConstants: %d\n",
          g_pHardwareConfig->Caps().m_NumPixelShaderConstants);
  Warning("m_NumVertexShaderConstants: %d\n",
          g_pHardwareConfig->Caps().m_NumVertexShaderConstants);
  Warning("m_NumBooleanVertexShaderConstants: %d\n",
          g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants);
  Warning("m_NumIntegerVertexShaderConstants: %d\n",
          g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants);
  Warning("m_TextureMemorySize: %zu\n",
          g_pHardwareConfig->Caps().m_TextureMemorySize);
  Warning("m_MaxNumLights: %d\n", g_pHardwareConfig->Caps().m_MaxNumLights);
  Warning("m_SupportsHardwareLighting: %s\n",
          g_pHardwareConfig->Caps().m_SupportsHardwareLighting ? "yes" : "no");
  Warning("m_MaxBlendMatrices: %d\n",
          g_pHardwareConfig->Caps().m_MaxBlendMatrices);
  Warning("m_MaxBlendMatrixIndices: %d\n",
          g_pHardwareConfig->Caps().m_MaxBlendMatrixIndices);
  Warning("m_MaxVertexShaderBlendMatrices: %d\n",
          g_pHardwareConfig->Caps().m_MaxVertexShaderBlendMatrices);
  Warning("m_SupportsMipmappedCubemaps: %s\n",
          g_pHardwareConfig->Caps().m_SupportsMipmappedCubemaps ? "yes" : "no");
  Warning("m_SupportsNonPow2Textures: %s\n",
          g_pHardwareConfig->Caps().m_SupportsNonPow2Textures ? "yes" : "no");
  Warning("m_nDXSupportLevel: %d\n",
          g_pHardwareConfig->Caps().m_nDXSupportLevel);
  Warning("m_PreferDynamicTextures: %s\n",
          g_pHardwareConfig->Caps().m_PreferDynamicTextures ? "yes" : "no");
  Warning("m_HasProjectedBumpEnv: %s\n",
          g_pHardwareConfig->Caps().m_HasProjectedBumpEnv ? "yes" : "no");
  Warning("m_MaxUserClipPlanes: %d\n",
          g_pHardwareConfig->Caps().m_MaxUserClipPlanes);
  Warning("m_SupportsSRGB: %s\n",
          g_pHardwareConfig->Caps().m_SupportsSRGB ? "yes" : "no");
  switch (g_pHardwareConfig->Caps().m_HDRType) {
    case HDR_TYPE_NONE:
      Warning("m_HDRType: HDR_TYPE_NONE\n");
      break;
    case HDR_TYPE_INTEGER:
      Warning("m_HDRType: HDR_TYPE_INTEGER\n");
      break;
    case HDR_TYPE_FLOAT:
      Warning("m_HDRType: HDR_TYPE_FLOAT\n");
      break;
    default:
      Assert(0);
      break;
  }
  Warning("m_bSupportsSpheremapping: %s\n",
          g_pHardwareConfig->Caps().m_bSupportsSpheremapping ? "yes" : "no");
  Warning("m_UseFastClipping: %s\n",
          g_pHardwareConfig->Caps().m_UseFastClipping ? "yes" : "no");
  Warning("m_pShaderDLL: %s\n", g_pHardwareConfig->Caps().m_pShaderDLL);
  Warning("m_bNeedsATICentroidHack: %s\n",
          g_pHardwareConfig->Caps().m_bNeedsATICentroidHack ? "yes" : "no");
  Warning(
      "m_bDisableShaderOptimizations: %s\n",
      g_pHardwareConfig->Caps().m_bDisableShaderOptimizations ? "yes" : "no");
  Warning("m_bColorOnSecondStream: %s\n",
          g_pHardwareConfig->Caps().m_bColorOnSecondStream ? "yes" : "no");
  Warning("m_MaxSimultaneousRenderTargets: %d\n",
          g_pHardwareConfig->Caps().m_MaxSimultaneousRenderTargets);

  Warning("m_bPresentAllowTearing: %s\n",
          g_pHardwareConfig->Caps().m_bPresentAllowTearing ? "yes" : "no");
}

// Swap buffers
void CShaderDeviceDx11::Present() {
  const DWORD present_flags{!m_is_rendering_occluded ? m_present_flags
                                                     : DXGI_PRESENT_TEST};

  // For the bit-block transfer (bitblt) model (DXGI_SWAP_EFFECT_DISCARD or
  // DXGI_SWAP_EFFECT_SEQUENTIAL), values are:
  // 0 - The presentation occurs immediately, there is no synchronization.
  // 1 through 4 - Synchronize presentation after the nth vertical blank.
  //
  // For the flip model (DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL), values are:
  // 0 - Cancel the remaining time on the previously presented frame and
  // discard this frame if a newer frame is queued. 1 through 4 - Synchronize
  // presentation for at least n vertical blanks.
  DXGI_PRESENT_PARAMETERS present_params = {};
  // For now update entire frame.
  present_params.DirtyRectsCount = 0;
  present_params.pDirtyRects = nullptr;
  present_params.pScrollOffset = nullptr;
  present_params.pScrollRect = nullptr;

  HRESULT hr{m_dxgi_swap_chain->Present1(m_present_sync_interval, present_flags,
                                         &present_params)};
  if (hr == DXGI_STATUS_OCCLUDED) {
    // There is a window covering our entire rendering area.
    // Don't render until we're visible again.
    m_is_rendering_occluded = true;
  } else if (hr == DXGI_ERROR_DEVICE_RESET) {
    // TODO(dimhotepus): If a mode change happened, we must reset the device

  } else if (DXGI_ERROR_DEVICE_REMOVED == hr) {
    // Possible return values include:
    // DXGI_ERROR_DEVICE_HUNG
    // DXGI_ERROR_DEVICE_REMOVED
    // DXGI_ERROR_DEVICE_RESET
    // DXGI_ERROR_DRIVER_INTERNAL_ERROR
    // DXGI_ERROR_INVALID_CALL
    // S_OK
    hr = m_d3d11_device->GetDeviceRemovedReason();

    Warning("GPU device has been removed. 0x%x: %s.\n", hr,
            GetDXGIErrorDescription(hr));

    // TODO(dimhotepus):
    // Use a callback to ask the app if it would like to find a new device.
    // If no device removed callback is set, then look for a new device
  } else if (SUCCEEDED(hr)) {
    if (m_is_rendering_occluded) {
      // Now that we're no longer occluded allow us to render again.
      m_is_rendering_occluded = false;
    }
  } else {
    Warning("IDXGISwapChain1::Present1 failed w/e %s.\n",
            se::win::com::com_error_category().message(hr).c_str());
  }
}

// Camma ramp
void CShaderDeviceDx11::SetHardwareGammaRamp(float fGamma,
                                             float fGammaTVRangeMin,
                                             float fGammaTVRangeMax,
                                             float fGammaTVExponent,
                                             bool bTVEnabled) {
  DevMsg("CShaderDeviceDx11::SetHardwareGammaRamp %f\n", fGamma);

  Assert(m_dxgi_output);
  if (!m_dxgi_output) return;

  float flMin = g_pHardwareConfig->Caps().m_flMinGammaControlPoint;
  float flMax = g_pHardwareConfig->Caps().m_flMaxGammaControlPoint;
  int nGammaPoints = g_pHardwareConfig->Caps().m_nGammaControlPointCount;

  DXGI_GAMMA_CONTROL gammaControl;
  gammaControl.Scale.Red = gammaControl.Scale.Green = gammaControl.Scale.Blue =
      1.0f;
  gammaControl.Offset.Red = gammaControl.Offset.Green =
      gammaControl.Offset.Blue = 0.0f;

  float flOOCount = 1.0f / (nGammaPoints - 1);
  for (int i = 0; i < nGammaPoints; i++) {
    float flGamma22 = i * flOOCount;
    float flCorrection = powf(flGamma22, fGamma / 2.2f);
    flCorrection = clamp(flCorrection, flMin, flMax);

    gammaControl.GammaCurve[i].Red = flCorrection;
    gammaControl.GammaCurve[i].Green = flCorrection;
    gammaControl.GammaCurve[i].Blue = flCorrection;
  }

  HRESULT hr{m_dxgi_output->SetGammaControl(&gammaControl)};
  if (FAILED(hr))
    Warning("IDXGIOuput::SetGammaControl failed w/e %s.\n",
            se::win::com::com_error_category().message(hr).c_str());
}

// Compiles all manner of shaders.
IShaderBuffer *CShaderDeviceDx11::CompileShader(const char *program,
                                                size_t program_size,
                                                const char *shader_version) {
  unsigned compile_flags = D3DCOMPILE_AVOID_FLOW_CONTROL;
  compile_flags |= D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;

#ifdef _DEBUG
  compile_flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL0;
#else
  compile_flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

  se::win::com::com_ptr<ID3DBlob> compiled_shader, error_messages;
  HRESULT hr = D3DCompile(program, program_size, nullptr, nullptr, nullptr,
                          "main", shader_version, compile_flags, 0,
                          &compiled_shader, &error_messages);

  if (FAILED(hr)) {
    if (auto *error = error_messages ? static_cast<const char *>(
                                           error_messages->GetBufferPointer())
                                     : nullptr;
        error) {
      Warning(
          "Vertex shader:\n\n%s\n\ncompilation failed! Reported the "
          "following "
          "errors:\n%s\n",
          program, error);
    }

    return nullptr;
  }

  // NOTE: This uses small block heap allocator; so I'm not going
  // to bother creating a memory pool.
  return new CShaderBuffer(std::move(compiled_shader));
}

// Release input layouts
void CShaderDeviceDx11::ReleaseInputLayouts(VertexShaderIndex_t nIndex) {
  InputLayoutDict_t &dict = m_VertexShaderDict[nIndex].m_InputLayouts;
  unsigned short it = dict.FirstInorder();
  while (it != dict.InvalidIndex()) {
    if (dict[it].m_pInputLayout) {
      dict[it].m_pInputLayout.Release();
    }
    it = dict.NextInorder(it);
  }
}

// Create, destroy vertex shader
VertexShaderHandle_t CShaderDeviceDx11::CreateVertexShader(
    IShaderBuffer *pShaderBuffer) {
  // Create the vertex shader
  se::win::com::com_ptr<ID3D11VertexShader> pShader;
  HRESULT hr = m_d3d11_device->CreateVertexShader(
      pShaderBuffer->GetBits(), pShaderBuffer->GetSize(), nullptr, &pShader);
  if (FAILED(hr) || !pShader) return VERTEX_SHADER_HANDLE_INVALID;

  se::win::com::com_ptr<ID3D11ShaderReflection> pInfo;
  hr = D3DReflect(pShaderBuffer->GetBits(), pShaderBuffer->GetSize(),
                  IID_PPV_ARGS(&pInfo));
  if (FAILED(hr) || !pInfo) return VERTEX_SHADER_HANDLE_INVALID;

  // Insert the shader into the dictionary of shaders
  VertexShaderIndex_t i = m_VertexShaderDict.AddToTail();
  VertexShader_t &dict = m_VertexShaderDict[i];
  dict.m_pShader = std::move(pShader);
  dict.m_pInfo = std::move(pInfo);
  dict.m_nByteCodeLen = pShaderBuffer->GetSize();
  dict.m_pByteCode = new unsigned char[dict.m_nByteCodeLen];
  memcpy(dict.m_pByteCode, pShaderBuffer->GetBits(), dict.m_nByteCodeLen);
  return (VertexShaderHandle_t)i;
}

void CShaderDeviceDx11::DestroyVertexShader(VertexShaderHandle_t hShader) {
  if (hShader == VERTEX_SHADER_HANDLE_INVALID) return;

  g_pShaderAPIDx11->Unbind(hShader);

  auto i = (VertexShaderIndex_t)hShader;
  VertexShader_t &dict = m_VertexShaderDict[i];
  dict.m_pShader.Release();
  dict.m_pInfo.Release();
  delete[] dict.m_pByteCode;
  ReleaseInputLayouts(i);
  m_VertexShaderDict.Remove(i);
}

// Create, destroy geometry shader
GeometryShaderHandle_t CShaderDeviceDx11::CreateGeometryShader(
    IShaderBuffer *pShaderBuffer) {
  // Create the geometry shader
  se::win::com::com_ptr<ID3D11GeometryShader> pShader;
  HRESULT hr = m_d3d11_device->CreateGeometryShader(
      pShaderBuffer->GetBits(), pShaderBuffer->GetSize(), nullptr, &pShader);
  if (FAILED(hr) || !pShader) return GEOMETRY_SHADER_HANDLE_INVALID;

  se::win::com::com_ptr<ID3D11ShaderReflection> pInfo;
  hr = D3DReflect(pShaderBuffer->GetBits(), pShaderBuffer->GetSize(),
                  IID_PPV_ARGS(&pInfo));
  if (FAILED(hr) || !pInfo) return GEOMETRY_SHADER_HANDLE_INVALID;

  // Insert the shader into the dictionary of shaders
  GeometryShaderIndex_t i = m_GeometryShaderDict.AddToTail();
  m_GeometryShaderDict[i].m_pShader = std::move(pShader);
  m_GeometryShaderDict[i].m_pInfo = std::move(pInfo);
  return (GeometryShaderHandle_t)i;
}

void CShaderDeviceDx11::DestroyGeometryShader(GeometryShaderHandle_t hShader) {
  if (hShader == GEOMETRY_SHADER_HANDLE_INVALID) return;

  g_pShaderAPIDx11->Unbind(hShader);

  auto i = (GeometryShaderIndex_t)hShader;
  m_GeometryShaderDict[i].m_pShader.Release();
  m_GeometryShaderDict[i].m_pInfo.Release();
  m_GeometryShaderDict.Remove(i);
}

// Create, destroy pixel shader
PixelShaderHandle_t CShaderDeviceDx11::CreatePixelShader(
    IShaderBuffer *pShaderBuffer) {
  // Create the pixel shader
  se::win::com::com_ptr<ID3D11PixelShader> pShader;
  HRESULT hr = m_d3d11_device->CreatePixelShader(
      pShaderBuffer->GetBits(), pShaderBuffer->GetSize(), nullptr, &pShader);
  if (FAILED(hr) || !pShader) return PIXEL_SHADER_HANDLE_INVALID;

  se::win::com::com_ptr<ID3D11ShaderReflection> pInfo;
  hr = D3DReflect(pShaderBuffer->GetBits(), pShaderBuffer->GetSize(),
                  IID_PPV_ARGS(&pInfo));
  if (FAILED(hr) || !pInfo) return PIXEL_SHADER_HANDLE_INVALID;

  // Insert the shader into the dictionary of shaders
  PixelShaderIndex_t i = m_PixelShaderDict.AddToTail();
  m_PixelShaderDict[i].m_pShader = std::move(pShader);
  m_PixelShaderDict[i].m_pInfo = std::move(pInfo);
  return (PixelShaderHandle_t)i;
}

void CShaderDeviceDx11::DestroyPixelShader(PixelShaderHandle_t hShader) {
  if (hShader == PIXEL_SHADER_HANDLE_INVALID) return;

  g_pShaderAPIDx11->Unbind(hShader);

  auto i = (PixelShaderIndex_t)hShader;
  m_PixelShaderDict[i].m_pShader.Release();
  m_PixelShaderDict[i].m_pInfo.Release();
  m_PixelShaderDict.Remove(i);
}

// Create, destroy hull shader
HullShaderHandle_t CShaderDeviceDx11::CreateHullShader(
    IShaderBuffer *pShaderBuffer) {
  // Create the hull shader
  se::win::com::com_ptr<ID3D11HullShader> pShader;
  HRESULT hr = m_d3d11_device->CreateHullShader(
      pShaderBuffer->GetBits(), pShaderBuffer->GetSize(), nullptr, &pShader);
  if (FAILED(hr) || !pShader) return HULL_SHADER_HANDLE_INVALID;

  se::win::com::com_ptr<ID3D11ShaderReflection> pInfo;
  hr = D3DReflect(pShaderBuffer->GetBits(), pShaderBuffer->GetSize(),
                  IID_PPV_ARGS(&pInfo));
  if (FAILED(hr) || !pInfo) return HULL_SHADER_HANDLE_INVALID;

  // Insert the shader into the dictionary of shaders
  auto i = m_HullShaderDict.AddToTail();
  m_HullShaderDict[i].m_pShader = std::move(pShader);
  m_HullShaderDict[i].m_pInfo = std::move(pInfo);
  return (HullShaderHandle_t)i;
}

void CShaderDeviceDx11::DestroyHullShader(HullShaderHandle_t hShader) {
  if (hShader == HULL_SHADER_HANDLE_INVALID) return;

  g_pShaderAPIDx11->Unbind(hShader);

  auto i = (HullShaderIndex_t)hShader;
  m_HullShaderDict[i].m_pShader.Release();
  m_HullShaderDict[i].m_pInfo.Release();
  m_HullShaderDict.Remove(i);
}

DomainShaderHandle_t CShaderDeviceDx11::CreateDomainShader(
    IShaderBuffer *pShaderBuffer) {
  // Create the domain shader
  se::win::com::com_ptr<ID3D11DomainShader> pShader;
  HRESULT hr = m_d3d11_device->CreateDomainShader(
      pShaderBuffer->GetBits(), pShaderBuffer->GetSize(), nullptr, &pShader);
  if (FAILED(hr) || !pShader) return DOMAIN_SHADER_HANDLE_INVALID;

  se::win::com::com_ptr<ID3D11ShaderReflection> pInfo;
  hr = D3DReflect(pShaderBuffer->GetBits(), pShaderBuffer->GetSize(),
                  IID_PPV_ARGS(&pInfo));
  if (FAILED(hr) || !pInfo) return DOMAIN_SHADER_HANDLE_INVALID;

  // Insert the shader into the dictionary of shaders
  DomainShaderIndex_t i = m_DomainShaderDict.AddToTail();
  m_DomainShaderDict[i].m_pShader = std::move(pShader);
  m_DomainShaderDict[i].m_pInfo = std::move(pInfo);
  return (DomainShaderHandle_t)i;
}

void CShaderDeviceDx11::DestroyDomainShader(DomainShaderHandle_t hShader) {
  if (hShader == DOMAIN_SHADER_HANDLE_INVALID) return;

  g_pShaderAPIDx11->Unbind(hShader);

  auto i = (DomainShaderIndex_t)hShader;
  m_DomainShaderDict[i].m_pShader.Release();
  m_DomainShaderDict[i].m_pInfo.Release();
  m_DomainShaderDict.Remove(i);
}

ComputeShaderHandle_t CShaderDeviceDx11::CreateComputeShader(
    IShaderBuffer *pShaderBuffer) {
  // Create the compute shader
  se::win::com::com_ptr<ID3D11ComputeShader> pShader;
  HRESULT hr = m_d3d11_device->CreateComputeShader(
      pShaderBuffer->GetBits(), pShaderBuffer->GetSize(), nullptr, &pShader);
  if (FAILED(hr) || !pShader) return COMPUTE_SHADER_HANDLE_INVALID;

  se::win::com::com_ptr<ID3D11ShaderReflection> pInfo;
  hr = D3DReflect(pShaderBuffer->GetBits(), pShaderBuffer->GetSize(),
                  IID_PPV_ARGS(&pInfo));
  if (FAILED(hr) || !pInfo) return COMPUTE_SHADER_HANDLE_INVALID;

  // Insert the shader into the dictionary of shaders
  ComputeShaderIndex_t i = m_ComputeShaderDict.AddToTail();
  m_ComputeShaderDict[i].m_pShader = std::move(pShader);
  m_ComputeShaderDict[i].m_pInfo = std::move(pInfo);
  return (ComputeShaderHandle_t)i;
}

void CShaderDeviceDx11::DestroyComputeShader(ComputeShaderHandle_t hShader) {
  if (hShader == COMPUTE_SHADER_HANDLE_INVALID) return;

  g_pShaderAPIDx11->Unbind(hShader);

  auto i = (ComputeShaderIndex_t)hShader;
  m_ComputeShaderDict[i].m_pShader.Release();
  m_ComputeShaderDict[i].m_pInfo.Release();
  m_ComputeShaderDict.Remove(i);
}

// Finds or creates an input layout for a given vertex shader + stream format
ID3D11InputLayout *CShaderDeviceDx11::GetInputLayout(
    VertexShaderHandle_t hShader, VertexFormat_t format) {
  if (hShader == VERTEX_SHADER_HANDLE_INVALID) return {};

  // FIXME: VertexFormat_t is not the appropriate way of specifying this
  // because it has no stream information
  InputLayout_t insert;
  insert.m_VertexFormat = format;

  auto i = (VertexShaderIndex_t)hShader;
  InputLayoutDict_t &dict = m_VertexShaderDict[i].m_InputLayouts;
  auto hIndex = dict.Find(insert);
  if (hIndex != dict.InvalidIndex()) return dict[hIndex].m_pInputLayout;

  VertexShader_t &shader = m_VertexShaderDict[i];
  insert.m_pInputLayout =
      std::move(CreateInputLayout(format, m_d3d11_device, shader.m_pInfo,
                                  shader.m_pByteCode, shader.m_nByteCodeLen));
  dict.Insert(insert);
  return insert.m_pInputLayout;
}

// Creates/destroys Mesh
IMesh *CShaderDeviceDx11::CreateStaticMesh(VertexFormat_t vertexFormat,
                                           const char *pBudgetGroup,
                                           IMaterial *pMaterial) {
  LOCK_SHADERAPI();
  return new CMeshDx11();
}

void CShaderDeviceDx11::DestroyStaticMesh(IMesh *pMesh) { LOCK_SHADERAPI(); }

// Creates/destroys vertex buffers + index buffers
IVertexBuffer *CShaderDeviceDx11::CreateVertexBuffer(ShaderBufferType_t type,
                                                     VertexFormat_t fmt,
                                                     int nVertexCount,
                                                     const char *pBudgetGroup) {
  LOCK_SHADERAPI();
  return new CVertexBufferDx11(m_d3d11_device, m_d3d11_immediate_context, type,
                               fmt, nVertexCount, pBudgetGroup);
}

void CShaderDeviceDx11::DestroyVertexBuffer(IVertexBuffer *vertex_buffer) {
  LOCK_SHADERAPI();
  if (vertex_buffer) {
    auto *buffer_dx11 = assert_cast<CVertexBufferDx11 *>(vertex_buffer);
    g_pShaderAPIDx11->UnbindVertexBuffer(buffer_dx11->GetDx11Buffer());
    delete buffer_dx11;
  }
}

IIndexBuffer *CShaderDeviceDx11::CreateIndexBuffer(ShaderBufferType_t type,
                                                   MaterialIndexFormat_t fmt,
                                                   int nIndexCount,
                                                   const char *pBudgetGroup) {
  LOCK_SHADERAPI();
  return new CIndexBufferDx11(m_d3d11_device, m_d3d11_immediate_context, type,
                              fmt, nIndexCount, pBudgetGroup);
}

void CShaderDeviceDx11::DestroyIndexBuffer(IIndexBuffer *index_buffer) {
  LOCK_SHADERAPI();
  if (index_buffer) {
    auto *buffer_dx11 = assert_cast<CIndexBufferDx11 *>(index_buffer);
    g_pShaderAPIDx11->UnbindIndexBuffer(buffer_dx11->GetDx11Buffer());
    delete buffer_dx11;
  }
}

IVertexBuffer *CShaderDeviceDx11::GetDynamicVertexBuffer(
    int nStreamID, VertexFormat_t vertexFormat, bool bBuffered) {
  LOCK_SHADERAPI();
  return nullptr;
}

IIndexBuffer *CShaderDeviceDx11::GetDynamicIndexBuffer(
    MaterialIndexFormat_t fmt, bool bBuffered) {
  LOCK_SHADERAPI();
  return nullptr;
}

const char *CShaderDeviceDx11::GetDisplayDeviceName() {
  if (m_display_device_name.IsEmpty()) {
    se::win::com::com_ptr<IDXGIOutput6> pOutput{
        g_ShaderDeviceMgrDx11.GetAdapterOutput(m_dxgi_adapter_index)};
    if (pOutput) {
      DXGI_OUTPUT_DESC1 desc;
      HRESULT hr{pOutput->GetDesc1(&desc)};
      if (FAILED(hr)) {
        Assert(false);
        V_wcscpy_safe(desc.DeviceName, L"Unknown");
      }

      char deviceName[std::size(desc.DeviceName)];
      Q_UnicodeToUTF8(desc.DeviceName, deviceName, sizeof(deviceName));

      m_display_device_name = deviceName;
    }
  }
  return m_display_device_name.Get();
}

bool CShaderDeviceDx11::DetermineHardwareCaps() {
  HardwareCaps_t &actual_caps = g_pHardwareConfig->ActualCapsForEdit();

  se::win::com::com_ptr<IDXGIAdapter4> adapter{
      g_ShaderDeviceMgrDx11.GetAdapter(m_dxgi_adapter_index)};
  se::win::com::com_ptr<IDXGIOutput6> output{
      g_ShaderDeviceMgrDx11.GetAdapterOutput(m_dxgi_adapter_index)};

  if (!g_ShaderDeviceMgrDx11.ComputeCapsFromD3D(&actual_caps, adapter,
                                                output)) {
    return false;
  }

  // See if the file tells us otherwise
  g_ShaderDeviceMgrDx11.ReadDXSupportLevels(actual_caps);

  // Read dxsupport.cfg which has config overrides for particular cards.
  g_ShaderDeviceMgrDx11.ReadHardwareCaps(actual_caps,
                                         actual_caps.m_nMaxDXSupportLevel);

  // What's in "-shader" overrides dxsupport.cfg
  const char *override{CommandLine()->ParmValue("-shader")};
  if (override) V_strcpy_safe(actual_caps.m_pShaderDLL, override);

  return true;
}

}  // namespace se::shaderapidx11
