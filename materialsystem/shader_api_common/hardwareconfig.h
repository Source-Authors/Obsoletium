//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef HARDWARECONFIG_H
#define HARDWARECONFIG_H

#ifdef _WIN32
#pragma once
#endif


#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "IHardwareConfigInternal.h"
#include "bitmap/imageformat.h"
#include "materialsystem/imaterialsystem.h"


struct ShaderDeviceInfo_t;


//-----------------------------------------------------------------------------
// ShaderAPI constants
//-----------------------------------------------------------------------------
enum
{
#if defined( DX_TO_GL_ABSTRACTION )
	MAXUSERCLIPPLANES = 2,
#else
	MAXUSERCLIPPLANES = 6,
#endif	
	MAX_NUM_LIGHTS = 4,
	MAX_OUTPUTS = 3,
};


//-----------------------------------------------------------------------------
// Hardware caps structures
//-----------------------------------------------------------------------------
enum CompressedTextureState_t
{
	COMPRESSED_TEXTURES_ON,
	COMPRESSED_TEXTURES_OFF,
	COMPRESSED_TEXTURES_NOT_INITIALIZED
};

struct HardwareCaps_t : public MaterialAdapterInfo_t
{
	// *****************************NOTE*********************************************
	// If you change any members, make sure and reflect the changes in CHardwareConfig::ForceCapsToDXLevel for every dxlevel!!!!!
	// If you change any members, make sure and reflect the changes in CHardwareConfig::ForceCapsToDXLevel for every dxlevel!!!!!
	// If you change any members, make sure and reflect the changes in CHardwareConfig::ForceCapsToDXLevel for every dxlevel!!!!!
	// If you change any members, make sure and reflect the changes in CHardwareConfig::ForceCapsToDXLevel for every dxlevel!!!!!
	// If you change any members, make sure and reflect the changes in CHardwareConfig::ForceCapsToDXLevel for every dxlevel!!!!!
	// *****************************NOTE*********************************************
	// 
	// NOTE: Texture stages are an obsolete concept; used by fixed-function hardware
	// Samplers are dx9+, indicating how many textures we can simultaneously bind
	// In Dx8, samplers didn't exist and texture stages were used to indicate the
	// number of simultaneously bound textures; we'll emulate that by slamming
	// the number of samplers to == the number of texture stages.
	CompressedTextureState_t m_SupportsCompressedTextures;
	VertexCompressionType_t m_SupportsCompressedVertices;
	int  m_NumSamplers;
	int  m_NumTextureStages;
	int	 m_nMaxAnisotropy;
	int  m_MaxTextureWidth;
	int  m_MaxTextureHeight;
	int  m_MaxTextureDepth;
	int	 m_MaxTextureAspectRatio;
	unsigned  m_MaxPrimitiveCount;
	int  m_NumPixelShaderConstants;
	int  m_NumBooleanPixelShaderConstants;
	int  m_NumIntegerPixelShaderConstants;
	int  m_NumVertexShaderConstants;
	int  m_NumBooleanVertexShaderConstants;
	int  m_NumIntegerVertexShaderConstants;
	size_t  m_TextureMemorySize;
	int  m_MaxNumLights;
	int  m_MaxBlendMatrices;
	int  m_MaxBlendMatrixIndices;
	int	 m_MaxVertexShaderBlendMatrices;
	int  m_MaxUserClipPlanes;
	HDRType_t m_HDRType;
	char m_pShaderDLL[32];
	ImageFormat m_ShadowDepthTextureFormat;
	ImageFormat m_NullTextureFormat;
	int  m_nVertexTextureCount;
	int  m_nMaxVertexTextureDimension;
	unsigned long m_AlphaToCoverageState;					// State to ping to toggle Alpha To Coverage              (vendor-dependent)
	unsigned long m_AlphaToCoverageEnableValue;				// Value to set above state to turn on Alpha To Coverage  (vendor-dependent)
	unsigned long m_AlphaToCoverageDisableValue;			// Value to set above state to turn off Alpha To Coverage (vendor-dependent)
	int	 m_nMaxViewports;
	float m_flMinGammaControlPoint;
	float m_flMaxGammaControlPoint;
	int m_nGammaControlPointCount;
	int m_MaxVertexShader30InstructionSlots;
	int m_MaxPixelShader30InstructionSlots;
	int m_MaxSimultaneousRenderTargets;

	bool m_bDeviceOk : 1;
	bool m_HasSetDeviceGammaRamp : 1;				// Indicates device support gamma ramps in the swap chain's current presentation mode (full-screen or windowed).
	bool m_SupportsVertexShaders : 1;				// Vertex shaders support.
	bool m_SupportsVertexShaders_2_0 : 1;			// Vertex shaders 2.0-2.x.
	bool m_SupportsPixelShaders : 1;				// Pixel shaders support.
	bool m_SupportsPixelShaders_1_4 : 1;			// Pixel shaders 1.1-1.4.
	bool m_SupportsPixelShaders_2_0 : 1;			// Pixel shaders 2.0.
	bool m_SupportsPixelShaders_2_b : 1;			// Pixel shaders 2.b.
	bool m_SupportsShaderModel_3_0 : 1;				// Shader model 3.0.
	bool m_SupportsShaderModel_4_0 : 1;				// Shader model 4.0.
	bool m_SupportsShaderModel_5_0 : 1;				// Shader model 5.0.
	bool m_SupportsShaderModel_5_1 : 1;				// Shader model 5.1.
	bool m_SupportsShaderModel_6_0 : 1;				// Shader model 6.0.
	bool m_SupportsHullShaders : 1;					// Hull shaders support.
	bool m_SupportsDomainShaders : 1;				// Domain shaders support.
	bool m_SupportsComputeShaders : 1;				// Compute shaders support.
	bool m_bSupportsAnisotropicFiltering : 1;		// Indicates device supports per-stage anisotropic filtering for minifying textures.
	bool m_bSupportsMagAnisotropicFiltering : 1;	// Indicates device supports per-stage anisotropic filtering for magnifying textures.
	bool m_bSupportsVertexTextures : 1;				// Indicates device supports vertex textures.
	bool m_ZBiasAndSlopeScaledDepthBiasSupported : 1;
	bool m_SupportsMipmapping : 1;					// Indicates device supports mipmaps.
	bool m_SupportsOverbright : 1;
	bool m_SupportsCubeMaps : 1;					// Indicates device supports cube textures.
	bool m_SupportsHardwareLighting : 1;			// Indicates device supports transformation and lighting in hardware.
	bool m_SupportsMipmappedCubemaps : 1;			// Indicates device supports mipmapped cube textures.
	bool m_SupportsNonPow2Textures : 1;				// Indicates non-power of 2 texture dimensions support.
	bool m_PreferDynamicTextures : 1;				// Indicates device supports dynamic textures.
	bool m_HasProjectedBumpEnv : 1;
	bool m_SupportsSRGB : 1;						// Indicates texture supports gamma correction during a read/write operation.
	bool m_bSupportsSpheremapping : 1;				// Indicates support of texture coordinates for sphere mapping.
	bool m_UseFastClipping : 1;
	bool m_bNeedsATICentroidHack : 1;
	bool m_bDisableShaderOptimizations : 1;
	bool m_bColorOnSecondStream : 1;				// Indicates device supports multiple concurrent data streams.
	bool m_bSupportsStreamOffset : 1;				// Indicates device supports stream offsets.
	bool m_bFogColorSpecifiedInLinearSpace : 1;
	bool m_bFogColorAlwaysLinearSpace : 1;
	bool m_bSupportsAlphaToCoverage : 1;			// Indicates alpha to coverage support. Alpha to coverage is a multisampling technique used in graphics rendering that allows for order-independent transparency, particularly useful for rendering complex scenes like foliage in video games.
	bool m_bSupportsShadowDepthTextures : 1;		// Indicates shadow depth formats support.
	bool m_bSupportsFetch4 : 1;						// ATI has a feature called Fetch4 that is able to fetch texels in a single call from a 1-component texture. Analog of Gather4 from sm5: https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/gather4--sm5---asm-
	bool m_bSoftwareVertexProcessing : 1;			// Vertex processing can be done in software.
	bool m_bScissorSupported : 1;					// Indicates scissor test support. See https://learn.microsoft.com/en-us/windows/win32/direct3d9/scissor-test
	bool m_bSupportsFloat32RenderTargets : 1;		// Indicated f32 formats support.
	bool m_bSupportsBorderColor : 1;				// Indicates texture border color support.
	bool m_bDX10Card : 1;							// Indicates DX10 part with performant vertex textures running DX9 path
	bool m_bDX10Blending : 1;						// Indicates DX10 part that does DX10 blending (but may not have performant vertex textures, such as Intel parts)
	bool m_bSupportsStaticControlFlow : 1;			// Useful on OpenGL, where we have a mix of support...
	bool m_FakeSRGBWrite : 1;						// Gotta do this on OpenGL.  Mostly hidden, but some high level code needs to know
	bool m_CanDoSRGBReadFromRTs : 1;				// Gotta do this on OpenGL.  Mostly hidden, but some high level code needs to know
	bool m_bSupportsGLMixedSizeTargets : 1;			// on OpenGL, are mixed size depth buffers supported - aka ARB_framebuffer_object
	bool m_bCanStretchRectFromTextures : 1;			// Does the device expose D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES (or is it >DX9?)

	bool m_bPresentAllowTearing : 1;				// The display supports tearing, a requirement of variable refresh rate displays.

	HDRType_t m_MaxHDRType;
};


//-----------------------------------------------------------------------------
// Contains the hardware configuration for the current device
//-----------------------------------------------------------------------------
class CHardwareConfig : public IHardwareConfigInternal
{
public:
	CHardwareConfig();

	// Sets up the hardware caps given the specified DX level
	void SetupHardwareCaps( const ShaderDeviceInfo_t& mode, const HardwareCaps_t &actualCaps );

	// FIXME: This is for backward compat only.. don't use these
	void SetupHardwareCaps( int nDXLevel, const HardwareCaps_t &actualCaps );
	HardwareCaps_t& ActualCapsForEdit() { return m_ActualCaps; }
	HardwareCaps_t& CapsForEdit() { return m_Caps; }

	// Members of IMaterialSystemHardwareConfig
	virtual bool HasDestAlphaBuffer() const;
	virtual bool HasStencilBuffer() const;
	virtual int	 GetFrameBufferColorDepth() const;
	virtual int  GetSamplerCount() const;
	virtual bool HasSetDeviceGammaRamp() const;
	virtual bool SupportsCompressedTextures() const;
	virtual VertexCompressionType_t SupportsCompressedVertices() const;
	virtual bool SupportsBorderColor() const;
	virtual bool SupportsFetch4() const;
	virtual bool CanStretchRectFromTextures() const;
	virtual bool SupportsVertexAndPixelShaders() const;
	virtual bool SupportsPixelShaders_1_4() const;
	virtual bool SupportsPixelShaders_2_0() const;
	virtual bool SupportsStaticControlFlow() const;
	virtual bool SupportsVertexShaders_2_0() const;
	virtual int  MaximumAnisotropicLevel() const;
	virtual int  MaxTextureWidth() const;
	virtual int  MaxTextureHeight() const;
	virtual size_t	 TextureMemorySize() const;
	virtual bool SupportsOverbright() const;
	virtual bool SupportsCubeMaps() const;
	virtual bool SupportsMipmappedCubemaps() const;
	virtual bool SupportsNonPow2Textures() const;
	virtual int  GetTextureStageCount() const;
	virtual int	 NumVertexShaderConstants() const;
	virtual int	 NumBooleanVertexShaderConstants() const;
	virtual int	 NumIntegerVertexShaderConstants() const;
	virtual int	 NumPixelShaderConstants() const;
	virtual int	 NumBooleanPixelShaderConstants() const;
	virtual int	 NumIntegerPixelShaderConstants() const;
	virtual int	 MaxNumLights() const;
	virtual bool SupportsHardwareLighting() const;
	virtual int	 MaxBlendMatrices() const;
	virtual int	 MaxBlendMatrixIndices() const;
	virtual int  MaxTextureAspectRatio() const;
	virtual int	 MaxVertexShaderBlendMatrices() const;
	virtual int	 MaxUserClipPlanes() const;
	virtual bool UseFastClipping() const;
	virtual int  GetDXSupportLevel() const;
	virtual const char *GetShaderDLLName() const;
	virtual bool ReadPixelsFromFrontBuffer() const;
	virtual bool PreferDynamicTextures() const;
	virtual bool SupportsHDR() const;
	virtual bool HasProjectedBumpEnv() const;
	virtual bool SupportsSpheremapping() const;
	virtual bool NeedsAAClamp() const;
	virtual bool NeedsATICentroidHack() const;
	virtual bool SupportsColorOnSecondStream() const;
	virtual bool SupportsStaticPlusDynamicLighting() const;
	virtual bool PreferReducedFillrate() const;
	virtual int	 GetMaxDXSupportLevel() const;
	virtual bool SpecifiesFogColorInLinearSpace() const;
	virtual bool SupportsSRGB() const;
	virtual bool FakeSRGBWrite() const;
	virtual bool CanDoSRGBReadFromRTs() const;
	virtual bool SupportsGLMixedSizeTargets() const;
	virtual bool IsAAEnabled() const;
	virtual int  GetVertexTextureCount() const;
	virtual int  GetMaxVertexTextureDimension() const;
	virtual int  MaxTextureDepth() const;
	virtual HDRType_t GetHDRType() const;
	virtual HDRType_t GetHardwareHDRType() const;
	virtual bool SupportsPixelShaders_2_b() const;
	virtual bool SupportsShaderModel_3_0() const;
	virtual bool SupportsStreamOffset() const;
	virtual int  StencilBufferBits() const;
	virtual int  MaxViewports() const;
	virtual void OverrideStreamOffsetSupport( bool bOverrideEnabled, bool bEnableSupport );
	virtual int  GetShadowFilterMode() const;
	virtual int NeedsShaderSRGBConversion() const;
	virtual bool UsesSRGBCorrectBlending() const;
	virtual bool HasFastVertexTextures() const;
	virtual int MaxHWMorphBatchCount() const;

	const char *GetHWSpecificShaderDLLName() const;
	int GetActualSamplerCount() const;
	int GetActualTextureStageCount() const;
	bool SupportsMipmapping() const;
	bool ActuallySupportsPixelShaders_2_b() const override;

	virtual bool SupportsHDRMode( HDRType_t nHDRMode ) const;

	const HardwareCaps_t& ActualCaps() const { return m_ActualCaps; }
	const HardwareCaps_t& Caps() const { return m_Caps; }
	virtual bool GetHDREnabled( void ) const;
	virtual void SetHDREnabled( bool bEnable );

protected:
	// Gets the recommended configuration associated with a particular dx level
	void ForceCapsToDXLevel( HardwareCaps_t *pCaps, int nDxLevel, const HardwareCaps_t &actualCaps );

	// Members related to capabilities
	HardwareCaps_t m_ActualCaps;
	HardwareCaps_t m_Caps;
	HardwareCaps_t m_UnOverriddenCaps;
	bool m_bHDREnabled;
};


//-----------------------------------------------------------------------------
// Singleton hardware config
//-----------------------------------------------------------------------------
extern CHardwareConfig *g_pHardwareConfig;


#endif // HARDWARECONFIG_H
