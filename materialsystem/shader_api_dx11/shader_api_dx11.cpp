// Copyright Valve Corporation, All rights reserved.

#include "shader_api_dx11.h"

#include "shaderapibase.h"
#include "shaderapi/ishaderutil.h"

#include "materialsystem/idebugtextureinfo.h"
#include "materialsystem/materialsystem_config.h"

#include "shader_api_dx11_global.h"
#include "mesh_dx11.h"
#include "shader_shadow_dx11.h"
#include "shader_device_dx11.h"
#include "imaterialinternal.h"

#include "windows/com_error_category.h"

namespace se::shaderapidx11 {

// Methods related to queuing functions to be called prior to rendering
CFunctionCommit::CFunctionCommit() {
  m_pCommitFlags = NULL;
  m_nCommitBufferSize = 0;
}

CFunctionCommit::~CFunctionCommit() {
  delete[] m_pCommitFlags;
  m_pCommitFlags = NULL;
}

void CFunctionCommit::Init(int nFunctionCount) {
  m_nCommitBufferSize = (nFunctionCount + 7) >> 3;
  Assert(!m_pCommitFlags);
  m_pCommitFlags = new unsigned char[m_nCommitBufferSize];
  memset(m_pCommitFlags, 0, m_nCommitBufferSize);
}

//-----------------------------------------------------------------------------
// Methods related to queuing functions to be called per-(pMesh->Draw call) or
// per-pass
//-----------------------------------------------------------------------------
inline bool CFunctionCommit::IsCommitFuncInUse(int nFunc) const {
  Assert(nFunc >> 3 < m_nCommitBufferSize);
  return (m_pCommitFlags[nFunc >> 3] & (1 << (nFunc & 0x7))) != 0;
}

inline void CFunctionCommit::MarkCommitFuncInUse(int nFunc) {
  Assert(nFunc >> 3 < m_nCommitBufferSize);
  m_pCommitFlags[nFunc >> 3] |= 1 << (nFunc & 0x7);
}

inline void CFunctionCommit::AddCommitFunc(StateCommitFunc_t f) {
  m_CommitFuncs.AddToTail(f);
}

//-----------------------------------------------------------------------------
// Clears all commit functions
//-----------------------------------------------------------------------------
inline void CFunctionCommit::ClearAllCommitFuncs() {
  memset(m_pCommitFlags, 0, m_nCommitBufferSize);
  m_CommitFuncs.RemoveAll();
}

//-----------------------------------------------------------------------------
// Calls all commit functions in a particular list
//-----------------------------------------------------------------------------
void CFunctionCommit::CallCommitFuncs(ID3D11Device *pDevice,
                                      ID3D11DeviceContext *pDeviceContext,
                                      const ShaderStateDx11_t &desiredState,
                                      ShaderStateDx11_t &currentState,
                                      bool should_force) {
  for (auto &cf : m_CommitFuncs) {
    cf(pDevice, pDeviceContext, desiredState, currentState, should_force);
  }

  ClearAllCommitFuncs();
}

}  // namespace se::shaderapidx11

// Helpers for commit functions
#define ADD_COMMIT_FUNC(_func_name)                            \
  if (!m_Commit.IsCommitFuncInUse(COMMIT_FUNC_##_func_name)) { \
    m_Commit.AddCommitFunc(_func_name);                        \
    m_Commit.MarkCommitFuncInUse(COMMIT_FUNC_##_func_name);    \
  }

#define ADD_RENDERSTATE_FUNC_IUNKNOWN_STATE(_func_name, _state, _val) \
  if (m_bResettingRenderState || (m_DesiredState._state != _val)) {   \
    m_DesiredState._state.Attach(_val);                               \
    ADD_COMMIT_FUNC(_func_name)                                       \
  }

#define ADD_RENDERSTATE_FUNC_OTHER_STATE(_func_name, _state, _val)  \
  if (m_bResettingRenderState || (m_DesiredState._state != _val)) { \
    m_DesiredState._state = _val;                                   \
    ADD_COMMIT_FUNC(_func_name)                                     \
  }

#define IMPLEMENT_RENDERSTATE_FUNC_1_ARG(_func_name, _state, _d3dFunc)   \
  static void _func_name(                                                \
      ID3D11Device *pDevice, ID3D11DeviceContext *pDeviceContext,        \
      const se::shaderapidx11::ShaderStateDx11_t &desiredState,          \
      se::shaderapidx11::ShaderStateDx11_t &currentState, bool bForce) { \
    if (bForce || (desiredState._state != currentState._state)) {        \
      pDeviceContext->_d3dFunc(desiredState._state);                     \
      currentState._state = desiredState._state;                         \
    }                                                                    \
  }

#define IMPLEMENT_RENDERSTATE_FUNC_2_ARG(_func_name, _state, _d3dFunc, arg2) \
  static void _func_name(                                                    \
      ID3D11Device *pDevice, ID3D11DeviceContext *pDeviceContext,            \
      const se::shaderapidx11::ShaderStateDx11_t &desiredState,              \
      se::shaderapidx11::ShaderStateDx11_t &currentState, bool bForce) {     \
    if (bForce || (desiredState._state != currentState._state)) {            \
      pDeviceContext->_d3dFunc(desiredState._state, arg2);                   \
      currentState._state = desiredState._state;                             \
    }                                                                        \
  }

#define IMPLEMENT_RENDERSTATE_FUNC_3_ARG(_func_name, _state, _d3dFunc, arg2, \
                                         arg3)                               \
  static void _func_name(                                                    \
      ID3D11Device *pDevice, ID3D11DeviceContext *pDeviceContext,            \
      const se::shaderapidx11::ShaderStateDx11_t &desiredState,              \
      se::shaderapidx11::ShaderStateDx11_t &currentState, bool bForce) {     \
    if (bForce || (desiredState._state != currentState._state)) {            \
      pDeviceContext->_d3dFunc(desiredState._state, arg2, arg3);             \
      currentState._state = desiredState._state;                             \
    }                                                                        \
  }

namespace {

// D3D state setting methods

// NOTE: For each commit func you create, add to this enumeration.
enum CommitFunc_t {
  COMMIT_FUNC_CommitSetViewports = 0,
  COMMIT_FUNC_CommitSetVertexShader,
  COMMIT_FUNC_CommitSetGeometryShader,
  COMMIT_FUNC_CommitSetPixelShader,
  COMMIT_FUNC_CommitSetVertexBuffer,
  COMMIT_FUNC_CommitSetIndexBuffer,
  COMMIT_FUNC_CommitSetInputLayout,
  COMMIT_FUNC_CommitSetTopology,
  COMMIT_FUNC_CommitSetRasterState,
  COMMIT_FUNC_CommitSetHullShader,
  COMMIT_FUNC_CommitSetDomainShader,
  COMMIT_FUNC_CommitSetComputeShader,

  COMMIT_FUNC_COUNT,
};

IMPLEMENT_RENDERSTATE_FUNC_1_ARG(CommitSetTopology, m_Topology,
                                 IASetPrimitiveTopology)
IMPLEMENT_RENDERSTATE_FUNC_3_ARG(CommitSetVertexShader, m_pVertexShader,
                                 VSSetShader, nullptr, 0)
IMPLEMENT_RENDERSTATE_FUNC_3_ARG(CommitSetGeometryShader, m_pGeometryShader,
                                 GSSetShader, nullptr, 0)
IMPLEMENT_RENDERSTATE_FUNC_3_ARG(CommitSetPixelShader, m_pPixelShader,
                                 PSSetShader, nullptr, 0)
IMPLEMENT_RENDERSTATE_FUNC_3_ARG(CommitSetHullShader, m_pHullShader,
                                 HSSetShader, nullptr, 0)
IMPLEMENT_RENDERSTATE_FUNC_3_ARG(CommitSetDomainShader, m_pDomainShader,
                                 DSSetShader, nullptr, 0)
IMPLEMENT_RENDERSTATE_FUNC_3_ARG(CommitSetComputeShader, m_pComputeShader,
                                 CSSetShader, nullptr, 0)

void CommitSetInputLayout(
    ID3D11Device *pDevice, ID3D11DeviceContext *pDeviceContext,
    const se::shaderapidx11::ShaderStateDx11_t &desiredState,
    se::shaderapidx11::ShaderStateDx11_t &currentState, bool bForce) {
  const se::shaderapidx11::ShaderInputLayoutStateDx11_t &newState =
      desiredState.m_InputLayout;
  if (bForce ||
      memcmp(&newState, &currentState.m_InputLayout,
             sizeof(se::shaderapidx11::ShaderInputLayoutStateDx11_t))) {
    // FIXME: Deal with multiple streams
    ID3D11InputLayout *pInputLayout =
        se::shaderapidx11::g_pShaderDeviceDx11->GetInputLayout(
            newState.m_hVertexShader, newState.m_pVertexDecl[0]);
    pDeviceContext->IASetInputLayout(pInputLayout);

    currentState.m_InputLayout = newState;
  }
}

void CommitSetViewports(
    ID3D11Device *pDevice, ID3D11DeviceContext *pDeviceContext,
    const se::shaderapidx11::ShaderStateDx11_t &desiredState,
    se::shaderapidx11::ShaderStateDx11_t &currentState, bool bForce) {
  bool bChanged = bForce || (desiredState.m_nViewportCount !=
                             currentState.m_nViewportCount);
  if (!bChanged && desiredState.m_nViewportCount > 0) {
    bChanged =
        memcmp(desiredState.m_pViewports, currentState.m_pViewports,
               desiredState.m_nViewportCount * sizeof(D3D11_VIEWPORT)) != 0;
  }

  if (!bChanged) return;

  pDeviceContext->RSSetViewports(desiredState.m_nViewportCount,
                                 desiredState.m_pViewports);
  currentState.m_nViewportCount = desiredState.m_nViewportCount;

#ifdef _DEBUG
  memset(currentState.m_pViewports, 0xDD, sizeof(currentState.m_pViewports));
#endif

  memcpy(currentState.m_pViewports, desiredState.m_pViewports,
         desiredState.m_nViewportCount * sizeof(D3D11_VIEWPORT));
}

void CommitSetIndexBuffer(
    ID3D11Device *pDevice, ID3D11DeviceContext *pDeviceContext,
    const se::shaderapidx11::ShaderStateDx11_t &desiredState,
    se::shaderapidx11::ShaderStateDx11_t &currentState, bool bForce) {
  const se::shaderapidx11::ShaderIndexBufferStateDx11_t &newState =
      desiredState.m_IndexBuffer;
  bool bChanged =
      bForce || memcmp(&newState, &currentState.m_IndexBuffer,
                       sizeof(se::shaderapidx11::ShaderIndexBufferStateDx11_t));
  if (!bChanged) return;

  pDeviceContext->IASetIndexBuffer(newState.m_pBuffer, newState.m_Format,
                                   newState.m_nOffset);
  memcpy(&currentState.m_IndexBuffer, &newState,
         sizeof(se::shaderapidx11::ShaderIndexBufferStateDx11_t));
}

void CommitSetVertexBuffer(
    ID3D11Device *pDevice, ID3D11DeviceContext *pDeviceContext,
    const se::shaderapidx11::ShaderStateDx11_t &desiredState,
    se::shaderapidx11::ShaderStateDx11_t &currentState, bool bForce) {
  ID3D11Buffer *ppVertexBuffers[MAX_DX11_STREAMS];
  UINT pStrides[MAX_DX11_STREAMS];
  UINT pOffsets[MAX_DX11_STREAMS];

  UINT nFirstBuffer = 0;
  UINT nBufferCount = 0;
  bool bInMatch = true;
  for (int i = 0; i < MAX_DX11_STREAMS; ++i) {
    const se::shaderapidx11::ShaderVertexBufferStateDx11_t &newState =
        desiredState.m_pVertexBuffer[i];
    bool bMatch =
        !bForce &&
        !memcmp(&newState, &currentState.m_pVertexBuffer[i],
                sizeof(se::shaderapidx11::ShaderVertexBufferStateDx11_t));
    if (!bMatch) {
      ppVertexBuffers[i] = newState.m_pBuffer;
      pStrides[i] = newState.m_nStride;
      pOffsets[i] = newState.m_nOffset;
      ++nBufferCount;
      memcpy(&currentState.m_pVertexBuffer[i], &newState,
             sizeof(se::shaderapidx11::ShaderVertexBufferStateDx11_t));
    }

    if (bInMatch) {
      if (!bMatch) {
        bInMatch = false;
        nFirstBuffer = i;
      }
      continue;
    }

    if (bMatch) {
      bInMatch = true;
      pDeviceContext->IASetVertexBuffers(
          nFirstBuffer, nBufferCount, &ppVertexBuffers[nFirstBuffer],
          &pStrides[nFirstBuffer], &pOffsets[nFirstBuffer]);
      nBufferCount = 0;
    }
  }

  if (!bInMatch) {
    pDeviceContext->IASetVertexBuffers(
        nFirstBuffer, nBufferCount, &ppVertexBuffers[nFirstBuffer],
        &pStrides[nFirstBuffer], &pOffsets[nFirstBuffer]);
  }
}

void GenerateRasterizerDesc(D3D11_RASTERIZER_DESC *pDesc,
                            const ShaderRasterState_t &state) {
  pDesc->FillMode = (state.m_FillMode == SHADER_FILL_WIREFRAME)
                        ? D3D11_FILL_WIREFRAME
                        : D3D11_FILL_SOLID;

  // Cull state
  if (state.m_bCullEnable) {
    pDesc->CullMode = D3D11_CULL_NONE;
  } else {
    pDesc->CullMode = (state.m_CullMode == MATERIAL_CULLMODE_CW)
                          ? D3D11_CULL_BACK
                          : D3D11_CULL_FRONT;
  }
  pDesc->FrontCounterClockwise = TRUE;

  // Depth bias state
  if (!state.m_bDepthBias) {
    pDesc->DepthBias = 0;
    pDesc->DepthBiasClamp = 0.0f;
    pDesc->SlopeScaledDepthBias = 0.0f;
    pDesc->DepthClipEnable = FALSE;
  } else {
    // FIXME: Implement! Read ConVars
  }

  pDesc->ScissorEnable = state.m_bScissorEnable ? TRUE : FALSE;
  pDesc->MultisampleEnable = state.m_bMultisampleEnable ? TRUE : FALSE;
  pDesc->AntialiasedLineEnable = FALSE;
}

void CommitSetRasterState(
    ID3D11Device *pDevice, ID3D11DeviceContext *pDeviceContext,
    const se::shaderapidx11::ShaderStateDx11_t &desiredState,
    se::shaderapidx11::ShaderStateDx11_t &currentState, bool bForce) {
  const ShaderRasterState_t &newState = desiredState.m_RasterState;
  if (bForce || memcmp(&newState, &currentState.m_RasterState,
                       sizeof(ShaderRasterState_t))) {
    // Clear out the existing state
    if (currentState.m_pRasterState) {
      currentState.m_pRasterState.Release();
    }

    D3D11_RASTERIZER_DESC desc;
    GenerateRasterizerDesc(&desc, newState);

    // NOTE: This does a search for existing matching state objects
    se::win::com::com_ptr<ID3D11RasterizerState> rasterizer_state;
    HRESULT hr = pDevice->CreateRasterizerState(&desc, &rasterizer_state);
    if (!FAILED(hr)) {
      Warning("ID3D11Device::CreateRasterizerState failed w/e %s.\n",
              se::win::com::com_error_category().message(hr).c_str());
    }

    pDeviceContext->RSSetState(rasterizer_state);
    currentState.m_pRasterState = std::move(rasterizer_state);

    BitwiseCopy(&newState, &currentState.m_RasterState, 1);
  }
}

static se::shaderapidx11::CShaderAPIDx11 s_ShaderAPIDx11;

}  // namespace

namespace se::shaderapidx11 {

CShaderAPIDx11 *g_pShaderAPIDx11 = &s_ShaderAPIDx11;

}  // namespace se::shaderapidx11

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CShaderAPIDx11, IShaderAPI,
                                  SHADERAPI_INTERFACE_VERSION, s_ShaderAPIDx11)

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CShaderAPIDx11, IDebugTextureInfo,
                                  DEBUG_TEXTURE_INFO_VERSION, s_ShaderAPIDx11)

namespace se::shaderapidx11 {

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CShaderAPIDx11::CShaderAPIDx11() {
  m_bResettingRenderState = false;
  m_Commit.Init(COMMIT_FUNC_COUNT);
  ClearShaderState(&m_DesiredState);
  ClearShaderState(&m_CurrentState);
}

CShaderAPIDx11::~CShaderAPIDx11() {}

//-----------------------------------------------------------------------------
// Clears the shader state to a well-defined value
//-----------------------------------------------------------------------------
void CShaderAPIDx11::ClearShaderState(ShaderStateDx11_t *pState) {
  memset(pState, 0, sizeof(ShaderStateDx11_t));
}

//-----------------------------------------------------------------------------
// Resets the render state
//-----------------------------------------------------------------------------
void CShaderAPIDx11::ResetRenderState(bool bFullReset) {
  D3D11_RASTERIZER_DESC rasterizer_desc;
  BitwiseClear(rasterizer_desc);

  rasterizer_desc.FillMode = D3D11_FILL_SOLID;
  rasterizer_desc.CullMode = D3D11_CULL_NONE;
  rasterizer_desc.FrontCounterClockwise = TRUE;  // right-hand rule

  ID3D11RasterizerState *rasterizer_state;
  HRESULT hr = g_pShaderDeviceDx11->D3D11Device()->CreateRasterizerState(
      &rasterizer_desc, &rasterizer_state);
  Assert(!FAILED(hr));
  SetDebugName(rasterizer_state, "Rasterizer State");
  g_pShaderDeviceDx11->D3D11DeviceContext()->RSSetState(rasterizer_state);

  D3D11_DEPTH_STENCIL_DESC depth_stencil_desc;
  memset(&depth_stencil_desc, 0, sizeof(depth_stencil_desc));

  ID3D11DepthStencilState *depth_stencil_state;
  hr = g_pShaderDeviceDx11->D3D11Device()->CreateDepthStencilState(
      &depth_stencil_desc, &depth_stencil_state);
  Assert(!FAILED(hr));
  SetDebugName(depth_stencil_state, "Depth Stencil State");
  g_pShaderDeviceDx11->D3D11DeviceContext()->OMSetDepthStencilState(
      depth_stencil_state, 0);

  D3D11_BLEND_DESC blend_desc;
  BitwiseClear(blend_desc);

  blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
  blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
  blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
  blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blend_desc.RenderTarget[0].RenderTargetWriteMask =
      D3D11_COLOR_WRITE_ENABLE_ALL;

  const FLOAT blend_factor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  ID3D11BlendState *blend_state;
  hr = g_pShaderDeviceDx11->D3D11Device()->CreateBlendState(&blend_desc,
                                                            &blend_state);
  Assert(!FAILED(hr));
  SetDebugName(blend_state, "Blend State");
  g_pShaderDeviceDx11->D3D11DeviceContext()->OMSetBlendState(
      blend_state, blend_factor, 0xFFFFFFFF);
}

//-----------------------------------------------------------------------------
// Commits queued-up state change requests
//-----------------------------------------------------------------------------
void CShaderAPIDx11::CommitStateChanges(bool should_force) {
  // Don't bother committing anything if we're deactivated
  if (g_pShaderDevice->IsDeactivated()) return;

  m_Commit.CallCommitFuncs(g_pShaderDeviceDx11->D3D11Device(),
                           g_pShaderDeviceDx11->D3D11DeviceContext(),
                           m_DesiredState, m_CurrentState, should_force);
}

void CShaderAPIDx11::GetBackBufferDimensions(int &nWidth, int &nHeight) const {
  g_pShaderDeviceDx11->GetBackBufferDimensions(nWidth, nHeight);
}

void CShaderAPIDx11::SetViewports(int nCount,
                                  const ShaderViewport_t *pViewports) {
  nCount = min(nCount, MAX_DX11_VIEWPORTS);
  m_DesiredState.m_nViewportCount = nCount;

  for (int i = 0; i < nCount; ++i) {
    Assert(pViewports[i].m_nVersion == SHADER_VIEWPORT_VERSION);

    D3D11_VIEWPORT &viewport = m_DesiredState.m_pViewports[i];
    viewport.TopLeftX = pViewports[i].m_nTopLeftX;
    viewport.TopLeftY = pViewports[i].m_nTopLeftY;
    viewport.Width = pViewports[i].m_nWidth;
    viewport.Height = pViewports[i].m_nHeight;
    viewport.MinDepth = pViewports[i].m_flMinZ;
    viewport.MaxDepth = pViewports[i].m_flMaxZ;
  }

  ADD_COMMIT_FUNC(CommitSetViewports);
}

int CShaderAPIDx11::GetViewports(ShaderViewport_t *pViewports, int nMax) const {
  int nCount = m_DesiredState.m_nViewportCount;
  if (pViewports && nMax) {
    nCount = min(nCount, nMax);
    memcpy(pViewports, m_DesiredState.m_pViewports,
           nCount * sizeof(ShaderViewport_t));
  }
  return nCount;
}

void CShaderAPIDx11::SetRasterState(const ShaderRasterState_t &state) {
  if (memcmp(&state, &m_DesiredState.m_RasterState,
             sizeof(ShaderRasterState_t))) {
    memcpy(&m_DesiredState.m_RasterState, &state, sizeof(ShaderRasterState_t));
    ADD_COMMIT_FUNC(CommitSetRasterState);
  }
}

void CShaderAPIDx11::ClearColor3ub(unsigned char r, unsigned char g,
                                   unsigned char b) {
  m_DesiredState.m_ClearColor[0] = r / 255.0f;
  m_DesiredState.m_ClearColor[1] = g / 255.0f;
  m_DesiredState.m_ClearColor[2] = b / 255.0f;
  m_DesiredState.m_ClearColor[3] = 1.0f;
}

void CShaderAPIDx11::ClearColor4ub(unsigned char r, unsigned char g,
                                   unsigned char b, unsigned char a) {
  m_DesiredState.m_ClearColor[0] = r / 255.0f;
  m_DesiredState.m_ClearColor[1] = g / 255.0f;
  m_DesiredState.m_ClearColor[2] = b / 255.0f;
  m_DesiredState.m_ClearColor[3] = a / 255.0f;
}

void CShaderAPIDx11::ClearBuffers(bool bClearColor, bool bClearDepth,
                                  bool bClearStencil, int renderTargetWidth,
                                  int renderTargetHeight) {
  // NOTE: State change commit isn't necessary since clearing doesn't use state
  //	CommitStateChanges();

  // FIXME: This implementation is totally bust0red [doesn't guarantee exact
  // color specified]
  if (bClearColor) {
    g_pShaderDeviceDx11->D3D11DeviceContext()->ClearRenderTargetView(
        g_pShaderDeviceDx11->D3D11RenderTargetView(),
        m_DesiredState.m_ClearColor);
  }
}

void CShaderAPIDx11::BindVertexShader(VertexShaderHandle_t hVertexShader) {
  ID3D11VertexShader *pVertexShader =
      g_pShaderDeviceDx11->GetVertexShader(hVertexShader);
  ADD_RENDERSTATE_FUNC_IUNKNOWN_STATE(CommitSetVertexShader, m_pVertexShader,
                                      pVertexShader);

  if (m_bResettingRenderState ||
      (m_DesiredState.m_InputLayout.m_hVertexShader != hVertexShader)) {
    m_DesiredState.m_InputLayout.m_hVertexShader = hVertexShader;
    ADD_COMMIT_FUNC(CommitSetInputLayout);
  }
}

void CShaderAPIDx11::BindGeometryShader(
    GeometryShaderHandle_t hGeometryShader) {
  ID3D11GeometryShader *pGeometryShader =
      g_pShaderDeviceDx11->GetGeometryShader(hGeometryShader);
  ADD_RENDERSTATE_FUNC_IUNKNOWN_STATE(CommitSetGeometryShader,
                                      m_pGeometryShader, pGeometryShader);
}

void CShaderAPIDx11::BindPixelShader(PixelShaderHandle_t hPixelShader) {
  ID3D11PixelShader *pPixelShader =
      g_pShaderDeviceDx11->GetPixelShader(hPixelShader);
  ADD_RENDERSTATE_FUNC_IUNKNOWN_STATE(CommitSetPixelShader, m_pPixelShader,
                                      pPixelShader);
}

void CShaderAPIDx11::BindHullShader(HullShaderHandle_t hHullShader) {
  ID3D11HullShader *pHullShader =
      g_pShaderDeviceDx11->GetHullShader(hHullShader);
  ADD_RENDERSTATE_FUNC_IUNKNOWN_STATE(CommitSetHullShader, m_pHullShader,
                                      pHullShader);
}

void CShaderAPIDx11::BindDomainShader(DomainShaderHandle_t hDomainShader) {
  ID3D11DomainShader *pDomainShader =
      g_pShaderDeviceDx11->GetDomainShader(hDomainShader);
  ADD_RENDERSTATE_FUNC_IUNKNOWN_STATE(CommitSetDomainShader, m_pDomainShader,
                                      pDomainShader);
}

void CShaderAPIDx11::BindComputeShader(ComputeShaderHandle_t hComputeShader) {
  ID3D11ComputeShader *pComputeShader =
      g_pShaderDeviceDx11->GetComputeShader(hComputeShader);
  ADD_RENDERSTATE_FUNC_IUNKNOWN_STATE(CommitSetComputeShader, m_pComputeShader,
                                      pComputeShader);
}

void CShaderAPIDx11::BindVertexBuffer(int nStreamID,
                                      IVertexBuffer *pVertexBuffer,
                                      int nOffsetInBytes, int nFirstVertex,
                                      int nVertexCount, VertexFormat_t fmt,
                                      int nRepetitions) {
  // FIXME: What to do about repetitions?
  auto *pVertexBufferDx11 = assert_cast<CVertexBufferDx11 *>(pVertexBuffer);

  ShaderVertexBufferStateDx11_t state;
  if (pVertexBufferDx11) {
    state.m_pBuffer = pVertexBufferDx11->GetDx11Buffer();
    state.m_nStride = pVertexBufferDx11->VertexSize();
  } else {
    state.m_pBuffer = {};
  }
  state.m_nOffset = nOffsetInBytes;

  if (m_bResettingRenderState ||
      memcmp(&m_DesiredState.m_pVertexBuffer[nStreamID], &state,
             sizeof(ShaderVertexBufferStateDx11_t))) {
    m_DesiredState.m_pVertexBuffer[nStreamID] = state;
    ADD_COMMIT_FUNC(CommitSetVertexBuffer);
  }

  if (m_bResettingRenderState ||
      (m_DesiredState.m_InputLayout.m_pVertexDecl[nStreamID] != fmt)) {
    m_DesiredState.m_InputLayout.m_pVertexDecl[nStreamID] = fmt;
    ADD_COMMIT_FUNC(CommitSetInputLayout);
  }
}

void CShaderAPIDx11::BindIndexBuffer(IIndexBuffer *pIndexBuffer,
                                     int nOffsetInBytes) {
  CIndexBufferDx11 *pIndexBufferDx11 =
      static_cast<CIndexBufferDx11 *>(pIndexBuffer);

  ShaderIndexBufferStateDx11_t state;
  if (pIndexBufferDx11) {
    state.m_pBuffer = pIndexBufferDx11->GetDx11Buffer();
    state.m_Format =
        (pIndexBufferDx11->GetIndexFormat() == MATERIAL_INDEX_FORMAT_16BIT)
            ? DXGI_FORMAT_R16_UINT
            : DXGI_FORMAT_R32_UINT;
  } else {
    state.m_pBuffer = {};
    state.m_Format = DXGI_FORMAT_R16_UINT;
  }
  state.m_nOffset = nOffsetInBytes;

  ADD_RENDERSTATE_FUNC_OTHER_STATE(CommitSetIndexBuffer, m_IndexBuffer, state);
}

// Unbinds resources because they are about to be deleted
void CShaderAPIDx11::Unbind(VertexShaderHandle_t hShader) {
  ID3D11VertexShader *pShader = g_pShaderDeviceDx11->GetVertexShader(hShader);
  Assert(pShader);
  if (m_DesiredState.m_pVertexShader == pShader) {
    BindVertexShader(VERTEX_SHADER_HANDLE_INVALID);
  }
  if (m_CurrentState.m_pVertexShader == pShader) {
    CommitStateChanges();
  }
}

void CShaderAPIDx11::Unbind(GeometryShaderHandle_t hShader) {
  ID3D11GeometryShader *pShader =
      g_pShaderDeviceDx11->GetGeometryShader(hShader);
  Assert(pShader);
  if (m_DesiredState.m_pGeometryShader == pShader) {
    BindGeometryShader(GEOMETRY_SHADER_HANDLE_INVALID);
  }
  if (m_CurrentState.m_pGeometryShader == pShader) {
    CommitStateChanges();
  }
}

void CShaderAPIDx11::Unbind(PixelShaderHandle_t hShader) {
  ID3D11PixelShader *pShader = g_pShaderDeviceDx11->GetPixelShader(hShader);
  Assert(pShader);
  if (m_DesiredState.m_pPixelShader == pShader) {
    BindPixelShader(PIXEL_SHADER_HANDLE_INVALID);
  }
  if (m_CurrentState.m_pPixelShader == pShader) {
    CommitStateChanges();
  }
}

void CShaderAPIDx11::Unbind(HullShaderHandle_t hShader) {
  ID3D11HullShader *pShader = g_pShaderDeviceDx11->GetHullShader(hShader);
  Assert(pShader);
  if (m_DesiredState.m_pHullShader == pShader) {
    BindHullShader(HULL_SHADER_HANDLE_INVALID);
  }
  if (m_CurrentState.m_pHullShader == pShader) {
    CommitStateChanges();
  }
}

void CShaderAPIDx11::Unbind(DomainShaderHandle_t hShader) {
  ID3D11DomainShader *pShader = g_pShaderDeviceDx11->GetDomainShader(hShader);
  Assert(pShader);
  if (m_DesiredState.m_pDomainShader == pShader) {
    BindDomainShader(DOMAIN_SHADER_HANDLE_INVALID);
  }
  if (m_CurrentState.m_pDomainShader == pShader) {
    CommitStateChanges();
  }
}

void CShaderAPIDx11::Unbind(ComputeShaderHandle_t hShader) {
  ID3D11ComputeShader *pShader = g_pShaderDeviceDx11->GetComputeShader(hShader);
  Assert(pShader);
  if (m_DesiredState.m_pComputeShader == pShader) {
    BindComputeShader(COMPUTE_SHADER_HANDLE_INVALID);
  }
  if (m_CurrentState.m_pComputeShader == pShader) {
    CommitStateChanges();
  }
}

void CShaderAPIDx11::UnbindVertexBuffer(ID3D11Buffer *pBuffer) {
  Assert(pBuffer);

  for (int i = 0; i < MAX_DX11_STREAMS; ++i) {
    if (m_DesiredState.m_pVertexBuffer[i].m_pBuffer == pBuffer) {
      BindVertexBuffer(i, NULL, 0, 0, 0, VERTEX_POSITION, 0);
    }
  }
  for (int i = 0; i < MAX_DX11_STREAMS; ++i) {
    if (m_CurrentState.m_pVertexBuffer[i].m_pBuffer == pBuffer) {
      CommitStateChanges();
      break;
    }
  }
}

void CShaderAPIDx11::UnbindIndexBuffer(ID3D11Buffer *pBuffer) {
  Assert(pBuffer);

  if (m_DesiredState.m_IndexBuffer.m_pBuffer == pBuffer) {
    BindIndexBuffer(NULL, 0);
  }
  if (m_CurrentState.m_IndexBuffer.m_pBuffer == pBuffer) {
    CommitStateChanges();
  }
}

// Sets the topology state
void CShaderAPIDx11::SetTopology(MaterialPrimitiveType_t topology) {
  D3D11_PRIMITIVE_TOPOLOGY d3dTopology;
  switch (topology) {
    case MATERIAL_POINTS:
      d3dTopology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
      break;

    case MATERIAL_LINES:
      d3dTopology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
      break;

    case MATERIAL_TRIANGLES:
      d3dTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
      break;

    case MATERIAL_TRIANGLE_STRIP:
      d3dTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
      break;

    case MATERIAL_LINE_STRIP:
      d3dTopology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
      break;

    default:
    case MATERIAL_LINE_LOOP:
    case MATERIAL_POLYGON:
    case MATERIAL_QUADS:
      Assert(0);
      d3dTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
      break;
  }

  ADD_RENDERSTATE_FUNC_OTHER_STATE(CommitSetTopology, m_Topology, d3dTopology);
}

// Main entry point for rendering
void CShaderAPIDx11::Draw(MaterialPrimitiveType_t primitiveType,
                          int nFirstIndex, int nIndexCount) {
  SetTopology(primitiveType);

  CommitStateChanges();

  Assert(nFirstIndex >= 0);
  Assert(nIndexCount > 0);

  // FIXME: How do I set the base vertex location!?
  g_pShaderDeviceDx11->D3D11DeviceContext()->DrawIndexed((UINT)nIndexCount,
                                                         (UINT)nFirstIndex, 0);
}

// Abandon all hope below this point

bool CShaderAPIDx11::DoRenderTargetsNeedSeparateDepthBuffer() const {
  return false;
}

bool CShaderAPIDx11::SetMode(void *hwnd, unsigned nAdapter,
                             const ShaderDeviceInfo_t &info) {
  return g_pShaderDeviceMgr->SetMode(hwnd, nAdapter, info);
}

// Can we download textures?
bool CShaderAPIDx11::CanDownloadTextures() const {
  // TODO(dimhotepus): Check device alive.
  return !g_pShaderDeviceDx11->IsDeactivated();
}

// Used to clear the transition table when we know it's become invalid.
void CShaderAPIDx11::ClearSnapshots() {}

// Sets the default *dynamic* state
void CShaderAPIDx11::SetDefaultState() {}

// Returns the snapshot id for the shader state
StateSnapshot_t CShaderAPIDx11::TakeSnapshot() {
  StateSnapshot_t id = 0;
  if (g_pShaderShadowDx11->m_IsTranslucent) id |= TRANSLUCENT;
  if (g_pShaderShadowDx11->m_IsAlphaTested) id |= ALPHATESTED;
  if (g_pShaderShadowDx11->m_bUsesVertexAndPixelShaders)
    id |= VERTEX_AND_PIXEL_SHADERS;
  if (g_pShaderShadowDx11->m_bIsDepthWriteEnabled) id |= DEPTHWRITE;
  return id;
}

// Returns true if the state snapshot is transparent
bool CShaderAPIDx11::IsTranslucent(StateSnapshot_t id) const {
  return (id & TRANSLUCENT) != 0;
}

bool CShaderAPIDx11::IsAlphaTested(StateSnapshot_t id) const {
  return (id & ALPHATESTED) != 0;
}

bool CShaderAPIDx11::IsDepthWriteEnabled(StateSnapshot_t id) const {
  return (id & DEPTHWRITE) != 0;
}

bool CShaderAPIDx11::UsesVertexAndPixelShaders(StateSnapshot_t id) const {
  return (id & VERTEX_AND_PIXEL_SHADERS) != 0;
}

// Gets the vertex format for a set of snapshot ids
VertexFormat_t CShaderAPIDx11::ComputeVertexFormat(
    int numSnapshots, StateSnapshot_t *pIds) const {
  return 0;
}

// Gets the vertex format for a set of snapshot ids
VertexFormat_t CShaderAPIDx11::ComputeVertexUsage(int numSnapshots,
                                                  StateSnapshot_t *pIds) const {
  return 0;
}

// Uses a state snapshot
void CShaderAPIDx11::UseSnapshot(StateSnapshot_t snapshot) {}

// Sets the color to modulate by
void CShaderAPIDx11::Color3f(float r, float g, float b) {}

void CShaderAPIDx11::Color3fv(float const *pColor) {}

void CShaderAPIDx11::Color4f(float r, float g, float b, float a) {}

void CShaderAPIDx11::Color4fv(float const *pColor) {}

// Faster versions of color
void CShaderAPIDx11::Color3ub(unsigned char r, unsigned char g,
                              unsigned char b) {}

void CShaderAPIDx11::Color3ubv(unsigned char const *rgb) {}

void CShaderAPIDx11::Color4ub(unsigned char r, unsigned char g, unsigned char b,
                              unsigned char a) {}

void CShaderAPIDx11::Color4ubv(unsigned char const *rgba) {}

void CShaderAPIDx11::GetStandardTextureDimensions(int *pWidth, int *pHeight,
                                                  StandardTextureId_t id) {
  ShaderUtil()->GetStandardTextureDimensions(pWidth, pHeight, id);
}

// The shade mode
void CShaderAPIDx11::ShadeMode(ShaderShadeMode_t mode) {}

// Binds a particular material to render with
void CShaderAPIDx11::Bind(IMaterial *pMaterial) {}

// Cull mode
void CShaderAPIDx11::CullMode(MaterialCullMode_t cullMode) {}

void CShaderAPIDx11::ForceDepthFuncEquals(bool bEnable) {}

// Forces Z buffering on or off
void CShaderAPIDx11::OverrideDepthEnable(bool bEnable, bool bDepthEnable) {}

void CShaderAPIDx11::OverrideAlphaWriteEnable(bool bOverrideEnable,
                                              bool bAlphaWriteEnable) {}

void CShaderAPIDx11::OverrideColorWriteEnable(bool bOverrideEnable,
                                              bool bColorWriteEnable) {}

// legacy fast clipping linkage
void CShaderAPIDx11::SetHeightClipZ(float z) {}

void CShaderAPIDx11::SetHeightClipMode(
    enum MaterialHeightClipMode_t heightClipMode) {}

// Sets the lights
void CShaderAPIDx11::SetLight(int lightNum, const LightDesc_t &desc) {}

void CShaderAPIDx11::SetAmbientLight(float r, float g, float b) {}

void CShaderAPIDx11::SetAmbientLightCube(Vector4D cube[6]) {}

// Get lights
int CShaderAPIDx11::GetMaxLights(void) const { return 0; }

const LightDesc_t &CShaderAPIDx11::GetLight(int lightNum) const {
  static LightDesc_t blah;
  return blah;
}

// Render state for the ambient light cube (vertex shaders)
void CShaderAPIDx11::SetVertexShaderStateAmbientLightCube() {}

void CShaderAPIDx11::SetSkinningMatrices() {}

// Lightmap texture binding
void CShaderAPIDx11::BindLightmap(TextureStage_t stage) {}

void CShaderAPIDx11::BindBumpLightmap(TextureStage_t stage) {}

void CShaderAPIDx11::BindFullbrightLightmap(TextureStage_t stage) {}

void CShaderAPIDx11::BindWhite(TextureStage_t stage) {}

void CShaderAPIDx11::BindBlack(TextureStage_t stage) {}

void CShaderAPIDx11::BindGrey(TextureStage_t stage) {}

// Gets the lightmap dimensions
void CShaderAPIDx11::GetLightmapDimensions(int *w, int *h) {
  g_pShaderUtil->GetLightmapDimensions(w, h);
}

// Special system flat normal map binding.
void CShaderAPIDx11::BindFlatNormalMap(TextureStage_t stage) {}

void CShaderAPIDx11::BindNormalizationCubeMap(TextureStage_t stage) {}

void CShaderAPIDx11::BindSignedNormalizationCubeMap(TextureStage_t stage) {}

void CShaderAPIDx11::BindFBTexture(TextureStage_t stage, int textureIndex) {}

// Flushes any primitives that are buffered
void CShaderAPIDx11::FlushBufferedPrimitives() {}

// Creates/destroys Mesh
IMesh *CShaderAPIDx11::CreateStaticMesh(VertexFormat_t fmt,
                                        const char *pTextureBudgetGroup,
                                        IMaterial *pMaterial) {
  return &m_Mesh;
}

void CShaderAPIDx11::DestroyStaticMesh(IMesh *mesh) {}

// Gets the dynamic mesh; note that you've got to render the mesh
// before calling this function a second time. Clients should *not*
// call DestroyStaticMesh on the mesh returned by this call.
IMesh *CShaderAPIDx11::GetDynamicMesh(IMaterial *pMaterial,
                                      int nHWSkinBoneCount, bool buffered,
                                      IMesh *pVertexOverride,
                                      IMesh *pIndexOverride) {
  Assert((pMaterial == NULL) ||
         ((IMaterialInternal *)pMaterial)->IsRealTimeVersion());
  return &m_Mesh;
}

IMesh *CShaderAPIDx11::GetDynamicMeshEx(IMaterial *pMaterial,
                                        VertexFormat_t fmt,
                                        int nHWSkinBoneCount, bool buffered,
                                        IMesh *pVertexOverride,
                                        IMesh *pIndexOverride) {
  // UNDONE: support compressed dynamic meshes if needed (pro: less VB memory,
  // con: time spent compressing)
  Assert(CompressionType(pVertexOverride->GetVertexFormat()) !=
         VERTEX_COMPRESSION_NONE);
  Assert((pMaterial == NULL) ||
         ((IMaterialInternal *)pMaterial)->IsRealTimeVersion());
  return &m_Mesh;
}

IMesh *CShaderAPIDx11::GetFlexMesh() { return &m_Mesh; }

// Begins a rendering pass that uses a state snapshot
void CShaderAPIDx11::BeginPass(StateSnapshot_t snapshot) {}

// Renders a single pass of a material
void CShaderAPIDx11::RenderPass(int nPass, int nPassCount) {}

// stuff related to matrix stacks
void CShaderAPIDx11::MatrixMode(MaterialMatrixMode_t matrixMode) {}

void CShaderAPIDx11::PushMatrix() {}

void CShaderAPIDx11::PopMatrix() {}

void CShaderAPIDx11::LoadMatrix(float *m) {}

void CShaderAPIDx11::MultMatrix(float *m) {}

void CShaderAPIDx11::MultMatrixLocal(float *m) {}

void CShaderAPIDx11::GetMatrix(MaterialMatrixMode_t matrixMode, float *dst) {}

void CShaderAPIDx11::LoadIdentity(void) {}

void CShaderAPIDx11::LoadCameraToWorld(void) {}

void CShaderAPIDx11::Ortho(double left, double top, double right, double bottom,
                           double zNear, double zFar) {}

void CShaderAPIDx11::PerspectiveX(double fovx, double aspect, double zNear,
                                  double zFar) {}

void CShaderAPIDx11::PerspectiveOffCenterX(double fovx, double aspect,
                                           double zNear, double zFar,
                                           double bottom, double top,
                                           double left, double right) {}

void CShaderAPIDx11::PickMatrix(int x, int y, int width, int height) {}

void CShaderAPIDx11::Rotate(float angle, float x, float y, float z) {}

void CShaderAPIDx11::Translate(float x, float y, float z) {}

void CShaderAPIDx11::Scale(float x, float y, float z) {}

void CShaderAPIDx11::ScaleXY(float x, float y) {}

// Fog methods...
void CShaderAPIDx11::FogMode(MaterialFogMode_t fogMode) {}

void CShaderAPIDx11::FogStart(float fStart) {}

void CShaderAPIDx11::FogEnd(float fEnd) {}

void CShaderAPIDx11::SetFogZ(float fogZ) {}

void CShaderAPIDx11::FogMaxDensity(float flMaxDensity) {}

void CShaderAPIDx11::GetFogDistances(float *fStart, float *fEnd, float *fFogZ) {
}

void CShaderAPIDx11::SceneFogColor3ub(unsigned char r, unsigned char g,
                                      unsigned char b) {}

void CShaderAPIDx11::SceneFogMode(MaterialFogMode_t fogMode) {}

void CShaderAPIDx11::GetSceneFogColor(unsigned char *rgb) {}

MaterialFogMode_t CShaderAPIDx11::GetSceneFogMode() {
  return MATERIAL_FOG_NONE;
}

int CShaderAPIDx11::GetPixelFogCombo() {
  return 0;  // FIXME
}

int CShaderAPIDx11::GetPixelFogCombo1(bool bRadial) {
  return 0;  // FIXME
}

void CShaderAPIDx11::FogColor3f(float r, float g, float b) {}

void CShaderAPIDx11::FogColor3fv(float const *rgb) {}

void CShaderAPIDx11::FogColor3ub(unsigned char r, unsigned char g,
                                 unsigned char b) {}

void CShaderAPIDx11::FogColor3ubv(unsigned char const *rgb) {}

void CShaderAPIDx11::Viewport(int x, int y, int width, int height) {}

void CShaderAPIDx11::GetViewport(int &x, int &y, int &width,
                                 int &height) const {}

// Sets the vertex and pixel shaders
void CShaderAPIDx11::SetVertexShaderIndex(int vshIndex) {}

void CShaderAPIDx11::SetPixelShaderIndex(int pshIndex) {}

// Sets the constant register for vertex and pixel shaders
void CShaderAPIDx11::SetVertexShaderConstant(int var, float const *pVec,
                                             int numConst, bool bForce) {}

void CShaderAPIDx11::SetPixelShaderConstant(int var, float const *pVec,
                                            int numConst, bool bForce) {}

void CShaderAPIDx11::InvalidateDelayedShaderConstants(void) {}

// Set's the linear->gamma conversion textures to use for this hardware for both
// srgb writes enabled and disabled(identity)
void CShaderAPIDx11::SetLinearToGammaConversionTextures(
    ShaderAPITextureHandle_t hSRGBWriteEnabledTexture,
    ShaderAPITextureHandle_t hIdentityTexture) {}

// Returns the nearest supported format
ImageFormat CShaderAPIDx11::GetNearestSupportedFormat(
    ImageFormat fmt, bool bFilteringRequired /* = true */) const {
  return fmt;
}

ImageFormat CShaderAPIDx11::GetNearestRenderTargetFormat(
    ImageFormat fmt) const {
  return fmt;
}

// Sets the texture state
void CShaderAPIDx11::BindTexture(Sampler_t stage,
                                 ShaderAPITextureHandle_t textureHandle) {}

// Indicates we're going to be modifying this texture
// TexImage2D, TexSubImage2D, TexWrap, TexMinFilter, and TexMagFilter
// all use the texture specified by this function.
void CShaderAPIDx11::ModifyTexture(ShaderAPITextureHandle_t textureHandle) {}

// Texture management methods
void CShaderAPIDx11::TexImage2D(int level, int cubeFace, ImageFormat dstFormat,
                                int zOffset, int width, int height,
                                ImageFormat srcFormat, bool bSrcIsTiled,
                                void *imageData) {}

void CShaderAPIDx11::TexSubImage2D(int level, int cubeFace, int xOffset,
                                   int yOffset, int zOffset, int width,
                                   int height, ImageFormat srcFormat,
                                   int srcStride, bool bSrcIsTiled,
                                   void *imageData) {}

void CShaderAPIDx11::TexImageFromVTF(IVTFTexture *pVTF, int iVTFFrame) {}

bool CShaderAPIDx11::TexLock(int level, int cubeFaceID, int xOffset,
                             int yOffset, int width, int height,
                             CPixelWriter &writer) {
  return false;
}

void CShaderAPIDx11::TexUnlock() {}

// These are bound to the texture, not the texture environment
void CShaderAPIDx11::TexMinFilter(ShaderTexFilterMode_t texFilterMode) {}

void CShaderAPIDx11::TexMagFilter(ShaderTexFilterMode_t texFilterMode) {}

void CShaderAPIDx11::TexWrap(ShaderTexCoordComponent_t coord,
                             ShaderTexWrapMode_t wrapMode) {}

void CShaderAPIDx11::TexSetPriority(int priority) {}

ShaderAPITextureHandle_t CShaderAPIDx11::CreateTexture(
    int width, int height, int depth, ImageFormat dstImageFormat,
    int numMipLevels, int numCopies, int flags, const char *pDebugName,
    const char *pTextureGroupName) {
  ShaderAPITextureHandle_t handle;
  CreateTextures(&handle, 1, width, height, depth, dstImageFormat, numMipLevels,
                 numCopies, flags, pDebugName, pTextureGroupName);
  return handle;
}

void CShaderAPIDx11::CreateTextures(ShaderAPITextureHandle_t *pHandles,
                                    int count, int width, int height, int depth,
                                    ImageFormat dstImageFormat,
                                    int numMipLevels, int numCopies, int flags,
                                    const char *pDebugName,
                                    const char *pTextureGroupName) {
  for (int k = 0; k < count; ++k) {
    pHandles[k] = 0;
  }
}

ShaderAPITextureHandle_t CShaderAPIDx11::CreateDepthTexture(
    ImageFormat renderFormat, int width, int height, const char *pDebugName,
    bool bTexture) {
  return 0;
}

void CShaderAPIDx11::DeleteTexture(ShaderAPITextureHandle_t textureHandle) {}

bool CShaderAPIDx11::IsTexture(ShaderAPITextureHandle_t textureHandle) {
  return true;
}

bool CShaderAPIDx11::IsTextureResident(ShaderAPITextureHandle_t textureHandle) {
  return false;
}

// stuff that isn't to be used from within a shader
void CShaderAPIDx11::ClearBuffersObeyStencil(bool bClearColor,
                                             bool bClearDepth) {}

void CShaderAPIDx11::ClearBuffersObeyStencilEx(bool bClearColor,
                                               bool bClearAlpha,
                                               bool bClearDepth) {}

void CShaderAPIDx11::PerformFullScreenStencilOperation(void) {}

void CShaderAPIDx11::ReadPixels(int x, int y, int width, int height,
                                unsigned char *data, ImageFormat dstFormat) {}

void CShaderAPIDx11::ReadPixels(Rect_t *pSrcRect, Rect_t *pDstRect,
                                unsigned char *data, ImageFormat dstFormat,
                                int nDstStride) {}

void CShaderAPIDx11::FlushHardware() {}

// Set the number of bone weights
void CShaderAPIDx11::SetNumBoneWeights(int numBones) {}

// Selection mode methods
int CShaderAPIDx11::SelectionMode(bool selectionMode) { return 0; }

void CShaderAPIDx11::SelectionBuffer(unsigned int *pBuffer, int size) {}

void CShaderAPIDx11::ClearSelectionNames() {}

void CShaderAPIDx11::LoadSelectionName(int name) {}

void CShaderAPIDx11::PushSelectionName(int name) {}

void CShaderAPIDx11::PopSelectionName() {}

// Use this to get the mesh builder that allows us to modify vertex data
CMeshBuilder *CShaderAPIDx11::GetVertexModifyBuilder() { return 0; }

// Board-independent calls, here to unify how shaders set state
// Implementations should chain back to IShaderUtil->BindTexture(), etc.

// Use this to begin and end the frame
void CShaderAPIDx11::BeginFrame() {}

void CShaderAPIDx11::EndFrame() {}

// returns the current time in seconds....
double CShaderAPIDx11::CurrentTime() const { return Sys_FloatTime(); }

// Get the current camera position in world space.
void CShaderAPIDx11::GetWorldSpaceCameraPosition(float *pPos) const {}

void CShaderAPIDx11::ForceHardwareSync(void) {}

void CShaderAPIDx11::SetClipPlane(int index, const float *pPlane) {}

void CShaderAPIDx11::EnableClipPlane(int index, bool bEnable) {}

void CShaderAPIDx11::SetFastClipPlane(const float *pPlane) {}

void CShaderAPIDx11::EnableFastClip(bool bEnable) {}

int CShaderAPIDx11::GetCurrentNumBones(void) const { return 0; }

// Is hardware morphing enabled?
bool CShaderAPIDx11::IsHWMorphingEnabled() const { return false; }

int CShaderAPIDx11::GetCurrentLightCombo(void) const { return 0; }

int CShaderAPIDx11::MapLightComboToPSLightCombo(int nLightCombo) const {
  return 0;
}

MaterialFogMode_t CShaderAPIDx11::GetCurrentFogType(void) const {
  return MATERIAL_FOG_NONE;
}

void CShaderAPIDx11::RecordString(const char *pStr) {}

void CShaderAPIDx11::DestroyVertexBuffers(bool bExitingLevel) {}

int CShaderAPIDx11::GetCurrentDynamicVBSize(void) { return 0; }

void CShaderAPIDx11::EvictManagedResources() {}

void CShaderAPIDx11::ReleaseShaderObjects() {}

void CShaderAPIDx11::RestoreShaderObjects() {}

void CShaderAPIDx11::SetTextureTransformDimension(TextureStage_t textureStage,
                                                  int dimension,
                                                  bool projected) {}

void CShaderAPIDx11::SetBumpEnvMatrix(TextureStage_t textureStage, float m00,
                                      float m01, float m10, float m11) {}

void CShaderAPIDx11::SyncToken(const char *pToken) {}

}  // namespace se::shaderapidx11
