//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef SHADERAPIDX10_H
#define SHADERAPIDX10_H

#ifdef _WIN32
#pragma once
#endif

#include <d3d10.h>

#include "shaderapibase.h"
#include "materialsystem/idebugtextureinfo.h"
#include "meshdx10.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct MaterialSystemHardwareIdentifier_t;


//-----------------------------------------------------------------------------
// DX10 enumerations that don't appear to exist
//-----------------------------------------------------------------------------
#define MAX_DX10_VIEWPORTS  16
#define MAX_DX10_STREAMS	16


//-----------------------------------------------------------------------------
// A record describing the state on the board
//-----------------------------------------------------------------------------
struct ShaderIndexBufferStateDx10_t
{
	ID3D10Buffer *m_pBuffer;
	DXGI_FORMAT m_Format;
	UINT m_nOffset;

	bool operator!=( const ShaderIndexBufferStateDx10_t& src ) const
	{
		return memcmp( this, &src, sizeof(ShaderIndexBufferStateDx10_t) ) != 0;
	}
};

struct ShaderVertexBufferStateDx10_t
{
	ID3D10Buffer *m_pBuffer;
	UINT m_nStride;
	UINT m_nOffset;
};

struct ShaderInputLayoutStateDx10_t
{
	VertexShaderHandle_t m_hVertexShader;
	VertexFormat_t m_pVertexDecl[ MAX_DX10_STREAMS ];
};

struct ShaderStateDx10_t
{
	int m_nViewportCount;
	D3D10_VIEWPORT m_pViewports[ MAX_DX10_VIEWPORTS ];
	FLOAT m_ClearColor[4];
	ShaderRasterState_t m_RasterState;
	ID3D10RasterizerState *m_pRasterState;
	ID3D10VertexShader *m_pVertexShader;
	ID3D10GeometryShader *m_pGeometryShader;
	ID3D10PixelShader *m_pPixelShader;
	ShaderVertexBufferStateDx10_t m_pVertexBuffer[ MAX_DX10_STREAMS ];
	ShaderIndexBufferStateDx10_t m_IndexBuffer;
	ShaderInputLayoutStateDx10_t m_InputLayout;
	D3D10_PRIMITIVE_TOPOLOGY m_Topology;
};


//-----------------------------------------------------------------------------
// Commit function helper class
//-----------------------------------------------------------------------------
typedef void (*StateCommitFunc_t)( ID3D10Device *pDevice, const ShaderStateDx10_t &desiredState, ShaderStateDx10_t &currentState, bool bForce );

class CFunctionCommit
{
public:
	CFunctionCommit();
	~CFunctionCommit();

	void Init( int nFunctionCount );

	// Methods related to queuing functions to be called per-(pMesh->Draw call) or per-pass
	void ClearAllCommitFuncs( );
	void CallCommitFuncs( bool bForce );
	bool IsCommitFuncInUse( int nFunc ) const;
	void MarkCommitFuncInUse( int nFunc );
	void AddCommitFunc( StateCommitFunc_t f );
	void CallCommitFuncs( ID3D10Device *pDevice, const ShaderStateDx10_t &desiredState, ShaderStateDx10_t &currentState, bool bForce = false );

private:
	// A list of state commit functions to run as per-draw call commit time
	unsigned char* m_pCommitFlags;
	int m_nCommitBufferSize;
	CUtlVector< StateCommitFunc_t >	m_CommitFuncs;
};


//-----------------------------------------------------------------------------
// The Dx10 implementation of the shader API
//-----------------------------------------------------------------------------
class CShaderAPIDx10 : public CShaderAPIBase, public IDebugTextureInfo
{
	typedef CShaderAPIBase BaseClass;

public:
	// constructor, destructor
	CShaderAPIDx10( );
	virtual ~CShaderAPIDx10();

	// Methods of IShaderAPI
	// NOTE: These methods have been ported over
public:
	void SetViewports( int nCount, const ShaderViewport_t* pViewports ) override;
	int GetViewports( ShaderViewport_t* pViewports, int nMax ) const override;
	void ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil, int renderTargetWidth, int renderTargetHeight ) override;
	void ClearColor3ub( unsigned char r, unsigned char g, unsigned char b ) override;
	void ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a ) override;
	void SetRasterState( const ShaderRasterState_t& state ) override;
	void BindVertexShader( VertexShaderHandle_t hVertexShader ) override;
	void BindGeometryShader( GeometryShaderHandle_t hGeometryShader ) override;
	void BindPixelShader( PixelShaderHandle_t hPixelShader ) override;
	void BindVertexBuffer( int nStreamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions = 1 ) override;
	void BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes ) override;
	void Draw( MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount ) override;

	// Methods of IShaderDynamicAPI
public:
	void GetBackBufferDimensions( int&, int& ) const override;

public:
	// Methods of CShaderAPIBase
	bool OnDeviceInit() override { ResetRenderState(); return true; }
	void OnDeviceShutdown() override {}
	void ReleaseShaderObjects() override;
	void RestoreShaderObjects() override;
	void BeginPIXEvent( unsigned long, const char * ) override {}
	void EndPIXEvent() override{}
	void AdvancePIXFrame() override {}

	// NOTE: These methods have not been ported over.
	// IDebugTextureInfo implementation.
public:

	bool IsDebugTextureListFresh( int numFramesAllowed = 1 ) override { return false; }
	void EnableDebugTextureList( bool ) override {}
	void EnableGetAllTextures( bool ) override {}
	KeyValues* GetDebugTextureList() override { return NULL; }
	int GetTextureMemoryUsed( TextureMemoryType ) override { return 0; }
	bool SetDebugTextureRendering( bool ) override { return false; }
	
public:
	// Other public methods
	void Unbind( VertexShaderHandle_t hShader );
	void Unbind( GeometryShaderHandle_t hShader );
	void Unbind( PixelShaderHandle_t hShader );
	void UnbindVertexBuffer( ID3D10Buffer *pBuffer );
	void UnbindIndexBuffer( ID3D10Buffer *pBuffer );

	void PrintfVA( char *fmt, va_list vargs ) override {}
	void Printf( PRINTF_FORMAT_STRING const char *fmt, ... ) override {}
	float Knob( char *knobname, float *setvalue = NULL ) override { return 0.0f;}

private:

	// Returns a d3d texture associated with a texture handle
	IDirect3DBaseTexture* GetD3DTexture( ShaderAPITextureHandle_t hTexture ) override { Assert(0); return NULL; }
	void QueueResetRenderState() override {}

	void SetTopology( MaterialPrimitiveType_t topology );

	bool DoRenderTargetsNeedSeparateDepthBuffer() const override;

	void SetHardwareGammaRamp( float )
	{
	}

	// Used to clear the transition table when we know it's become invalid.
	void ClearSnapshots() override;

	// Sets the mode...
	bool SetMode( void* hwnd, unsigned nAdapter, const ShaderDeviceInfo_t &info ) override;

	void ChangeVideoMode(const ShaderDeviceInfo_t &) override
	{
	}

	// Called when the dx support level has changed
	void DXSupportLevelChanged() override {}

	void EnableUserClipTransformOverride( bool ) override {}
	void UserClipTransform( const VMatrix & ) override {}
	virtual bool GetUserClipTransform( VMatrix & ) { return false; }

	// Sets the default *dynamic* state
	void SetDefaultState( ) override;

	// Returns the snapshot id for the shader state
	StateSnapshot_t	 TakeSnapshot( ) override;

	// Returns true if the state snapshot is transparent
	bool IsTranslucent( StateSnapshot_t id ) const override;
	bool IsAlphaTested( StateSnapshot_t id ) const override;
	bool UsesVertexAndPixelShaders( StateSnapshot_t id ) const override;
	bool IsDepthWriteEnabled( StateSnapshot_t id ) const override;

	// Gets the vertex format for a set of snapshot ids
	VertexFormat_t ComputeVertexFormat( int numSnapshots, StateSnapshot_t* pIds ) const override;

	// Gets the vertex format for a set of snapshot ids
	VertexFormat_t ComputeVertexUsage( int numSnapshots, StateSnapshot_t* pIds ) const override;

	// Begins a rendering pass that uses a state snapshot
	void BeginPass( StateSnapshot_t snapshot  ) override;

	// Uses a state snapshot
	void UseSnapshot( StateSnapshot_t snapshot );

	// Use this to get the mesh builder that allows us to modify vertex data
	CMeshBuilder* GetVertexModifyBuilder() override;

	// Sets the color to modulate by
	void Color3f( float r, float g, float b ) override;
	void Color3fv( float const* pColor ) override;
	void Color4f( float r, float g, float b, float a ) override;
	void Color4fv( float const* pColor ) override;

	// Faster versions of color
	void Color3ub( unsigned char r, unsigned char g, unsigned char b ) override;
	void Color3ubv( unsigned char const* rgb ) override;
	void Color4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a ) override;
	void Color4ubv( unsigned char const* rgba ) override;

	// Sets the lights
	void SetLight( int lightNum, const LightDesc_t& desc ) override;
	void SetAmbientLight( float r, float g, float b ) override;
	void SetAmbientLightCube( Vector4D cube[6] ) override;
	void SetLightingOrigin( Vector ) override {}

	// Get the lights
	int GetMaxLights( void ) const override;
	const LightDesc_t& GetLight( int lightNum ) const override;

	// Render state for the ambient light cube (vertex shaders)
	void SetVertexShaderStateAmbientLightCube() override;
	void SetPixelShaderStateAmbientLightCube( int pshReg, bool bForceToBlack = false ) override {}
	void SetPixelShaderStateAmbientLightCube( int pshReg )
	{
	}
	void GetDX9LightState( LightState_t *state ) const override {}

	float GetAmbientLightCubeLuminance(void) override
	{
		return 0.0f;
	}

	void SetSkinningMatrices() override;

	// Lightmap texture binding
	void BindLightmap( TextureStage_t stage );
	void BindLightmapAlpha( TextureStage_t stage )
	{
	}
	void BindBumpLightmap( TextureStage_t stage );
	void BindFullbrightLightmap( TextureStage_t stage );
	void BindWhite( TextureStage_t stage );
	void BindBlack( TextureStage_t stage );
	void BindGrey( TextureStage_t stage );
	void BindFBTexture( TextureStage_t stage, int textureIdex );
	void CopyRenderTargetToTexture( ShaderAPITextureHandle_t texID ) override
	{
	}

	void CopyRenderTargetToTextureEx( ShaderAPITextureHandle_t texID, int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect ) override
	{
	}

	void CopyTextureToRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t textureHandle, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL ) override
	{
	}

	// Special system flat normal map binding.
	void BindFlatNormalMap( TextureStage_t stage );
	void BindNormalizationCubeMap( TextureStage_t stage );
	void BindSignedNormalizationCubeMap( TextureStage_t stage );

	// Set the number of bone weights
	void SetNumBoneWeights( int numBones ) override;

	// Flushes any primitives that are buffered
	void FlushBufferedPrimitives() override;

	// Creates/destroys Mesh
	IMesh* CreateStaticMesh( VertexFormat_t fmt, const char *pTextureBudgetGroup, IMaterial * pMaterial = NULL );
	void DestroyStaticMesh( IMesh* mesh );

	// Gets the dynamic mesh; note that you've got to render the mesh
	// before calling this function a second time. Clients should *not*
	// call DestroyStaticMesh on the mesh returned by this call.
	IMesh* GetDynamicMesh( IMaterial* pMaterial, int nHWSkinBoneCount, bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride ) override;
	IMesh* GetDynamicMeshEx( IMaterial* pMaterial, VertexFormat_t fmt, int nHWSkinBoneCount, bool buffered, IMesh* pVertexOverride, IMesh* pIndexOverride ) override;
	IVertexBuffer *GetDynamicVertexBuffer( IMaterial* pMaterial, bool buffered )
	{
		Assert( 0 );
		return NULL;
	}
	IIndexBuffer *GetDynamicIndexBuffer( IMaterial* pMaterial, bool buffered )
	{
		Assert( 0 );
		return NULL;
	}
	IMesh* GetFlexMesh() override;

	// Renders a single pass of a material
	void RenderPass( int nPass, int nPassCount ) override;

	// stuff related to matrix stacks
	void MatrixMode( MaterialMatrixMode_t matrixMode ) override;
	void PushMatrix() override;
	void PopMatrix() override;
	void LoadMatrix( float *m ) override;
	void LoadBoneMatrix( int boneIndex, const float *m ) override {}
	void MultMatrix( float *m ) override;
	void MultMatrixLocal( float *m ) override;
	void GetMatrix( MaterialMatrixMode_t matrixMode, float *dst ) override;
	void LoadIdentity( void ) override;
	void LoadCameraToWorld( void ) override;
	void Ortho( double left, double top, double right, double bottom, double zNear, double zFar ) override;
	void PerspectiveX( double fovx, double aspect, double zNear, double zFar ) override;
	void PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right ) override;
	void PickMatrix( int x, int y, int width, int height ) override;
	void Rotate( float angle, float x, float y, float z ) override;
	void Translate( float x, float y, float z ) override;
	void Scale( float x, float y, float z ) override;
	void ScaleXY( float x, float y ) override;

	void Viewport( int x, int y, int width, int height );
	void GetViewport( int& x, int& y, int& width, int& height ) const;

	// Fog methods...
	void FogMode( MaterialFogMode_t fogMode );
	void FogStart( float fStart ) override;
	void FogEnd( float fEnd ) override;
	void SetFogZ( float fogZ ) override;
	void FogMaxDensity( float flMaxDensity ) override;
	void GetFogDistances( float *fStart, float *fEnd, float *fFogZ ) override;
	void FogColor3f( float r, float g, float b );
	void FogColor3fv( float const* rgb );
	void FogColor3ub( unsigned char r, unsigned char g, unsigned char b );
	void FogColor3ubv( unsigned char const* rgb );

	void SceneFogColor3ub( unsigned char r, unsigned char g, unsigned char b ) override;
	void SceneFogMode( MaterialFogMode_t fogMode ) override;
	void GetSceneFogColor( unsigned char *rgb ) override;
	MaterialFogMode_t GetSceneFogMode( ) override;
	int GetPixelFogCombo( ) override;

	void SetHeightClipZ( float z ) override; 
	void SetHeightClipMode( enum MaterialHeightClipMode_t heightClipMode ) override; 

	void SetClipPlane( int index, const float *pPlane ) override;
	void EnableClipPlane( int index, bool bEnable ) override;

	void SetFastClipPlane( const float *pPlane ) override;
	void EnableFastClip( bool bEnable ) override;

	// We use smaller dynamic VBs during level transitions, to free up memory
	int  GetCurrentDynamicVBSize( void ) override;
	void DestroyVertexBuffers( bool bExitingLevel = false ) override;

	// Sets the vertex and pixel shaders
	void SetVertexShaderIndex( int vshIndex ) override;
	void SetPixelShaderIndex( int pshIndex ) override;

	// Sets the constant register for vertex and pixel shaders
	void SetVertexShaderConstant( int var, float const* pVec, int numConst = 1, bool bForce = false ) override;
	void SetPixelShaderConstant( int var, float const* pVec, int numConst = 1, bool bForce = false ) override;

	void SetBooleanVertexShaderConstant( int var, BOOL const* pVec, int numBools = 1, bool bForce = false ) override
	{
		Assert(0);
	}


	void SetIntegerVertexShaderConstant( int var, int const* pVec, int numIntVecs = 1, bool bForce = false ) override
	{
		Assert(0);
	}

	
	void SetBooleanPixelShaderConstant( int var, BOOL const* pVec, int numBools = 1, bool bForce = false ) override
	{
		Assert(0);
	}

	void SetIntegerPixelShaderConstant( int var, int const* pVec, int numIntVecs = 1, bool bForce = false ) override
	{
		Assert(0);
	}

	bool ShouldWriteDepthToDestAlpha( void ) const override
	{
		Assert(0);
		return false;
	}


	void InvalidateDelayedShaderConstants( void ) override;

	// Gamma<->Linear conversions according to the video hardware we're running on
	float GammaToLinear_HardwareSpecific( float fGamma ) const override
	{
		return 0.;
	}

	float LinearToGamma_HardwareSpecific( float fLinear ) const override
	{
		return 0.;
	}

	//Set's the linear->gamma conversion textures to use for this hardware for both srgb writes enabled and disabled(identity)
	void SetLinearToGammaConversionTextures( ShaderAPITextureHandle_t hSRGBWriteEnabledTexture, ShaderAPITextureHandle_t hIdentityTexture ) override;

	// Cull mode
	void CullMode( MaterialCullMode_t cullMode ) override;

	// Force writes only when z matches. . . useful for stenciling things out
	// by rendering the desired Z values ahead of time.
	void ForceDepthFuncEquals( bool bEnable ) override;

	// Forces Z buffering on or off
	void OverrideDepthEnable( bool bEnable, bool bDepthEnable ) override;
	// Forces alpha writes on or off
	void OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable ) override;
	//forces color writes on or off
	void OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable ) override;

	// Sets the shade mode
	void ShadeMode( ShaderShadeMode_t mode ) override;

	// Binds a particular material to render with
	void Bind( IMaterial* pMaterial ) override;

	// Returns the nearest supported format
	ImageFormat GetNearestSupportedFormat( ImageFormat fmt, bool bFilteringRequired = true ) const override;
	ImageFormat GetNearestRenderTargetFormat( ImageFormat fmt ) const override;

	// Sets the texture state
	void BindTexture( Sampler_t stage, ShaderAPITextureHandle_t textureHandle ) override;

	void SetRenderTarget( ShaderAPITextureHandle_t colorTextureHandle, ShaderAPITextureHandle_t depthTextureHandle ) override
	{
	}

	void SetRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t colorTextureHandle, ShaderAPITextureHandle_t depthTextureHandle ) override
	{
	}

	// Indicates we're going to be modifying this texture
	// TexImage2D, TexSubImage2D, TexWrap, TexMinFilter, and TexMagFilter
	// all use the texture specified by this function.
	void ModifyTexture( ShaderAPITextureHandle_t textureHandle ) override;

	// Texture management methods
	void TexImage2D( int level, int cubeFace, ImageFormat dstFormat, int zOffset, int width, int height, 
		ImageFormat srcFormat, bool bSrcIsTiled, void *imageData ) override;
	void TexSubImage2D( int level, int cubeFace, int xOffset, int yOffset, int zOffset, int width, int height,
		ImageFormat srcFormat, int srcStride, bool bSrcIsTiled, void *imageData ) override;
	void TexImageFromVTF( IVTFTexture *pVTF, int iVTFFrame ) override;

	bool TexLock( int level, int cubeFaceID, int xOffset, int yOffset, 
		int width, int height, CPixelWriter& writer ) override;
	void TexUnlock( ) override;

	// These are bound to the texture, not the texture environment
	void TexMinFilter( ShaderTexFilterMode_t texFilterMode ) override;
	void TexMagFilter( ShaderTexFilterMode_t texFilterMode ) override;
	void TexWrap( ShaderTexCoordComponent_t coord, ShaderTexWrapMode_t wrapMode ) override;
	void TexSetPriority( int priority ) override;	

	ShaderAPITextureHandle_t CreateTexture( 
		int width, 
		int height,
		int depth,
		ImageFormat dstImageFormat, 
		int numMipLevels, 
		int numCopies, 
		int flags, 
		const char *pDebugName,
		const char *pTextureGroupName ) override;
	void CreateTextures( 
		ShaderAPITextureHandle_t *pHandles,
		int count,
		int width, 
		int height,
		int depth,
		ImageFormat dstImageFormat, 
		int numMipLevels, 
		int numCopies, 
		int flags, 
		const char *pDebugName,
		const char *pTextureGroupName ) override;
	ShaderAPITextureHandle_t CreateDepthTexture( ImageFormat renderFormat, int width, int height, const char *pDebugName, bool bTexture ) override;
	void DeleteTexture( ShaderAPITextureHandle_t textureHandle ) override;
	bool IsTexture( ShaderAPITextureHandle_t textureHandle ) override;
	bool IsTextureResident( ShaderAPITextureHandle_t textureHandle ) override;

	// stuff that isn't to be used from within a shader
	void ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth ) override;
	void ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth ) override;
	void PerformFullScreenStencilOperation( void ) override;
	void ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat ) override;
	void ReadPixels( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *data, ImageFormat dstFormat, int nDstStride ) override;

	// Selection mode methods
	int SelectionMode( bool selectionMode ) override;
	void SelectionBuffer( unsigned int* pBuffer, int size ) override;
	void ClearSelectionNames( ) override;
	void LoadSelectionName( int name ) override;
	void PushSelectionName( int name ) override;
	void PopSelectionName() override;

	void FlushHardware() override;
	void ResetRenderState( bool bFullReset = true ) override;

	// Can we download textures?
	bool CanDownloadTextures() const override;

	// Board-independent calls, here to unify how shaders set state
	// Implementations should chain back to IShaderUtil->BindTexture(), etc.

	// Use this to begin and end the frame
	void BeginFrame() override;
	void EndFrame() override;

	// returns current time
	double CurrentTime() const override;

	// Get the current camera position in world space.
	void GetWorldSpaceCameraPosition( float * pPos ) const override;

	void ForceHardwareSync( void ) override;

	int GetCurrentNumBones( void ) const override;
	bool IsHWMorphingEnabled( ) const override;
	int GetCurrentLightCombo( void ) const override;
	int MapLightComboToPSLightCombo( int nLightCombo ) const;
	MaterialFogMode_t GetCurrentFogType( void ) const override;

	void RecordString( const char *pStr );

	void EvictManagedResources() override;

	void SetTextureTransformDimension( TextureStage_t textureStage, int dimension, bool projected ) override;
	void DisableTextureTransform( TextureStage_t textureStage ) override
	{
	}
	void SetBumpEnvMatrix( TextureStage_t textureStage, float m00, float m01, float m10, float m11 ) override;

	// Gets the lightmap dimensions
	void GetLightmapDimensions( int *w, int *h ) override;

	void SyncToken( const char *pToken ) override;

	// Setup standard vertex shader constants (that don't change)
	// This needs to be called anytime that overbright changes.
	void SetStandardVertexShaderConstants( float fOverbright ) override
	{
	}

	// Scissor Rect
	void SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor ) override {}

	// Reports support for a given CSAA mode
	bool SupportsCSAAMode( int nNumSamples, int nQualityLevel ) override { return false; }

	// Level of anisotropic filtering
	void SetAnisotropicLevel( int nAnisotropyLevel ) override
	{
	}

	void SetDefaultDynamicState()
	{
	}
	void CommitPixelShaderLighting( int pshReg ) override
	{
	}

	void MarkUnusedVertexFields( unsigned int nFlags, int nTexCoordCount, bool *pUnusedTexCoords ) override
	{
	}

	ShaderAPIOcclusionQuery_t CreateOcclusionQueryObject( void ) override
	{
		return INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE;
	}

	void DestroyOcclusionQueryObject( ShaderAPIOcclusionQuery_t handle ) override
	{
	}

	void BeginOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t handle ) override
	{
	}

	void EndOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t handle ) override
	{
	}

	int OcclusionQuery_GetNumPixelsRendered( ShaderAPIOcclusionQuery_t handle, bool bFlush ) override
	{
		return 0;
	}

	void AcquireThreadOwnership() override {}
	void ReleaseThreadOwnership() override {}

	virtual bool SupportsBorderColor() const { return false; }
	virtual bool SupportsFetch4() const { return false; }
	void EnableBuffer2FramesAhead( bool bEnable ) override {}

	void SetDepthFeatheringPixelShaderConstant( int iConstant, float fDepthBlendScale ) override {}

	void SetPixelShaderFogParams( int reg ) override
	{
	}

	bool InFlashlightMode() const override
	{
		return false;
	}

	bool InEditorMode() const override
	{
		return false;
	}

	// What fields in the morph do we actually use?
	MorphFormat_t ComputeMorphFormat( int numSnapshots, StateSnapshot_t* pIds ) const override
	{
		return 0;
	}

	// Gets the bound morph's vertex format; returns 0 if no morph is bound
	MorphFormat_t GetBoundMorphFormat() override
	{
		return 0;
	}

	void GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id ) override;

	// Binds a standard texture
	void BindStandardTexture( Sampler_t stage, StandardTextureId_t id ) override
	{
	}

	void BindStandardVertexTexture( VertexTextureSampler_t stage, StandardTextureId_t id ) override
	{
	}

	void SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture ) override
	{
	}

	void SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture ) override
	{
	}

	const FlashlightState_t &GetFlashlightState( VMatrix &worldToTexture ) const override
	{
		static FlashlightState_t  blah;
		return blah;
	}

	const FlashlightState_t &GetFlashlightStateEx( VMatrix &worldToTexture, ITexture **pFlashlightDepthTexture ) const override
	{
		static FlashlightState_t  blah;
		return blah;
	}

	virtual void SetModeChangeCallback( ModeChangeCallbackFunc_t func )
	{
	}


	void ClearVertexAndPixelShaderRefCounts() override
	{
	}

	void PurgeUnusedVertexAndPixelShaders() override
	{
	}

	// Binds a vertex texture to a particular texture stage in the vertex pipe
	void BindVertexTexture( VertexTextureSampler_t nStage, ShaderAPITextureHandle_t hTexture ) override
	{
	}

	// Sets morph target factors
	void SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights ) override
	{
	}

	// NOTE: Stuff after this is added after shipping HL2.
	ITexture *GetRenderTargetEx( int nRenderTargetID ) override
	{
		return NULL;
	}

	void SetToneMappingScaleLinear( const Vector &scale ) override
	{
	}

	const Vector &GetToneMappingScaleLinear( void ) const override
	{
		static Vector dummy;
		return dummy;
	}

	float GetLightMapScaleFactor( void ) const override
	{
		return 1.0;
	}

	// For dealing with device lost in cases where SwapBuffers isn't called all the time (Hammer)
	void HandleDeviceLost() override
	{
	}

	void EnableLinearColorSpaceFrameBuffer( bool bEnable ) override
	{
	}

	// Lets the shader know about the full-screen texture so it can 
	void SetFullScreenTextureHandle( ShaderAPITextureHandle_t h ) override
	{
	}

	void SetFloatRenderingParameter(int parm_number, float value) override
	{
	}

	void SetIntRenderingParameter(int parm_number, int value) override
	{
	}
	void SetVectorRenderingParameter(int parm_number, Vector const &value) override
	{
	}

	float GetFloatRenderingParameter(int parm_number) const override
	{
		return 0;
	}

	int GetIntRenderingParameter(int parm_number) const override
	{
		return 0;
	}

	Vector GetVectorRenderingParameter(int parm_number) const override
	{
		return Vector(0,0,0);
	}

	// Methods related to stencil
	void SetStencilEnable(bool onoff) override
	{
	}

	void SetStencilFailOperation(StencilOperation_t op) override
	{
	}

	void SetStencilZFailOperation(StencilOperation_t op) override
	{
	}

	void SetStencilPassOperation(StencilOperation_t op) override
	{
	}

	void SetStencilCompareFunction(StencilComparisonFunction_t cmpfn) override
	{
	}

	void SetStencilReferenceValue(int ref) override
	{
	}

	void SetStencilTestMask(uint32 msk) override
	{
	}

	void SetStencilWriteMask(uint32 msk) override
	{
	}

	void ClearStencilBufferRectangle( int xmin, int ymin, int xmax, int ymax,int value) override
	{
	}

	void GetDXLevelDefaults(uint &max_dxlevel, uint &recommended_dxlevel) override
	{
		max_dxlevel=recommended_dxlevel=90;
	}

	void GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices ) override
	{
		*pMaxVerts = 32768;
		*pMaxIndices = 32768;
	}

	// Returns the max possible vertices + indices to render in a single draw call
	int GetMaxVerticesToRender( IMaterial *pMaterial ) override
	{
		return 32768;
	}

	int GetMaxIndicesToRender( ) override
	{
		return 32768;
	}
	int CompareSnapshots( StateSnapshot_t snapshot0, StateSnapshot_t snapshot1 ) override { return 0; }

	void DisableAllLocalLights() override {}

	bool SupportsMSAAMode( int nMSAAMode ) override { return false; }

	// Hooks for firing PIX events from outside the Material System...
	void SetPIXMarker( unsigned long color, const char *szName ) override {}

	void ComputeVertexDescription( unsigned char* pBuffer, VertexFormat_t vertexFormat, MeshDesc_t& desc ) const override {}

	bool SupportsShadowDepthTextures() override { return false; }

	int NeedsShaderSRGBConversion(void) const { return 1; }

	bool SupportsFetch4() override { return false; }

	void SetShadowDepthBiasFactors( float fShadowSlopeScaleDepthBias, float fShadowDepthBias ) override {}

	void SetDisallowAccess( bool ) override {}
	void EnableShaderShaderMutex( bool ) override {}
	void ShaderLock() override {}
	void ShaderUnlock() override {}
	void EnableHWMorphing( bool bEnable ) override {}
	ImageFormat GetNullTextureFormat( void ) override { 	return IMAGE_FORMAT_ABGR8888; }	// stub
	void PushDeformation( DeformationBase_t const *Deformation ) override
	{
	}

	void PopDeformation( ) override
	{
	}

	int GetNumActiveDeformations() const override
	{
		return 0;
	}


	void ExecuteCommandBuffer( uint8 *pBuf ) override
	{
	}

	void SetStandardTextureHandle(StandardTextureId_t,ShaderAPITextureHandle_t) override
	{
	}

	void SetPSNearAndFarZ( int pshReg ) override
	{
	}

	void CopyRenderTargetToScratchTexture(ShaderAPITextureHandle_t srcRt,
		ShaderAPITextureHandle_t dstTex, Rect_t* pSrcRect = NULL, Rect_t* pDstRect = NULL) override
	{
		Assert(0);
	}

	void LockRect( void** pOutBits, int* pOutPitch, ShaderAPITextureHandle_t texHandle, int mipmap,
		int x, int y, int w, int h, bool bWrite, bool bRead ) override
	{
		Assert(0);
	}

	void UnlockRect( ShaderAPITextureHandle_t texHandle, int mipmap ) override
	{
		Assert(0);
	}

	// Set the finest mipmap that can be used for the texture which is currently being modified. 
	void TexLodClamp(int finest) override
	{
		Assert(0);
	}

	// Set the Lod Bias for the texture which is currently being modified. 
	void TexLodBias( float bias ) override
	{
		Assert(0);
	}
	
	void CopyTextureToTexture( ShaderAPITextureHandle_t srcTex, ShaderAPITextureHandle_t dstTex ) override
	{
		Assert(0);
	}

	int GetPackedDeformationInformation( int nMaskOfUnderstoodDeformations,
										 float *pConstantValuesOut,
										 int nBufferSize,
										 int nMaximumDeformations,
										 int *pNumDefsOut ) const override
	{
		*pNumDefsOut = 0;
		return 0;
	}
	
	bool OwnGPUResources( bool bEnable ) override
	{
		return false;
	}

private:
	enum
	{
		TRANSLUCENT = 0x1,
		ALPHATESTED = 0x2,
		VERTEX_AND_PIXEL_SHADERS = 0x4,
		DEPTHWRITE = 0x8,
	};
	void EnableAlphaToCoverage() override {}
	void DisableAlphaToCoverage() override {}

	ImageFormat GetShadowDepthTextureFormat() override { return IMAGE_FORMAT_UNKNOWN; }

	//
	// NOTE: Under here are real methods being used by dx10 implementation
	// above is stuff I still have to port over.
	//
private:
	void ClearShaderState( ShaderStateDx10_t* pState );
	void CommitStateChanges( bool bForce = false );

private:
	CMeshDx10 m_Mesh;

	bool m_bResettingRenderState : 1;
	CFunctionCommit m_Commit;
	ShaderStateDx10_t m_DesiredState;
	ShaderStateDx10_t m_CurrentState;
};


//-----------------------------------------------------------------------------
// Singleton global
//-----------------------------------------------------------------------------
extern CShaderAPIDx10* g_pShaderAPIDx10;

#endif // SHADERAPIDX10_H

