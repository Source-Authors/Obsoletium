// Copyright Valve Corporation, All rights reserved.

#ifndef SRC_MATERIALSYSTEM_SHADERAPIDX11_SHADER_SHADOW_DX11_H_
#define SRC_MATERIALSYSTEM_SHADERAPIDX11_SHADER_SHADOW_DX11_H_

#include "shaderapi/ishaderapi.h"
#include "shaderapi/ishadershadow.h"

namespace se::shaderapidx11 {

// Shader shadow on DirectX 11 hardware.
class CShaderShadowDx11 final : public IShaderShadow {
 public:
  CShaderShadowDx11();
  ~CShaderShadowDx11();

  // Sets the default *shadow* state
  void SetDefaultState();

  // Methods related to depth buffering
  void DepthFunc(ShaderDepthFunc_t depthFunc);
  void EnableDepthWrites(bool bEnable);
  void EnableDepthTest(bool bEnable);
  void EnablePolyOffset(PolygonOffsetMode_t nOffsetMode);

  // Suppresses/activates color writing
  void EnableColorWrites(bool bEnable);
  void EnableAlphaWrites(bool bEnable);

  // Methods related to alpha blending
  void EnableBlending(bool bEnable);
  void BlendFunc(ShaderBlendFactor_t srcFactor, ShaderBlendFactor_t dstFactor);

  // Alpha testing
  void EnableAlphaTest(bool bEnable);
  void AlphaFunc(ShaderAlphaFunc_t alphaFunc, float alphaRef /* [0-1] */);

  // Wireframe/filled polygons
  void PolyMode(ShaderPolyModeFace_t face, ShaderPolyMode_t polyMode);

  // Back face culling
  void EnableCulling(bool bEnable);

  // constant color + transparency
  void EnableConstantColor(bool bEnable);

  // Indicates the vertex format for use with a vertex shader
  // The flags to pass in here come from the VertexFormatFlags_t enum
  // If pTexCoordDimensions is *not* specified, we assume all coordinates
  // are 2-dimensional
  void VertexShaderVertexFormat(unsigned int flags, int numTexCoords,
                                int* pTexCoordDimensions, int userDataSize);

  // Indicates we're going to light the model
  void EnableLighting(bool bEnable);
  void EnableSpecular(bool bEnable);

  // vertex blending
  void EnableVertexBlend(bool bEnable);

  // per texture unit stuff
  void OverbrightValue(TextureStage_t stage, float value);
  void EnableTexture(Sampler_t stage, bool bEnable);
  void EnableTexGen(TextureStage_t stage, bool bEnable);
  void TexGen(TextureStage_t stage, ShaderTexGenParam_t param);

  // alternate method of specifying per-texture unit stuff, more flexible and
  // more complicated Can be used to specify different operation per channel
  // (alpha/color)...
  void EnableCustomPixelPipe(bool bEnable);
  void CustomTextureStages(int stageCount);
  void CustomTextureOperation(TextureStage_t stage, ShaderTexChannel_t channel,
                              ShaderTexOp_t op, ShaderTexArg_t arg1,
                              ShaderTexArg_t arg2);

  // indicates what per-vertex data we're providing
  void DrawFlags(unsigned int drawFlags);

  // A simpler method of dealing with alpha modulation
  void EnableAlphaPipe(bool bEnable);
  void EnableConstantAlpha(bool bEnable);
  void EnableVertexAlpha(bool bEnable);
  void EnableTextureAlpha(TextureStage_t stage, bool bEnable);

  // GR - Separate alpha blending
  void EnableBlendingSeparateAlpha(bool bEnable);
  void BlendFuncSeparateAlpha(ShaderBlendFactor_t srcFactor,
                              ShaderBlendFactor_t dstFactor);

  // Sets the vertex and pixel shaders
  void SetVertexShader(const char* pFileName, int vshIndex);
  void SetPixelShader(const char* pFileName, int pshIndex);

  // Convert from linear to gamma color space on writes to frame buffer.
  void EnableSRGBWrite(bool) {}

  void EnableSRGBRead(Sampler_t, bool) {}

  virtual void FogMode(ShaderFogMode_t) {}
  virtual void SetDiffuseMaterialSource(ShaderMaterialSource_t) {}

  virtual void SetMorphFormat(MorphFormat_t) {}

  virtual void EnableStencil(bool) {}
  virtual void StencilFunc(ShaderStencilFunc_t) {}
  virtual void StencilPassOp(ShaderStencilOp_t) {}
  virtual void StencilFailOp(ShaderStencilOp_t) {}
  virtual void StencilDepthFailOp(ShaderStencilOp_t) {}
  virtual void StencilReference(int) {}
  virtual void StencilMask(int) {}
  virtual void StencilWriteMask(int) {}

  virtual void DisableFogGammaCorrection(bool) {
    // FIXME: empty for now.
  }
  virtual void FogMode(ShaderFogMode_t, bool) {
    // FIXME: empty for now.
  }

  // Alpha to coverage
  void EnableAlphaToCoverage(bool bEnable);

  void SetShadowDepthFiltering(Sampler_t stage);

  // More alpha blending state
  void BlendOp(ShaderBlendOp_t blendOp);
  void BlendOpSeparateAlpha(ShaderBlendOp_t blendOp);

  bool m_IsTranslucent;
  bool m_IsAlphaTested;
  bool m_bIsDepthWriteEnabled;
  bool m_bUsesVertexAndPixelShaders;
};

extern CShaderShadowDx11* g_pShaderShadowDx11;

}  // namespace se::shaderapidx11

#endif  // !SRC_MATERIALSYSTEM_SHADERAPIDX11_SHADER_SHADOW_DX11_H_
