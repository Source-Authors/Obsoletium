//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//
// The dx8 implementation of the shader API
//===========================================================================//

/*
DX9 todo:
-make the transforms in the older shaders match the transforms in lightmappedgeneric
-fix polyoffset for hardware that doesn't support D3DRS_SLOPESCALEDEPTHBIAS and D3DRS_DEPTHBIAS
	- code is there, I think matrix offset just needs tweaking
-fix forcehardwaresync - implement texture locking for hardware that doesn't support async query
-get the format for GetAdapterModeCount and EnumAdapterModes from somewhere (shaderapidx8.cpp, GetModeCount, GetModeInfo)
-record frame sync objects (allocframesyncobjects, free framesync objects, ForceHardwareSync)
-Need to fix ENVMAPMASKSCALE, BUMPOFFSET in lightmappedgeneric*.cpp and vertexlitgeneric*.cpp
fix this:
		// FIXME: This also depends on the vertex format and whether or not we are static lit in dx9
	#ifndef SHADERAPIDX9
		if (m_DynamicState.m_VertexShader != shader) // garymcthack
	#endif // !SHADERAPIDX9
unrelated to dx9:
mat_fullbright 1 doesn't work properly on alpha materials in testroom_standards
*/
#define DISABLE_PROTECTED_THINGS
#include "shaderapidx8.h"
#include "shaderapidx8_global.h"
#include "shadershadowdx8.h"
#include "locald3dtypes.h"
#include "IHardwareConfigInternal.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapi/commandbuffer.h"

#include "materialsystem/deformations.h"
#include "materialsystem/idebugtextureinfo.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imorph.h"
#include "materialsystem/itexture.h"
#include "materialsystem/materialsystem_config.h"
#include "imaterialinternal.h"

#include "imeshdx8.h"
#include "colorformatdx8.h"
#include "texturedx8.h"
#include "shaderdevicedx8.h"
#include "vertexshaderdx8.h"
#include "recording.h"
#include "TransitionTable.h"
#include "vertexdecl.h"

#include "tier0/vcrmode.h"
#include "tier0/vprof.h"
#include "tier0/tslist.h"
#include "tier0/icommandline.h"

#include "tier1/interface.h"
#include "tier1/utlstack.h"
#include "tier1/utlvector.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlsymbol.h"
#include "tier1/strtools.h"
#include "tier1/tier1.h"
#include "tier1/utlbuffer.h"
#include "tier1/utllinkedlist.h"
#include "tier1/convar.h"
#include "tier1/KeyValues.h"

#include "tier2/tier2.h"

#include "bitmap/imageformat.h"
#include "bitmap/tgawriter.h"

#include <crtmemdebug.h>

#include "filesystem.h"
#include "worldsize.h"
#include "mathlib/mathlib.h"
#include "IShaderSystem.h"
#include "Color.h"
#ifdef RECORDING
#include "materialsystem/IShader.h"
#endif
#include "../stdshaders/common_hlsl_cpp_consts.h" // hack hack hack!
#include "vtf/vtf.h"
#include "datacache/idatacache.h"
#include "renderparm.h"
#include "com_ptr.h"
#include "wmi.h"
#include "filesystem/IQueuedLoader.h"
#include "togl/rendermechanism.h"

// Define this if you want to use a stubbed d3d.
//#define STUBD3D

#ifdef STUBD3D
#include "stubd3ddevice.h"
#endif

#include "winutils.h"
#include <DirectXMath/MatrixStack/DirectXMatrixStack.h>

#include "windows/com_error_category.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( OSX )
	typedef unsigned int DWORD;
	typedef DWORD* LPDWORD;
#endif

ConVar mat_texture_limit( "mat_texture_limit", "-1", FCVAR_NEVER_AS_STRING, 
	"If this value is not -1, the material system will limit the amount of texture memory it uses in a frame."
	" Useful for identifying performance cliffs. The value is in kilobytes." );

ConVar mat_frame_sync_enable( "mat_frame_sync_enable", "1", FCVAR_CHEAT );
ConVar mat_frame_sync_force_texture( "mat_frame_sync_force_texture", "0", FCVAR_CHEAT, "Force frame syncing to lock a managed texture." );


extern ConVar mat_debugalttab;

#define ALLOW_SMP_ACCESS 0

#if ALLOW_SMP_ACCESS
static ConVar mat_use_smp( "mat_use_smp", "0" );
#endif

// TODO: Convars for driving PIX (not all hooked up yet...JasonM)
static ConVar r_pix_start( "r_pix_start", "0" );
static ConVar r_pix_recordframes( "r_pix_recordframes", "0" );



//-----------------------------------------------------------------------------
// These board states change with high frequency; are not shadowed
//-----------------------------------------------------------------------------
struct TextureStageState_t
{
	D3DTEXTURETRANSFORMFLAGS m_TextureTransformFlags;
	float					m_BumpEnvMat00;
	float					m_BumpEnvMat01;
	float					m_BumpEnvMat10;
	float					m_BumpEnvMat11;
};

struct SamplerState_t
{
	ShaderAPITextureHandle_t	m_BoundTexture;
	D3DTEXTUREADDRESS			m_UTexWrap;
	D3DTEXTUREADDRESS			m_VTexWrap;
	D3DTEXTUREADDRESS			m_WTexWrap;
	D3DTEXTUREFILTERTYPE		m_MagFilter;
	D3DTEXTUREFILTERTYPE		m_MinFilter;
	D3DTEXTUREFILTERTYPE		m_MipFilter;
	int							m_FinestMipmapLevel;
	float						m_LodBias;
	int							m_nAnisotropicLevel;
	bool						m_TextureEnable;
	bool						m_SRGBReadEnable;
};


//-----------------------------------------------------------------------------
// State related to vertex textures
//-----------------------------------------------------------------------------
struct VertexTextureState_t
{
	ShaderAPITextureHandle_t	m_BoundTexture;
	D3DTEXTUREADDRESS			m_UTexWrap;
	D3DTEXTUREADDRESS			m_VTexWrap;
	D3DTEXTUREFILTERTYPE		m_MagFilter;
	D3DTEXTUREFILTERTYPE		m_MinFilter;
	D3DTEXTUREFILTERTYPE		m_MipFilter;
};


enum TransformType_t
{
	TRANSFORM_IS_IDENTITY = 0,
	TRANSFORM_IS_CAMERA_TO_WORLD,
	TRANSFORM_IS_GENERAL,
};

enum TransformDirtyBits_t
{
	STATE_CHANGED_VERTEX_SHADER = 0x1,
	STATE_CHANGED_FIXED_FUNCTION = 0x2,
	STATE_CHANGED = 0x3
};

enum
{
	MAX_NUM_RENDERSTATES = ( D3DRS_BLENDOPALPHA+1 ),
//	MORPH_TARGET_FACTOR_COUNT = VERTEX_SHADER_MORPH_TARGET_FACTOR_COUNT * 4,
};

struct DynamicState_t
{
	// Constant color
	unsigned int	m_ConstantColor;

	// Normalize normals?
	bool			m_NormalizeNormals;

	// Viewport state
	D3DVIEWPORT9	m_Viewport;

	// Transform state
	DirectX::XMMATRIX	m_Transform[NUM_MATRIX_MODES];
	unsigned char	m_TransformType[NUM_MATRIX_MODES];
	unsigned char	m_TransformChanged[NUM_MATRIX_MODES];

	// Ambient light color
	D3DCOLOR		m_Ambient;
	D3DLIGHT		m_Lights[MAX_NUM_LIGHTS];
	LightDesc_t		m_LightDescs[MAX_NUM_LIGHTS];
	bool			m_LightEnable[MAX_NUM_LIGHTS];
	Vector4D		m_AmbientLightCube[6];
	unsigned char	m_LightChanged[MAX_NUM_LIGHTS];
	unsigned char	m_LightEnableChanged[MAX_NUM_LIGHTS];
	VertexShaderLightTypes_t	m_LightType[MAX_NUM_LIGHTS];
	Vector			m_vLightingOrigin;
	int				m_NumLights;

	// Shade mode
	D3DSHADEMODE	m_ShadeMode;

	// Clear color
	D3DCOLOR		m_ClearColor;

	// Fog
	D3DCOLOR		m_FogColor;
	float			m_PixelFogColor[4];
	bool			m_bFogGammaCorrectionDisabled;
	bool			m_FogEnable;
	MaterialFogMode_t	m_SceneFog;
	D3DFOGMODE		m_FogMode;
	float			m_FogStart;
	float			m_FogEnd;
	float			m_FogZ;
	float			m_FogMaxDensity;

	float			m_HeightClipZ;
	MaterialHeightClipMode_t m_HeightClipMode;	
	
	// user clip planes
	int				m_UserClipPlaneEnabled;
	int				m_UserClipPlaneChanged;
	DirectX::XMVECTOR		m_UserClipPlaneWorld[MAXUSERCLIPPLANES];
	DirectX::XMVECTOR		m_UserClipPlaneProj[MAXUSERCLIPPLANES];
	bool			m_UserClipLastUpdatedUsingFixedFunction;

	bool			m_FastClipEnabled;
	bool			m_bFastClipPlaneChanged;
	DirectX::XMVECTOR		m_FastClipPlane;

	// Used when overriding the user clip plane
	bool			m_bUserClipTransformOverride;
	DirectX::XMMATRIX		m_UserClipTransform;

	// Cull mode
	D3DCULL			m_DesiredCullMode;
	D3DCULL			m_CullMode;
	bool			m_bCullEnabled;
	
	// Skinning
	D3DVERTEXBLENDFLAGS	m_VertexBlend;
	int					m_NumBones;

	// Pixel and vertex shader constants...
	Vector4D*		m_pVectorVertexShaderConstant;
	BOOL*			m_pBooleanVertexShaderConstant;
	IntVector4D*	m_pIntegerVertexShaderConstant;
	Vector4D*		m_pVectorPixelShaderConstant;
	BOOL*			m_pBooleanPixelShaderConstant;
	IntVector4D*	m_pIntegerPixelShaderConstant;

	// Texture stage state
	TextureStageState_t m_TextureStage[MAX_TEXTURE_STAGES];
	SamplerState_t m_SamplerState[MAX_SAMPLERS];

	// Vertex texture stage state
	VertexTextureState_t m_VertexTextureState[SHADER_VERTEXTEXTURE_COUNT];

	DWORD m_RenderState[MAX_NUM_RENDERSTATES];

	RECT  m_ScissorRect;

	se::win::com::com_ptr<IDirect3DVertexDeclaration9> m_pVertexDecl;

	bool	m_bSRGBWritesEnabled;
	bool	m_bHWMorphingEnabled;

	float	m_DestAlphaDepthRange; //Dest alpha writes compress the depth to get better results. This holds the default setting that can be overriden with r_destalpharange

	DynamicState_t() = default;
	DynamicState_t& operator=( DynamicState_t const& ) = default;
	DynamicState_t( DynamicState_t const& ) = delete;
};

//-----------------------------------------------------------------------------
// Method to queue up dirty dynamic state change calls
//-----------------------------------------------------------------------------
typedef void (*StateCommitFunc_t)( IDirect3DDevice9 *pDevice, const DynamicState_t &desiredState, DynamicState_t &currentState, bool bForce );
static void CommitSetViewports( IDirect3DDevice9 *pDevice, const DynamicState_t &desiredState, DynamicState_t &currentState, bool bForce );

// NOTE: It's slightly memory inefficient, and definitely not typesafe,
// to put all commit funcs into the same table (vs, ff, per-draw, per-pass),
// but it makes the code a heck of a lot simpler and smaller.
enum CommitFunc_t
{
	COMMIT_FUNC_CommitVertexTextures = 0,
	COMMIT_FUNC_CommitFlexWeights,
	COMMIT_FUNC_CommitSetScissorRect,
	COMMIT_FUNC_CommitSetViewports,

	COMMIT_FUNC_COUNT,
	COMMIT_FUNC_BYTE_COUNT = ( COMMIT_FUNC_COUNT + 0x7 ) >> 3,
};

enum CommitFuncType_t
{
	COMMIT_PER_DRAW = 0,
	COMMIT_PER_PASS,

	COMMIT_FUNC_TYPE_COUNT,
};

enum CommitShaderType_t
{
	COMMIT_FIXED_FUNCTION = 0,
	COMMIT_VERTEX_SHADER,
	COMMIT_ALWAYS,

	COMMIT_SHADER_TYPE_COUNT,
};


#define ADD_COMMIT_FUNC( _func, _shader, _func_name )	\
	if ( !IsCommitFuncInUse( _func, _shader, COMMIT_FUNC_ ## _func_name ) )	\
	{																		\
		AddCommitFunc( _func, _shader, _func_name );						\
		MarkCommitFuncInUse( _func, _shader, COMMIT_FUNC_ ## _func_name );	\
	}

#define ADD_RENDERSTATE_FUNC( _func, _shader, _func_name, _state, _val )	\
	if ( m_bResettingRenderState || (m_DesiredState._state != _val) )	\
	{																		\
		m_DesiredState._state = _val;									\
		ADD_COMMIT_FUNC( _func, _shader, _func_name )						\
	}

#define ADD_VERTEX_TEXTURE_FUNC( _func, _shader, _func_name, _stage, _state, _val )	\
	Assert( _stage >= SHADER_VERTEXTEXTURE_SAMPLER0 && _stage < SHADER_VERTEXTEXTURE_COUNT );							\
	if ( m_bResettingRenderState || (m_DesiredState.m_VertexTextureState[_stage]._state != _val) )	\
	{																		\
		m_DesiredState.m_VertexTextureState[_stage]._state = _val;		\
		ADD_COMMIT_FUNC( _func, _shader, _func_name )						\
	}


//-----------------------------------------------------------------------------
// Check render state support at compile time instead of runtime
//-----------------------------------------------------------------------------
#define SetSupportedRenderState( _state, _val )										\
	{																				\
		if( _state != D3DRS_NOTSUPPORTED )											\
		{																			\
			SetRenderState( _state, _val, false );									\
		}																			\
	}

#define SetSupportedRenderStateForce( _state, _val )								\
	{																				\
		if( _state != D3DRS_NOTSUPPORTED )											\
		{																			\
			SetRenderStateForce( _state, _val );									\
		}																			\
	}

//-----------------------------------------------------------------------------
// Allocated textures
//-----------------------------------------------------------------------------
struct Texture_t
{
	Texture_t()
	{
		m_Flags = 0;
		m_Count = 1;
		m_CountIndex = 0;
		m_nTimesBoundMax = 0;
		m_nTimesBoundThisFrame = 0;
		m_pTexture = NULL;
		m_ppTexture = NULL;
		m_ImageFormat = IMAGE_FORMAT_RGBA8888;
		m_pTextureGroupCounterGlobal = NULL;
		m_pTextureGroupCounterFrame = NULL;
		m_FinestMipmapLevel = 0;
		m_LodBias = 0.0f;
	}

	// FIXME: Compress this info
	D3DTEXTUREADDRESS		m_UTexWrap;
	D3DTEXTUREADDRESS		m_VTexWrap;
	D3DTEXTUREADDRESS		m_WTexWrap;
	D3DTEXTUREFILTERTYPE	m_MagFilter;
	D3DTEXTUREFILTERTYPE	m_MinFilter;
	D3DTEXTUREFILTERTYPE	m_MipFilter;
	int						m_FinestMipmapLevel;
	float					m_LodBias;

	unsigned char			m_NumLevels;
	unsigned char			m_SwitchNeeded;	// Do we need to advance the current copy?
	unsigned char			m_NumCopies;	// copies are used to optimize procedural textures
	unsigned char			m_CurrentCopy;	// the current copy we're using...

	int						m_CreationFlags;

	CUtlSymbol				m_DebugName;
	CUtlSymbol				m_TextureGroupName;
	uintp					*m_pTextureGroupCounterGlobal;	// Global counter for this texture's group.
	uintp					*m_pTextureGroupCounterFrame;	// Per-frame global counter for this texture's group.

	// stats stuff
	intp					m_SizeBytes;
	int						m_SizeTexels;
	int						m_LastBoundFrame;
	int						m_nTimesBoundMax;
	int						m_nTimesBoundThisFrame;

	enum Flags_t
	{
		IS_ALLOCATED             = 0x0001,
		IS_DEPTH_STENCIL         = 0x0002,
		IS_DEPTH_STENCIL_TEXTURE = 0x0004,	// depth stencil texture, not surface
		IS_RENDERABLE            = ( IS_DEPTH_STENCIL | IS_ALLOCATED ),
		IS_LOCKABLE				 = 0x0008,
		IS_FINALIZED             = 0x0010,	// 360: completed async hi-res load
		IS_FAILED                = 0x0020,	// 360: failed during load
		CAN_CONVERT_FORMAT       = 0x0040,	// 360: allow format conversion
		IS_LINEAR                = 0x0080,	// 360: unswizzled linear format
		IS_RENDER_TARGET         = 0x0100,	// 360: marks a render target texture source
		IS_RENDER_TARGET_SURFACE = 0x0200,	// 360: marks a render target surface target
		IS_VERTEX_TEXTURE		 = 0x0800,
	};

	short					m_Width;
	short					m_Height;
	short					m_Depth;
	unsigned short			m_Flags;

	typedef IDirect3DBaseTexture	*IDirect3DBaseTexturePtr;
	typedef IDirect3DBaseTexture	**IDirect3DBaseTexturePtrPtr;
	typedef IDirect3DSurface		*IDirect3DSurfacePtr;

	IDirect3DBaseTexturePtr GetTexture( void )
	{
		Assert( m_NumCopies == 1 );
		Assert( !( m_Flags & IS_DEPTH_STENCIL ) );
		return m_pTexture;
	}
	IDirect3DBaseTexturePtr GetTexture( int copy )
	{
		Assert( m_NumCopies > 1 );
		Assert( !( m_Flags & IS_DEPTH_STENCIL ) );
		return m_ppTexture[copy];
	}
	IDirect3DBaseTexturePtrPtr &GetTextureArray( void )
	{
		Assert( m_NumCopies > 1 );
		Assert( !( m_Flags & IS_DEPTH_STENCIL ) );
		return m_ppTexture;
	}

	IDirect3DSurfacePtr &GetDepthStencilSurface( void )
	{
		Assert( m_NumCopies == 1 );
		Assert( (m_Flags & IS_DEPTH_STENCIL) );
		return m_pDepthStencilSurface;
	}

	IDirect3DSurfacePtr &GetRenderTargetSurface( bool bSRGB )
	{
		Assert( m_NumCopies == 1 );
		Assert( m_Flags & IS_RENDER_TARGET_SURFACE );
		return m_pRenderTargetSurface[bSRGB];
	}

	void SetTexture( IDirect3DBaseTexturePtr pPtr )
	{
		m_pTexture = pPtr;
	}
	void SetTexture( int copy, IDirect3DBaseTexturePtr pPtr )
	{
		m_ppTexture[copy] = pPtr;
	}

	intp GetMemUsage() const
	{
		return m_SizeBytes;
	}

	int GetWidth() const
	{
		return ( int )m_Width;
	}

	int GetHeight() const
	{
		return ( int )m_Height;
	}

	int GetDepth() const
	{
		return ( int )m_Depth;
	}

	int GetLodClamp() const
	{
		return m_FinestMipmapLevel;
	}

	void SetImageFormat( ImageFormat format )
	{
		m_ImageFormat = format;
	}
	ImageFormat GetImageFormat() const
	{
		return m_ImageFormat;
	}

private:
	union
	{
		IDirect3DBaseTexture	*m_pTexture;				// used when there's one copy
		IDirect3DBaseTexture	**m_ppTexture;				// used when there are more than one copies
		IDirect3DSurface		*m_pDepthStencilSurface;	// used when there's one depth stencil surface
		IDirect3DSurface		*m_pRenderTargetSurface[2];
	};

	ImageFormat m_ImageFormat;

public:
	short m_Count;
	short m_CountIndex;

	short GetCount() const
	{
		return m_Count;
	}
};

#define MAX_DEFORMATION_PARAMETERS 16
#define DEFORMATION_STACK_DEPTH 10

struct Deformation_t
{
	int m_nDeformationType;
	int m_nNumParameters;
	float m_flDeformationParameters[MAX_DEFORMATION_PARAMETERS];
};


//-----------------------------------------------------------------------------
// The DX8 implementation of the shader API
//-----------------------------------------------------------------------------
class CShaderAPIDx8 final : public CShaderDeviceDx8, public IShaderAPIDX8, public IDebugTextureInfo
{
	typedef CShaderDeviceDx8 BaseClass;

public:
	// constructor, destructor
	CShaderAPIDx8( );
	~CShaderAPIDx8();

	// Methods of IShaderAPI
public:
	virtual void SetViewports( int nCount, const ShaderViewport_t* pViewports );
	virtual int  GetViewports( ShaderViewport_t* pViewports, int nMax ) const;
	virtual void ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil, int renderTargetWidth, int renderTargetHeight );
	virtual void ClearColor3ub( unsigned char r, unsigned char g, unsigned char b );
	virtual void ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a );
	virtual void BindVertexShader( VertexShaderHandle_t hVertexShader );
	virtual void BindGeometryShader( GeometryShaderHandle_t hGeometryShader );
	virtual void BindPixelShader( PixelShaderHandle_t hPixelShader );
	virtual void SetRasterState( const ShaderRasterState_t& state );
	virtual void SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights );

	// Methods of IShaderDynamicAPI
public:
	void GetBackBufferDimensions( int &nWidth, int &nHeight ) const override
	{
		// Chain to the device
		BaseClass::GetBackBufferDimensions( nWidth, nHeight );
	}
	void MarkUnusedVertexFields( unsigned int nFlags, int nTexCoordCount, bool *pUnusedTexCoords ) override;

public:
	// Methods of CShaderAPIBase
	bool OnDeviceInit() override;
	void OnDeviceShutdown() override;
	void ReleaseShaderObjects() override;
	void RestoreShaderObjects() override;
	void BeginPIXEvent( unsigned long color, const char *szName ) override;
	void EndPIXEvent() override;
	void AdvancePIXFrame() override;

public:
	// Methods of IShaderAPIDX8
	virtual void QueueResetRenderState();

	//
	// Abandon all hope ye who pass below this line which hasn't been ported.
	//

	// Sets the mode...
	bool SetMode( void* VD3DHWND, unsigned nAdapter, const ShaderDeviceInfo_t &info );

	// Change the video mode after it's already been set.
	void ChangeVideoMode( const ShaderDeviceInfo_t &info );

	// Sets the default render state
	void SetDefaultState();

	// Methods to ask about particular state snapshots
	virtual bool IsTranslucent( StateSnapshot_t id ) const;
	virtual bool IsAlphaTested( StateSnapshot_t id ) const;
	virtual bool UsesVertexAndPixelShaders( StateSnapshot_t id ) const;
	virtual int CompareSnapshots( StateSnapshot_t snapshot0, StateSnapshot_t snapshot1 );

	// Computes the vertex format for a particular set of snapshot ids
	VertexFormat_t ComputeVertexFormat( int num, StateSnapshot_t* pIds ) const;
	VertexFormat_t ComputeVertexUsage( int num, StateSnapshot_t* pIds ) const;

	// What fields in the morph do we actually use?
	virtual MorphFormat_t ComputeMorphFormat( int numSnapshots, StateSnapshot_t* pIds ) const;

	// Uses a state snapshot
	void UseSnapshot( StateSnapshot_t snapshot );

	// Color state
	void Color3f( float r, float g, float b );
	void Color4f( float r, float g, float b, float a );
	void Color3fv( float const* c );
	void Color4fv( float const* c );

	void Color3ub( unsigned char r, unsigned char g, unsigned char b );
	void Color3ubv( unsigned char const* pColor );
	void Color4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a );
	void Color4ubv( unsigned char const* pColor );

	// Set the number of bone weights
	virtual void SetNumBoneWeights( int numBones );
	virtual void EnableHWMorphing( bool bEnable );

	// Sets the vertex and pixel shaders
	virtual void SetVertexShaderIndex( int vshIndex = -1 );
	virtual void SetPixelShaderIndex( int pshIndex = 0 );

	// Matrix state
	void MatrixMode( MaterialMatrixMode_t matrixMode );
	void PushMatrix();
	void PopMatrix();
	void LoadMatrix( float *m );
	void LoadBoneMatrix( int boneIndex, const float *m );
	void MultMatrix( float *m );
	void MultMatrixLocal( float *m );
	void GetMatrix( MaterialMatrixMode_t matrixMode, float *dst );
	void LoadIdentity( void );
	void LoadCameraToWorld( void );
	void Ortho( double left, double top, double right, double bottom, double zNear, double zFar );
	void PerspectiveX( double fovx, double aspect, double zNear, double zFar );
	void PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right );
	void PickMatrix( int x, int y, int width, int height );
	void Rotate( float angle, float x, float y, float z );
	void Translate( float x, float y, float z );
	void Scale( float x, float y, float z );
	void ScaleXY( float x, float y );

	// Binds a particular material to render with
	void Bind( IMaterial* pMaterial );
	IMaterialInternal* GetBoundMaterial();

	// Level of anisotropic filtering
	virtual void SetAnisotropicLevel( int nAnisotropyLevel );

	virtual void	SyncToken( const char *pToken );

	// Cull mode
	void CullMode( MaterialCullMode_t cullMode );

	// Force writes only when z matches. . . useful for stenciling things out
	// by rendering the desired Z values ahead of time.
	void ForceDepthFuncEquals( bool bEnable );

	// Turns off Z buffering
	void OverrideDepthEnable( bool bEnable, bool bDepthEnable );

	void OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable );
	void OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable );

	void SetHeightClipZ( float z );
	void SetHeightClipMode( enum MaterialHeightClipMode_t heightClipMode );

	void SetClipPlane( int index, const float *pPlane );
	void EnableClipPlane( int index, bool bEnable );
	
	void SetFastClipPlane(const float *pPlane);
	void EnableFastClip(bool bEnable);
	
	// The shade mode
	void ShadeMode( ShaderShadeMode_t mode );

	// Vertex blend state
	void SetVertexBlendState( int numBones );

	// Gets the dynamic mesh
	IMesh* GetDynamicMesh( IMaterial* pMaterial, int nHWSkinBoneCount, bool buffered,
		IMesh* pVertexOverride, IMesh* pIndexOverride );
	IMesh* GetDynamicMeshEx( IMaterial* pMaterial, VertexFormat_t vertexFormat, int nHWSkinBoneCount, 
		bool bBuffered, IMesh* pVertexOverride, IMesh* pIndexOverride );
	IMesh *GetFlexMesh();

	// Returns the number of vertices we can render using the dynamic mesh
	virtual void GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices );
	virtual int GetMaxVerticesToRender( IMaterial *pMaterial );
	virtual int GetMaxIndicesToRender( );

	// Draws the mesh
	void DrawMesh( CMeshBase* mesh );

	// modifies the vertex data when necessary
	void ModifyVertexData( );

	// Draws
	void BeginPass( StateSnapshot_t snapshot  );
	void RenderPass( int nPass, int nPassCount );

	// We use smaller dynamic VBs during level transitions, to free up memory
	virtual int  GetCurrentDynamicVBSize( void );
	virtual void DestroyVertexBuffers( bool bExitingLevel = false );

	void SetVertexDecl( VertexFormat_t vertexFormat, bool bHasColorMesh, bool bUsingFlex, bool bUsingMorph );

	// Sets the constant register for vertex and pixel shaders
	FORCEINLINE void SetVertexShaderConstantInternal( int var, float const* pVec, int numVecs = 1, bool bForce = false );

	void SetVertexShaderConstant( int var, float const* pVec, int numVecs = 1, bool bForce = false );
	void SetBooleanVertexShaderConstant( int var, BOOL const* pVec, int numBools = 1, bool bForce = false );
	void SetIntegerVertexShaderConstant( int var, int const* pVec, int numIntVecs = 1, bool bForce = false );
	
	void SetPixelShaderConstant( int var, float const* pVec, int numVecs = 1, bool bForce = false );
	FORCEINLINE void SetPixelShaderConstantInternal( int var, float const* pValues, int nNumConsts, bool bForce );

	void SetBooleanPixelShaderConstant( int var, BOOL const* pVec, int numBools = 1, bool bForce = false );
	void SetIntegerPixelShaderConstant( int var, int const* pVec, int numIntVecs = 1, bool bForce = false );

	void InvalidateDelayedShaderConstants( void );

	// Returns the nearest supported format
	ImageFormat GetNearestSupportedFormat( ImageFormat fmt, bool bFilteringRequired = true ) const;
	ImageFormat GetNearestRenderTargetFormat( ImageFormat format ) const;
	virtual bool DoRenderTargetsNeedSeparateDepthBuffer() const;

	// stuff that shouldn't be used from within a shader 
    void ModifyTexture( ShaderAPITextureHandle_t textureHandle );
	void BindTexture( Sampler_t sampler, ShaderAPITextureHandle_t textureHandle );
	virtual void BindVertexTexture( VertexTextureSampler_t nStage, ShaderAPITextureHandle_t textureHandle );
	void DeleteTexture( ShaderAPITextureHandle_t textureHandle );

	void WriteTextureToFile( ShaderAPITextureHandle_t hTexture, const char *szFileName );

	bool IsTexture( ShaderAPITextureHandle_t textureHandle );
	bool IsTextureResident( ShaderAPITextureHandle_t textureHandle );
	FORCEINLINE bool TextureIsAllocated( ShaderAPITextureHandle_t hTexture )
	{
		return m_Textures.IsValidIndex( hTexture ) && ( GetTexture( hTexture ).m_Flags & Texture_t::IS_ALLOCATED );
	}
	FORCEINLINE void AssertValidTextureHandle( ShaderAPITextureHandle_t textureHandle )
	{
#ifdef _DEBUG
		Assert( TextureIsAllocated( textureHandle ) );
#endif
	}

	// Lets the shader know about the full-screen texture so it can 
	virtual void SetFullScreenTextureHandle( ShaderAPITextureHandle_t h );

	virtual void SetLinearToGammaConversionTextures( ShaderAPITextureHandle_t hSRGBWriteEnabledTexture, ShaderAPITextureHandle_t hIdentityTexture );

	// Set the render target to a texID.
	// Set to SHADER_RENDERTARGET_BACKBUFFER if you want to use the regular framebuffer.
	void SetRenderTarget( ShaderAPITextureHandle_t colorTextureHandle = SHADER_RENDERTARGET_BACKBUFFER, 
		ShaderAPITextureHandle_t depthTextureHandle = SHADER_RENDERTARGET_DEPTHBUFFER );
	// Set the render target to a texID.
	// Set to SHADER_RENDERTARGET_BACKBUFFER if you want to use the regular framebuffer.
	void SetRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t colorTextureHandle = SHADER_RENDERTARGET_BACKBUFFER, 
		ShaderAPITextureHandle_t depthTextureHandle = SHADER_RENDERTARGET_DEPTHBUFFER );

	// These are bound to the texture, not the texture environment
	void TexMinFilter( ShaderTexFilterMode_t texFilterMode );
	void TexMagFilter( ShaderTexFilterMode_t texFilterMode );
	void TexWrap( ShaderTexCoordComponent_t coord, ShaderTexWrapMode_t wrapMode );
	void TexSetPriority( int priority );
	void TexLodClamp( int finest );
	void TexLodBias( float bias );

	ShaderAPITextureHandle_t CreateTextureHandle( void );
	void CreateTextureHandles( ShaderAPITextureHandle_t *handles, int count );

	ShaderAPITextureHandle_t CreateTexture( 
		int width, 
		int height,
		int depth,
		ImageFormat dstImageFormat, 
		int numMipLevels, 
		int numCopies, 
		int creationFlags, 
		const char *pDebugName,
		const char *pTextureGroupName );

	// Create a multi-frame texture (equivalent to calling "CreateTexture" multiple times, but more efficient)
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
		const char *pTextureGroupName );

	ShaderAPITextureHandle_t CreateDepthTexture( 
		ImageFormat renderTargetFormat, 
		int width, 
		int height, 
		const char *pDebugName,
		bool bTexture );

	void TexImage2D( 
		int level, 
		int cubeFaceID, 
		ImageFormat dstFormat, 
		int zOffset, 
		int width, 
		int height, 
		ImageFormat srcFormat, 
		bool bSrcIsTiled,
		void *imageData );

	void TexSubImage2D( 
		int level, 
		int cubeFaceID, 
		int xOffset, 
		int yOffset, 
		int zOffset, 
		int width, 
		int height,
		ImageFormat srcFormat, 
		int srcStride, 
		bool bSrcIsTiled,
		void *imageData );

	void TexImageFromVTF( IVTFTexture *pVTF, int iVTFFrame );

	bool TexLock( int level, int cubeFaceID, int xOffset, int yOffset, int width, int height, CPixelWriter& writer );
	void TexUnlock( );

	// stuff that isn't to be used from within a shader
	// what's the best way to hide this? subclassing?
	virtual void ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth );
	virtual void ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth );
	virtual void PerformFullScreenStencilOperation( void );
	void ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat );
	virtual void ReadPixels( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *data, ImageFormat dstFormat, int nDstStride );

	// Gets the current buffered state... (debug only)
	void GetBufferedState( BufferedState_t& state );

	// Buffered primitives
	void FlushBufferedPrimitives();
	void FlushBufferedPrimitivesInternal( );

	// Make sure we finish drawing everything that has been requested 
	void FlushHardware();
	
	// Use this to begin and end the frame
	void BeginFrame();
	void EndFrame();

	// Used to clear the transition table when we know it's become invalid.
	void ClearSnapshots();

	// Backward compat
	virtual int GetActualTextureStageCount() const;
	virtual int GetActualSamplerCount() const;
	virtual int StencilBufferBits() const;
	virtual bool IsAAEnabled() const;	// Is antialiasing being used?
	virtual bool OnAdapterSet( );
	bool m_bAdapterSet;

	void UpdateFastClipUserClipPlane( void );
	bool ReadPixelsFromFrontBuffer() const;

	// returns the current time in seconds....
	double CurrentTime() const;

	// Get the current camera position in world space.
	void GetWorldSpaceCameraPosition( float* pPos ) const;

	// Fog methods
	void FogMode( MaterialFogMode_t fogMode );
	void FogStart( float fStart );
	void FogEnd( float fEnd );
	void FogMaxDensity( float flMaxDensity );
	void SetFogZ( float fogZ );
	void GetFogDistances( float *fStart, float *fEnd, float *fFogZ );

	void SceneFogMode( MaterialFogMode_t fogMode );
	MaterialFogMode_t GetSceneFogMode( );
	MaterialFogMode_t GetPixelFogMode( );
	int GetPixelFogCombo( );//0 is either range fog, or no fog simulated with rigged range fog values. 1 is height fog
	bool ShouldUsePixelFogForMode( MaterialFogMode_t fogMode );
	void SceneFogColor3ub( unsigned char r, unsigned char g, unsigned char b );
	void GetSceneFogColor( unsigned char *rgb );
	void GetSceneFogColor( unsigned char *r, unsigned char *g, unsigned char *b );
	
	// Selection mode methods
	int  SelectionMode( bool selectionMode );
	void SelectionBuffer( unsigned int* pBuffer, int size );
	void ClearSelectionNames( );
	void LoadSelectionName( int name );
	void PushSelectionName( int name );
	void PopSelectionName();
	bool IsInSelectionMode() const;
	void RegisterSelectionHit( float minz, float maxz );
	void WriteHitRecord();

	// Binds a standard texture
	virtual void BindStandardTexture( Sampler_t sampler, StandardTextureId_t id );
	virtual void BindStandardVertexTexture( VertexTextureSampler_t sampler, StandardTextureId_t id );
	virtual void GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id );

	// Gets the lightmap dimensions
	virtual void GetLightmapDimensions( int *w, int *h );

	// Use this to get the mesh builder that allows us to modify vertex data
	CMeshBuilder* GetVertexModifyBuilder();

	virtual bool InFlashlightMode() const;
	virtual bool InEditorMode() const;

	// Gets the bound morph's vertex format; returns 0 if no morph is bound
	virtual MorphFormat_t GetBoundMorphFormat();

	// Helper to get at the texture state stage
	TextureStageState_t& TextureStage( int stage ) { return m_DynamicState.m_TextureStage[stage]; }
	const TextureStageState_t& TextureStage( int stage ) const { return m_DynamicState.m_TextureStage[stage]; }
	SamplerState_t& SamplerState( int nSampler ) { return m_DynamicState.m_SamplerState[nSampler]; }
	const SamplerState_t& SamplerState( int nSampler ) const { return m_DynamicState.m_SamplerState[nSampler]; }

	void SetAmbientLight( float r, float g, float b );
	void SetLight( int lightNum, const LightDesc_t& desc );
	void SetLightingOrigin( Vector vLightingOrigin );
	void DisableAllLocalLights();
	void SetAmbientLightCube( Vector4D colors[6] );
	float GetAmbientLightCubeLuminance( void );

	int GetMaxLights( void ) const;
	const LightDesc_t& GetLight( int lightNum ) const;

	void SetVertexShaderStateAmbientLightCube();
	void SetPixelShaderStateAmbientLightCube( int pshReg, bool bForceToBlack = false );

	void CopyRenderTargetToTexture( ShaderAPITextureHandle_t textureHandle );
	void CopyRenderTargetToTextureEx( ShaderAPITextureHandle_t textureHandle, int nRenderTargetID, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL );
	void CopyTextureToRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t textureHandle, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL );
	void CopyRenderTargetToScratchTexture( ShaderAPITextureHandle_t srcHandle, ShaderAPITextureHandle_t dstHandle, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL );

	virtual void LockRect( void** pOutBits, int* pOutPitch, ShaderAPITextureHandle_t texHandle, int mipmap, int x, int y, int w, int h, bool bWrite, bool bRead );
	virtual void UnlockRect( ShaderAPITextureHandle_t texHandle, int mipmap );

	virtual void CopyTextureToTexture( ShaderAPITextureHandle_t srcTex, ShaderAPITextureHandle_t dstTex );

	// Returns the cull mode (for fill rate computation)
	D3DCULL GetCullMode() const;
	void SetCullModeState( bool bEnable, D3DCULL nDesiredCullMode );
	void ApplyCullEnable( bool bEnable ) override;

	// Alpha to coverage
	void ApplyAlphaToCoverage( bool bEnable ) override;

	// Applies Z Bias
	void ApplyZBias( const ShadowState_t& shaderState );

	// Applies texture enable
	void ApplyTextureEnable( const ShadowState_t& state, int stage );

	void ApplyFogMode( ShaderFogMode_t fogMode, bool bSRGBWritesEnabled, bool bDisableFogGammaCorrection ) override;
	void UpdatePixelFogColorConstant( void );

	void EnabledSRGBWrite( bool bEnabled );

	// Gamma<->Linear conversions according to the video hardware we're running on
	float GammaToLinear_HardwareSpecific( float fGamma ) const;
	float LinearToGamma_HardwareSpecific( float fLinear ) const;

	// Applies alpha blending
	void ApplyAlphaBlend( bool bEnable, D3DBLEND srcBlend, D3DBLEND destBlend );

	// Applies alpha texture op
	void ApplyColorTextureStage( int stage, D3DTEXTUREOP op, int arg1, int arg2 );
	void ApplyAlphaTextureStage( int stage, D3DTEXTUREOP op, int arg1, int arg2 );

	// Sets texture stage stage + render stage state
	void SetSamplerState( int stage, D3DSAMPLERSTATETYPE state, DWORD val );
	void SetTextureStageState( int stage, D3DTEXTURESTAGESTATETYPE state, DWORD val);
	void SetRenderStateForce( D3DRENDERSTATETYPE state, DWORD val );
	void SetRenderState( D3DRENDERSTATETYPE state, DWORD val, 
						 bool bFlushBufferedPrimitivesIfChanged = false);

	// Scissor Rect
	void SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor );
	// Can we download textures?
	virtual bool CanDownloadTextures() const;

	void ForceHardwareSync_WithManagedTexture();
	void ForceHardwareSync( void );
	void UpdateFrameSyncQuery( int queryIndex, bool bIssue );

	void EvictManagedResources();

	virtual void EvictManagedResourcesInternal();

	// Gets at a particular transform
	inline const DirectX::XMMATRIX& GetTransform( int i )
	{
		return *m_pMatrixStack[i]->GetTop();
	}

	int GetCurrentNumBones( void ) const;
	bool IsHWMorphingEnabled( ) const;
	int GetCurrentLightCombo( void ) const;				// Used for DX8 only
	void GetDX9LightState( LightState_t *state ) const;	// Used for DX9 only

	MaterialFogMode_t GetCurrentFogType( void ) const;

	void RecordString( const char *pStr );

	virtual bool IsRenderingMesh() const { return m_pRenderMesh != 0; }

	void SetTextureTransformDimension( TextureStage_t textureStage, int dimension, bool projected );
	void DisableTextureTransform( TextureStage_t textureMatrix );
	void SetBumpEnvMatrix( TextureStage_t textureStage, float m00, float m01, float m10, float m11 );

	int GetCurrentFrameCounter( void ) const
	{
		return m_CurrentFrame;
	}
 
	// Workaround hack for visualization of selection mode
	virtual void SetupSelectionModeVisualizationState();

	// Allocate and delete query objects.
	virtual ShaderAPIOcclusionQuery_t CreateOcclusionQueryObject( void );
	virtual void DestroyOcclusionQueryObject( ShaderAPIOcclusionQuery_t h );

	// Bracket drawing with begin and end so that we can get counts next frame.
	virtual void BeginOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t h );
	virtual void EndOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t h );

	// Get the number of pixels rendered between begin and end on an earlier frame.
	// Calling this in the same frame is a huge perf hit!
	virtual int OcclusionQuery_GetNumPixelsRendered( ShaderAPIOcclusionQuery_t h, bool bFlush );

	void SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture );
	void SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture );
	const FlashlightState_t &GetFlashlightState( VMatrix &worldToTexture ) const;
	const FlashlightState_t &GetFlashlightStateEx( VMatrix &worldToTexture, ITexture **pFlashlightDepthTexture ) const;

	// Gets at the shadow state for a particular state snapshot
	virtual bool IsDepthWriteEnabled( StateSnapshot_t id ) const;

// IDebugTextureInfo implementation.

	virtual bool IsDebugTextureListFresh( int numFramesAllowed = 1 );
	virtual void EnableDebugTextureList( bool bEnable );
	virtual bool SetDebugTextureRendering( bool bEnable );
	virtual void EnableGetAllTextures( bool bEnable );
	virtual KeyValues* GetDebugTextureList();
	virtual int GetTextureMemoryUsed( TextureMemoryType eTextureMemory );

	virtual void ClearVertexAndPixelShaderRefCounts();
	virtual void PurgeUnusedVertexAndPixelShaders();

	// Called when the dx support level has changed
	virtual void DXSupportLevelChanged();

	// User clip plane override
	virtual void EnableUserClipTransformOverride( bool bEnable );
	virtual void UserClipTransform( const VMatrix &worldToProjection );

	bool UsingSoftwareVertexProcessing() const;

	// Mark all user clip planes as being dirty
	void MarkAllUserClipPlanesDirty();

	// Converts a DirectX::XMMATRIX to a VMatrix and back
	void XM_CALLCONV XMMatrixToVMatrix( DirectX::XMMATRIX in, VMatrix& out );
	DirectX::XMMATRIX XM_CALLCONV VMatrixToXMMatrix( const VMatrix &in );

	ITexture *GetRenderTargetEx( int nRenderTargetID );

	virtual void SetToneMappingScaleLinear( const Vector &scale );
	virtual const Vector &GetToneMappingScaleLinear( void ) const;
	float GetLightMapScaleFactor( void ) const;

	void SetFloatRenderingParameter(int parm_number, float value);

	void SetIntRenderingParameter(int parm_number, int value);
	void SetVectorRenderingParameter(int parm_number, Vector const &value);

	float GetFloatRenderingParameter(int parm_number) const;

	int GetIntRenderingParameter(int parm_number) const;

	Vector GetVectorRenderingParameter(int parm_number) const;

	// For dealing with device lost in cases where Present isn't called all the time (Hammer)
	virtual void HandleDeviceLost();

	virtual void EnableLinearColorSpaceFrameBuffer( bool bEnable );

	virtual void SetPSNearAndFarZ( int pshReg );

	// stencil methods
	void SetStencilEnable(bool onoff);
	void SetStencilFailOperation(StencilOperation_t op);
	void SetStencilZFailOperation(StencilOperation_t op);
	void SetStencilPassOperation(StencilOperation_t op);
	void SetStencilCompareFunction(StencilComparisonFunction_t cmpfn);
	void SetStencilReferenceValue(int ref);
	void SetStencilTestMask(uint32 msk);
	void SetStencilWriteMask(uint32 msk);
	void ClearStencilBufferRectangle(int xmin, int ymin, int xmax, int ymax,int value);

	virtual void GetDXLevelDefaults(uint &max_dxlevel,uint &recommended_dxlevel);

	virtual bool OwnGPUResources( bool bEnable );

// ------------ New Vertex/Index Buffer interface ----------------------------
	void BindVertexBuffer( int streamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions = 1 );
	void BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes );
	void Draw( MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount );

	// Draw the mesh with the currently bound vertex and index buffers.
	void DrawWithVertexAndIndexBuffers( void );
// ------------ End ----------------------------

	// deformations
	virtual void PushDeformation( const DeformationBase_t *pDeformation );
	virtual void PopDeformation( );
	virtual intp GetNumActiveDeformations( ) const;


	// for shaders to set vertex shader constants. returns a packed state which can be used to set the dynamic combo
	virtual int GetPackedDeformationInformation( int nMaskOfUnderstoodDeformations,
												 float *pConstantValuesOut,
												 int nBufferSize,
												 int nMaximumDeformations,
												 int *pNumDefsOut ) const ;

	inline Texture_t &GetTexture( ShaderAPITextureHandle_t hTexture )
	{
		return m_Textures[hTexture];
	}

	// Gets the texture 
	IDirect3DBaseTexture* GetD3DTexture( ShaderAPITextureHandle_t hTexture );


	virtual bool ShouldWriteDepthToDestAlpha( void ) const;

	void AcquireThreadOwnership() override;
	void ReleaseThreadOwnership() override;

	// Shader creation, destruction
	HullShaderHandle_t CreateHullShader(IShaderBuffer* pShaderBuffer) override {
		return HULL_SHADER_HANDLE_INVALID;
	}
	void DestroyHullShader( HullShaderHandle_t hShader ) override {}
	DomainShaderHandle_t CreateDomainShader(IShaderBuffer* pShaderBuffer) override {
		return DOMAIN_SHADER_HANDLE_INVALID;
	}
	void DestroyDomainShader( DomainShaderHandle_t hShader ) override {}
	ComputeShaderHandle_t CreateComputeShader(IShaderBuffer* pShaderBuffer) override {
		return COMPUTE_SHADER_HANDLE_INVALID;
	}
	void DestroyComputeShader( ComputeShaderHandle_t hShader ) override {}

	void BindHullShader(HullShaderHandle_t hPixelShader) override {}
        void BindDomainShader(DomainShaderHandle_t hPixelShader) override {}
	void BindComputeShader(ComputeShaderHandle_t hPixelShader) override {}

private:
	enum
	{
		SMALL_BACK_BUFFER_SURFACE_WIDTH = 256,
		SMALL_BACK_BUFFER_SURFACE_HEIGHT = 256,
	};

	bool m_bEnableDebugTextureList;
	bool m_bDebugGetAllTextures;
	bool m_bDebugTexturesRendering;
	KeyValues *m_pDebugTextureList;
	intp m_nTextureMemoryUsedLastFrame, m_nTextureMemoryUsedTotal;
	intp m_nTextureMemoryUsedPicMip1, m_nTextureMemoryUsedPicMip2;
	int m_nDebugDataExportFrame;

	FlashlightState_t m_FlashlightState;
	VMatrix m_FlashlightWorldToTexture;
	ITexture *m_pFlashlightDepthTexture;

	CShaderAPIDx8( CShaderAPIDx8 const& ) = delete;
	CShaderAPIDx8& operator=( CShaderAPIDx8 const& ) = delete;

	enum
	{
		INVALID_TRANSITION_OP = 0xFFFF
	};

	// State transition table for the device is as follows:

	// Other app init causes transition from OK to OtherAppInit, during transition we must release resources
	// !Other app init causes transition from OtherAppInit to OK, during transition we must restore resources
	// Minimized or device lost or device not reset causes transition from OK to LOST_DEVICE, during transition we must release resources
	// Minimized or device lost or device not reset causes transition from OtherAppInit to LOST_DEVICE

	// !minimized AND !device lost causes transition from LOST_DEVICE to NEEDS_RESET
	// minimized or device lost causes transition from NEEDS_RESET to LOST_DEVICE

	// Successful TryDeviceReset and !Other app init causes transition from NEEDS_RESET to OK, during transition we must restore resources
	// Successful TryDeviceReset and Other app init causes transition from NEEDS_RESET to OtherAppInit

	void ExportTextureList();
	void AddBufferToTextureList( const char *pName, D3DSURFACE_DESC &desc );

	void SetupTextureGroup( ShaderAPITextureHandle_t hTexture, const char *pTextureGroupName );

	// Creates the matrix stack
	void CreateMatrixStacks();
	void FreeMatrixStacks();

	// Initializes the render state
	void InitRenderState( );

	// Resets all dx renderstates to dx default so that our shadows are correct.
	void ResetDXRenderState( );
	
	// Resets the render state
	void ResetRenderState( bool bFullReset = true );

	// Setup standard vertex shader constants (that don't change)
	void SetStandardVertexShaderConstants( float fOverbright );
	
	// Initializes vertex and pixel shaders
	void InitVertexAndPixelShaders();

	// Discards the vertex and index buffers
	void DiscardVertexBuffers();

	// Computes the fill rate
	void ComputeFillRate();

	// Takes a snapshot
	virtual StateSnapshot_t TakeSnapshot( );

	// Converts the clear color to be appropriate for HDR
	D3DCOLOR GetActualClearColor( D3DCOLOR clearColor );

	// We lost the device
	void OnDeviceLost();

	// Gets the matrix stack from the matrix mode
	int GetMatrixStack( MaterialMatrixMode_t mode ) const;

	// Flushes the matrix state, returns false if we don't need to
	// do any more work
	bool MatrixIsChanging( TransformType_t transform = TRANSFORM_IS_GENERAL );

	// Updates the matrix transform state
	void UpdateMatrixTransform( TransformType_t transform = TRANSFORM_IS_GENERAL );

	// Sets the vertex shader modelView state..
	// NOTE: GetProjectionMatrix should only be called from the Commit functions!
	const DirectX::XMMATRIX& GetProjectionMatrix( void );
	void SetVertexShaderViewProj();
	void SetVertexShaderModelViewProjAndModelView();

	void SetPixelShaderFogParams( int reg );
	void SetPixelShaderFogParams( int reg, ShaderFogMode_t fogMode );

	FORCEINLINE void UpdateVertexShaderFogParams( void )
	{
		if ( g_pHardwareConfig->Caps().m_SupportsPixelShaders )
		{
			float ooFogRange = 1.0f;

			float fStart = m_VertexShaderFogParams[0];
			float fEnd = m_VertexShaderFogParams[1];

			// Check for divide by zero
			if ( fEnd != fStart )
			{
				ooFogRange = 1.0f / ( fEnd - fStart );
			}

			float fogParams[4];
			fogParams[0] = ooFogRange * fEnd;
			fogParams[1] = 1.0f;
			fogParams[2] = 1.0f - clamp( m_flFogMaxDensity, 0.0f, 1.0f ); // Max fog density

			fogParams[3] = ooFogRange;

			float vertexShaderCameraPos[4];
			vertexShaderCameraPos[0] = m_WorldSpaceCameraPositon[0];
			vertexShaderCameraPos[1] = m_WorldSpaceCameraPositon[1];
			vertexShaderCameraPos[2] = m_WorldSpaceCameraPositon[2];
			vertexShaderCameraPos[3] = m_DynamicState.m_FogZ;  // waterheight

			// cFogEndOverFogRange, cFogOne, unused, cOOFogRange
			SetVertexShaderConstant( VERTEX_SHADER_FOG_PARAMS, fogParams, 1 );

			// eyepos.x eyepos.y eyepos.z cWaterZ
			SetVertexShaderConstant( VERTEX_SHADER_CAMERA_POS, vertexShaderCameraPos );
		}
	}

	// Compute stats info for a texture
	void ComputeStatsInfo( ShaderAPITextureHandle_t hTexture, bool isCubeMap, bool isVolumeTexture );

	// For procedural textures
	void AdvanceCurrentCopy( ShaderAPITextureHandle_t hTexture );

	// Deletes a D3D texture
	void DeleteD3DTexture( ShaderAPITextureHandle_t hTexture );

	// Unbinds a texture
	void UnbindTexture( ShaderAPITextureHandle_t hTexture );

	// Releases all D3D textures
	void ReleaseAllTextures();

	// Deletes all textures
	void DeleteAllTextures();

	// Gets the currently modified texture handle
	ShaderAPITextureHandle_t GetModifyTextureHandle() const;

	// Gets the bind id
	ShaderAPITextureHandle_t GetBoundTextureBindId( Sampler_t sampler ) const;
	
	// If mat_texture_limit is enabled, then this tells us if binding the specified texture would
	// take us over the limit.
	bool WouldBeOverTextureLimit( ShaderAPITextureHandle_t hTexture );

	// Sets the texture state
	void SetTextureState( Sampler_t sampler, ShaderAPITextureHandle_t hTexture, bool force = false );

	// Grab/release the internal render targets such as the back buffer and the save game thumbnail
	void AcquireInternalRenderTargets();
	void ReleaseInternalRenderTargets();

	// create/release linear->gamma table texture lookups. Only used by hardware supporting pixel shader 2b and up
	void AcquireLinearToGammaTableTextures();
	void ReleaseLinearToGammaTableTextures();

	// Gets the texture being modified
	IDirect3DBaseTexture* GetModifyTexture();
	void SetModifyTexture( IDirect3DBaseTexture *pTex );

	// returns true if we're using texture coordinates at a given stage
	bool IsUsingTextureCoordinates( int stage, int flags ) const;

	// Returns true if the board thinks we're generating spheremap coordinates
	bool IsSpheremapRenderStateActive( int stage ) const;

	// Returns true if we're modulating constant color into the vertex color
	bool IsModulatingVertexColor() const;

	// Recomputes ambient light cube
	void RecomputeAmbientLightCube( );

	// Debugging spew
	void SpewBoardState();

	// Compute and save the world space camera position.
	void CacheWorldSpaceCameraPosition();

	// Compute and save the projection atrix with polyoffset built in if we need it.
	void CachePolyOffsetProjectionMatrix();

	// Vertex shader helper functions
	int  FindVertexShader( VertexFormat_t fmt, char const* pFileName ) const;
	int  FindPixelShader( char const* pFileName ) const;

	// Returns copies of the front and back buffers
	se::win::com::com_ptr<IDirect3DSurface9> GetFrontBufferImage( ImageFormat& format );
	se::win::com::com_ptr<IDirect3DSurface9> GetBackBufferImage( Rect_t *pSrcRect, Rect_t *pDstRect, ImageFormat& format );
	se::win::com::com_ptr<IDirect3DSurface9> GetBackBufferImageHDR( Rect_t *pSrcRect, Rect_t *pDstRect, ImageFormat& format );

	// Copy bits from a host-memory surface
	void CopyBitsFromHostSurface( IDirect3DSurface* pSurfaceBits, 
		const Rect_t &dstRect, unsigned char *pData, ImageFormat srcFormat, ImageFormat dstFormat, int nDstStride );

	FORCEINLINE void SetTransform( D3DTRANSFORMSTATETYPE State, CONST DirectX::XMMATRIX *pMatrix )
	{
		Dx9Device()->SetTransform( State, reinterpret_cast<CONST D3DMATRIX *>(pMatrix) );
	}

	FORCEINLINE void SetLight( DWORD Index, CONST D3DLIGHT9 *pLight )
	{
		Dx9Device()->SetLight( Index, pLight );
	}

	FORCEINLINE void LightEnable( DWORD LightIndex, bool bEnable )
	{
		Dx9Device()->LightEnable( LightIndex, bEnable );
	}


	void ExecuteCommandBuffer( uint8 *pCmdBuffer );
	void SetStandardTextureHandle( StandardTextureId_t nId, ShaderAPITextureHandle_t );

	// Methods related to queuing functions to be called per-(pMesh->Draw call) or per-pass
	void ClearAllCommitFuncs( CommitFuncType_t func, CommitShaderType_t shader );
	void CallCommitFuncs( CommitFuncType_t func, CommitShaderType_t shader, bool bForce );
	bool IsCommitFuncInUse( CommitFuncType_t func, CommitShaderType_t shader, int nFunc ) const;
	void MarkCommitFuncInUse( CommitFuncType_t func, CommitShaderType_t shader, int nFunc );
	void AddCommitFunc( CommitFuncType_t func, CommitShaderType_t shader, StateCommitFunc_t f );
	void CallCommitFuncs( CommitFuncType_t func, bool bUsingFixedFunction, bool bForce = false );

	// Commits transforms and lighting
	void CommitStateChanges();

	// Commits transforms that have to be dealt with on a per pass basis (ie. projection matrix for polyoffset)
	void CommitPerPassStateChanges( StateSnapshot_t id );

	// Need to handle fog mode on a per-pass basis
	void CommitPerPassFogMode( bool bUsingVertexAndPixelShaders );

	// Commits user clip planes
	void CommitUserClipPlanes( bool bUsingFixedFunction );

	// Gets the user clip transform (world->view)
	DirectX::XMMATRIX XM_CALLCONV GetUserClipTransform( );

	// transform commit
	bool VertexShaderTransformChanged( int i );
	bool FixedFunctionTransformChanged( int i );

	void UpdateVertexShaderMatrix( int iMatrix );
	void SetVertexShaderStateSkinningMatrices();
	void CommitVertexShaderTransforms();
	void CommitPerPassVertexShaderTransforms();

	void UpdateFixedFunctionMatrix( int iMatrix );
	void SetFixedFunctionStateSkinningMatrices();
	void CommitFixedFunctionTransforms();
	void CommitPerPassFixedFunctionTransforms();

	// Recomputes the fast-clip plane matrices based on the current fast-clip plane
	void CommitFastClipPlane( );

	// Computes a matrix which includes the poly offset given an initial projection matrix
	void XM_CALLCONV ComputePolyOffsetMatrix( DirectX::FXMMATRIX matProjection, DirectX::XMMATRIX& matProjectionOffset );

	void SetSkinningMatrices();
	
	// lighting commit
	bool VertexShaderLightingChanged( int i );
	bool VertexShaderLightingEnableChanged( int i );
	bool FixedFunctionLightingChanged( int i );
	bool FixedFunctionLightingEnableChanged( int i );
	VertexShaderLightTypes_t ComputeLightType( int i ) const;
	void SortLights( int* index );
	void CommitVertexShaderLighting();
	void CommitPixelShaderLighting( int pshReg );
	void CommitFixedFunctionLighting();

	// Gets the surface associated with a texture (refcount of surface is increased)
	se::win::com::com_ptr<IDirect3DSurface9> GetTextureSurface( ShaderAPITextureHandle_t textureHandle );
	se::win::com::com_ptr<IDirect3DSurface9> GetDepthTextureSurface( ShaderAPITextureHandle_t textureHandle );

	//
	// Methods related to hardware config
	//
	void SetDefaultConfigValuesForDxLevel( int dxLevelFromCaps, ShaderDeviceInfo_t &info, unsigned int nFlagsUsed );

	// Determines hardware capabilities
	bool DetermineHardwareCaps( );

	// Alpha To Coverage entrypoints and states - much of this involves vendor-dependent paths and states...
	bool CheckVendorDependentAlphaToCoverage();
	void EnableAlphaToCoverage();
	void DisableAlphaToCoverage();

	// Vendor-dependent shadow mapping detection
	void CheckVendorDependentShadowMappingSupport( bool &bSupportsShadowDepthTextures, bool &bSupportsFetch4 );

	// Override caps based on a requested dx level
	void OverrideCaps( int nForcedDXLevel );

	// Reports support for a given MSAA mode
	bool SupportsMSAAMode( int nMSAAMode );

	// Reports support for a given CSAA mode
	bool SupportsCSAAMode( int nNumSamples, int nQualityLevel );

	// Gamma correction of fog color, or not...
	D3DCOLOR ComputeGammaCorrectedFogColor( unsigned char r, unsigned char g, unsigned char b, bool bSRGBWritesEnabled );

	void SetDefaultMaterial();

	bool RestorePersistedDisplay( bool bUseFrontBuffer );

	void ClearStdTextureHandles( void );

	// debug logging
	void PrintfVA( char *fmt, va_list vargs );
	void Printf( PRINTF_FORMAT_STRING const char *fmt, ... );
	float Knob( char *knobname, float *setvalue = NULL );

	// "normal" back buffer and depth buffer.  Need to keep this around so that we
	// know what to set the render target to when we are done rendering to a texture.
	se::win::com::com_ptr<IDirect3DSurface9> m_pBackBufferSurface;
	se::win::com::com_ptr<IDirect3DSurface9> m_pZBufferSurface;

	// Optimization for screenshots
	se::win::com::com_ptr<IDirect3DSurface9> m_pSmallBackBufferFP16TempSurface;

	ShaderAPITextureHandle_t m_hFullScreenTexture;

	ShaderAPITextureHandle_t m_hLinearToGammaTableTexture;
	ShaderAPITextureHandle_t m_hLinearToGammaTableIdentityTexture;

	//
	// State needed at the time of rendering (after snapshots have been applied)
	//

	// Interface for the DirectX::MatrixStack
	DirectX::MatrixStack*	m_pMatrixStack[NUM_MATRIX_MODES];
	matrix3x4_t			m_boneMatrix[NUM_MODEL_TRANSFORMS];
	int					m_maxBoneLoaded;

	// Current matrix mode
	D3DTRANSFORMSTATETYPE m_MatrixMode;
	int m_CurrStack;

	// The current camera position in world space.
	Vector4D m_WorldSpaceCameraPositon;

	// The current projection matrix with polyoffset baked into it.
	DirectX::XMMATRIX		m_CachedPolyOffsetProjectionMatrix;
	DirectX::XMMATRIX		m_CachedFastClipProjectionMatrix;
	DirectX::XMMATRIX		m_CachedFastClipPolyOffsetProjectionMatrix;

	// The texture stage state that changes frequently
	DynamicState_t	m_DynamicState;

	// The *desired* dynamic state. Most dynamic state is committed into actual hardware state
	// at either per-pass or per-material time.	This can also be used to force the hardware
	// to match the desired state after returning from alt-tab.
	DynamicState_t	m_DesiredState;

	// A list of state commit functions to run as per-draw call commit time
	unsigned char m_pCommitFlags[COMMIT_FUNC_TYPE_COUNT][COMMIT_SHADER_TYPE_COUNT][ COMMIT_FUNC_BYTE_COUNT ];
	CUtlVector< StateCommitFunc_t >	m_CommitFuncs[COMMIT_FUNC_TYPE_COUNT][COMMIT_SHADER_TYPE_COUNT];

	// Render data
	CMeshBase *m_pRenderMesh;
	int m_nDynamicVBSize;
	IMaterialInternal *m_pMaterial;

	// Shadow depth bias states
	float m_fShadowSlopeScaleDepthBias;
	float m_fShadowDepthBias;

	bool		m_bReadPixelsEnabled;

	// Render-to-texture stuff...
	bool		m_UsingTextureRenderTarget;

	int			m_ViewportMaxWidth;
	int			m_ViewportMaxHeight;

	// Ambient cube map ok?
	int m_CachedAmbientLightCube;

	// The current frame
	int m_CurrentFrame;

	// The texture we're currently modifying
	ShaderAPITextureHandle_t m_ModifyTextureHandle;
	char m_ModifyTextureLockedLevel;
	unsigned char m_ModifyTextureLockedFace;

	// Stores all textures
	CUtlFixedLinkedList< Texture_t >	m_Textures;

	// Mesh builder used to modify vertex data
	CMeshBuilder m_ModifyBuilder;

	float			m_VertexShaderFogParams[2];
	float			m_flFogMaxDensity;

	// Shadow state transition table
	CTransitionTable m_TransitionTable;
	StateSnapshot_t m_nCurrentSnapshot;

	// Depth test override...
	bool	m_bOverrideMaterialIgnoreZ;
	bool 	m_bIgnoreZValue;

	// Are we in the middle of resetting the render state?
	bool	m_bResettingRenderState;

	// Can we buffer 2 frames ahead?
	bool	m_bBuffer2FramesAhead;

	// Selection name stack
	CUtlStack< int >	m_SelectionNames;
	bool				m_InSelectionMode;
	unsigned int*		m_pSelectionBufferEnd;
	unsigned int*		m_pSelectionBuffer;
	unsigned int*		m_pCurrSelectionRecord;
	float				m_SelectionMinZ;
	float				m_SelectionMaxZ;
	int					m_NumHits;

	// fog
	unsigned char m_SceneFogColor[3];
	MaterialFogMode_t m_SceneFogMode;

	// Tone Mapping state ( w is gamma scale )
	Vector4D m_ToneMappingScale;

	Deformation_t m_DeformationStack[DEFORMATION_STACK_DEPTH];

	Deformation_t *m_pDeformationStackPtr;

	// rendering parameter storage
	int IntRenderingParameters[MAX_INT_RENDER_PARMS];
	float FloatRenderingParameters[MAX_FLOAT_RENDER_PARMS];
	Vector VectorRenderingParameters[MAX_VECTOR_RENDER_PARMS];

	ShaderAPITextureHandle_t m_StdTextureHandles[TEXTURE_MAX_STD_TEXTURES];
	
	// PIX instrumentation utlities...enable these with PIX_INSTRUMENTATION
	void StartPIXInstrumentation();
	void EndPIXInstrumentation();
	void SetPIXMarker( unsigned long color, const char *szName );
	void IncrementPIXError();
	bool PIXError();
	int m_nPIXErrorCount;
	int m_nPixFrame;
	bool m_bPixCapturing;

	void ComputeVertexDescription( unsigned char* pBuffer, VertexFormat_t vertexFormat, MeshDesc_t& desc ) const
	{
		return MeshMgr()->ComputeVertexDescription( pBuffer, vertexFormat, desc );
	}

	// Reports support for shadow depth texturing
	bool SupportsShadowDepthTextures( void );
	bool SupportsFetch4( void );

	void SetShadowDepthBiasFactors( float fShadowSlopeScaleDepthBias, float fShadowDepthBias );

	// Vendor-dependent depth stencil texture format
	ImageFormat GetShadowDepthTextureFormat( void );

	bool SupportsBorderColor( void ) const;
	bool SupportsFetch4( void ) const;
	
	void EnableBuffer2FramesAhead( bool bEnable );

	void SetDepthFeatheringPixelShaderConstant( int iConstant, float fDepthBlendScale );

	void SetDisallowAccess( bool b )
	{
		g_bShaderAccessDisallowed = b;
	}

	void EnableShaderShaderMutex( bool b )
	{
		Assert( g_ShaderMutex.GetOwnerId() == 0 );
		g_bUseShaderMutex = b;
	}

	void ShaderLock()
	{
		g_ShaderMutex.Lock();
	}

	void ShaderUnlock()
	{
		g_ShaderMutex.Unlock();
	}

	// Vendor-dependent slim texture format
	ImageFormat GetNullTextureFormat( void );

	//The idea behind a delayed constant is this.
	// Some shaders set constants based on rendering states, and some rendering states aren't updated until after a shader's already called Draw().
	//  So, for some functions that are state based, we save the constant we set and if the state changes between when it's set in the shader setup code 
	//   and when the shader is drawn, we update that constant.
	struct DelayedConstants_t
	{
		int iPixelShaderFogParams;

		void Invalidate( void )
		{
			iPixelShaderFogParams = -1;
		}
		DelayedConstants_t( void ) { this->Invalidate(); }
	};
	DelayedConstants_t m_DelayedShaderConstants;

	int	m_MaxVectorVertexShaderConstant;	
	int	m_MaxBooleanVertexShaderConstant;
	int	m_MaxIntegerVertexShaderConstant;
	int	m_MaxVectorPixelShaderConstant;	
	int	m_MaxBooleanPixelShaderConstant;
	int	m_MaxIntegerPixelShaderConstant;

	bool m_bGPUOwned;
	bool m_bResetRenderStateNeeded;

#ifdef ENABLE_NULLREF_DEVICE_SUPPORT
	bool m_NullDevice;
#endif
};


//-----------------------------------------------------------------------------
// Class Factory
//-----------------------------------------------------------------------------
static CShaderAPIDx8 g_ShaderAPIDX8;
IShaderAPIDX8 *g_pShaderAPIDX8 = &g_ShaderAPIDX8;
CShaderDeviceDx8* g_pShaderDeviceDx8 = &g_ShaderAPIDX8;

// FIXME: Remove IShaderAPI + IShaderDevice; they change after SetMode
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderAPIDx8, IShaderAPI, 
				SHADERAPI_INTERFACE_VERSION, g_ShaderAPIDX8 )

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderAPIDx8, IShaderDevice, 
				SHADER_DEVICE_INTERFACE_VERSION, g_ShaderAPIDX8 )

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CShaderAPIDx8, IDebugTextureInfo, 
				DEBUG_TEXTURE_INFO_VERSION, g_ShaderAPIDX8 )

//-----------------------------------------------------------------------------
// Accessors for major interfaces
//-----------------------------------------------------------------------------

#if defined( PIX_INSTRUMENTATION ) || defined( NVPERFHUD )
// Pix wants a max of 32 characters
// We'll give it the right-most substrings separated by slashes
static char s_pPIXMaterialName[32];
static void PIXifyName( char *pDst, int destSize, const char *pSrc )
{
	char *pSrcWalk = (char *)pSrc;

	while ( V_strlen( pSrcWalk ) > 31 )						// While we still have too many characters
	{
		char *pTok = strpbrk( pSrcWalk, "/\\" );			// Find next token

		if ( pTok )
			pSrcWalk = pTok + 1;
		else
			break;
	}

	V_strncpy( pDst, pSrcWalk, min( 32, destSize ) );
}
#endif

static int AdjustUpdateRange( float const* pVec, void const *pOut, int numVecs, int* pSkip )
{
	int skip = 0;
	const uint32* pSrc = (const uint32*)pVec;
	uint32* pDst = (uint32*)pOut;
	while( numVecs && !( ( pSrc[0] ^ pDst[0] ) | ( pSrc[1] ^ pDst[1] ) | ( pSrc[2] ^ pDst[2] ) | ( pSrc[3] ^ pDst[3] ) ) )
	{
		pSrc += 4;
		pDst += 4;
		numVecs--;
		skip++;
	}
	*pSkip = skip;
	if ( !numVecs )
		return 0;

	const uint32* pSrcLast = pSrc + numVecs * 4 - 4;
	uint32* pDstLast = pDst + numVecs * 4 - 4;
	while( numVecs > 1 && !( ( pSrcLast[0] ^ pDstLast[0] ) | ( pSrcLast[1] ^ pDstLast[1] ) | ( pSrcLast[2] ^ pDstLast[2] ) | ( pSrcLast[3] ^ pDstLast[3] ) ) )
	{
		pSrcLast -= 4;
		pDstLast -= 4;
		numVecs--;
	}

	return numVecs;
}

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CShaderAPIDx8::CShaderAPIDx8() :
	m_hFullScreenTexture( INVALID_SHADERAPI_TEXTURE_HANDLE ),
	m_hLinearToGammaTableTexture( INVALID_SHADERAPI_TEXTURE_HANDLE ),
	m_hLinearToGammaTableIdentityTexture( INVALID_SHADERAPI_TEXTURE_HANDLE ),
	m_CurrStack( -1 ),
	m_pRenderMesh( 0 ),
	m_nDynamicVBSize( DYNAMIC_VERTEX_BUFFER_MEMORY ),
	m_pMaterial( NULL ),
	m_fShadowSlopeScaleDepthBias( 16.0f ),
	m_fShadowDepthBias( 0.00008f ),
	m_bReadPixelsEnabled( false ),
	m_CachedAmbientLightCube( STATE_CHANGED ),
	m_CurrentFrame( 0 ),
	m_ModifyTextureHandle( INVALID_SHADERAPI_TEXTURE_HANDLE ),
	m_ModifyTextureLockedLevel( -1 ),
	m_Textures( 32 ),
	m_bResettingRenderState( false ),
	m_InSelectionMode(false),
	m_pSelectionBufferEnd( 0 ),
	m_pSelectionBuffer( 0 ),
	m_SelectionMinZ( FLT_MAX ),
	m_SelectionMaxZ( FLT_MIN ),
	m_ToneMappingScale( 1.0f, 1.0f, 1.0f, 1.0f ),
	m_nPIXErrorCount(0),
	m_nPixFrame(0),
	m_bPixCapturing( false ),
	m_bResetRenderStateNeeded( false )
{
	// FIXME: Remove! Backward compat
	m_bAdapterSet = false;
	m_bBuffer2FramesAhead = false;
	m_bReadPixelsEnabled = true;

	memset( m_pMatrixStack, 0, sizeof(m_pMatrixStack) );
	memset( &m_DynamicState, 0, sizeof(m_DynamicState) );
	//m_DynamicState.m_HeightClipMode = MATERIAL_HEIGHTCLIPMODE_DISABLE;
	m_nWindowHeight = m_nWindowWidth = 0;
	m_maxBoneLoaded = 0;

	m_bEnableDebugTextureList = 0;
	m_bDebugTexturesRendering = 0;
	m_pDebugTextureList = NULL;
	m_nTextureMemoryUsedLastFrame = 0;
	m_nTextureMemoryUsedTotal = 0;
	m_nTextureMemoryUsedPicMip1 = 0;
	m_nTextureMemoryUsedPicMip2 = 0;
	m_nDebugDataExportFrame = 0;

	m_SceneFogColor[0] = 0;
	m_SceneFogColor[1] = 0;
	m_SceneFogColor[2] = 0;
	m_SceneFogMode = MATERIAL_FOG_NONE;

	// We haven't yet detected whether we support CreateQuery or not yet.
	memset(IntRenderingParameters,0,sizeof(IntRenderingParameters));
	memset(FloatRenderingParameters,0,sizeof(FloatRenderingParameters));
	memset(VectorRenderingParameters,0,sizeof(VectorRenderingParameters));

	m_pDeformationStackPtr = m_DeformationStack + DEFORMATION_STACK_DEPTH;

	m_bGPUOwned = false;
	m_MaxVectorVertexShaderConstant = 0;
	m_MaxBooleanVertexShaderConstant = 0;
	m_MaxIntegerVertexShaderConstant = 0;
	m_MaxVectorPixelShaderConstant = 0;
	m_MaxBooleanPixelShaderConstant = 0;
	m_MaxIntegerPixelShaderConstant = 0;

	ClearStdTextureHandles();
	
	//Debugger();
#ifdef ENABLE_NULLREF_DEVICE_SUPPORT
	m_NullDevice = !!CommandLine()->FindParm( "-nulldevice" );
#endif
}

CShaderAPIDx8::~CShaderAPIDx8()
{
	delete[] m_DynamicState.m_pVectorVertexShaderConstant;
	m_DynamicState.m_pVectorVertexShaderConstant = NULL;

	delete[] m_DynamicState.m_pBooleanVertexShaderConstant;
	m_DynamicState.m_pBooleanVertexShaderConstant = NULL;

	delete[] m_DynamicState.m_pIntegerVertexShaderConstant;
	m_DynamicState.m_pIntegerVertexShaderConstant = NULL;

	delete[] m_DynamicState.m_pVectorPixelShaderConstant;
	m_DynamicState.m_pVectorPixelShaderConstant = NULL;

	delete[] m_DynamicState.m_pBooleanPixelShaderConstant;
	m_DynamicState.m_pBooleanPixelShaderConstant = NULL;

	delete[] m_DynamicState.m_pIntegerPixelShaderConstant;
	m_DynamicState.m_pIntegerPixelShaderConstant = NULL;

	if ( m_pDebugTextureList )
	{
		m_pDebugTextureList->deleteThis();
		m_pDebugTextureList = NULL;
	}
}


void CShaderAPIDx8::ClearStdTextureHandles( void )
{
	for( auto &&h : m_StdTextureHandles )
		h = INVALID_SHADERAPI_TEXTURE_HANDLE;
}


//-----------------------------------------------------------------------------
// FIXME: Remove! Backward compat.
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::OnAdapterSet()
{
	if ( !DetermineHardwareCaps( ) )
		return false;

	// Modify the caps based on requested DXlevels
	int nForcedDXLevel = CommandLine()->ParmValue( "-dxlevel", 0 );
	
	if ( nForcedDXLevel > 0 )
	{
		nForcedDXLevel = MAX( nForcedDXLevel, ABSOLUTE_MINIMUM_DXLEVEL );
	}
	

	// FIXME: Check g_pHardwareConfig->ActualCaps() for a preferred DX level
	OverrideCaps( nForcedDXLevel );

	m_bAdapterSet = true;
	return true;
}


void CShaderAPIDx8::GetDXLevelDefaults(uint &max_dxlevel,uint &recommended_dxlevel)
{
	max_dxlevel=g_pHardwareConfig->ActualCaps().m_nMaxDXSupportLevel;
	recommended_dxlevel=g_pHardwareConfig->ActualCaps().m_nDXSupportLevel;
}

//-----------------------------------------------------------------------------
// Can we download textures?
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::CanDownloadTextures() const
{
	if ( IsDeactivated() )
		return false;

	return IsActive();
}


//-----------------------------------------------------------------------------
// Grab the render targets
//-----------------------------------------------------------------------------
void CShaderAPIDx8::AcquireInternalRenderTargets()
{
	GLMPRINTF(( ">-A- CShaderAPIDx8::AcquireInternalRenderTargets... "));
	if ( mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: CShaderAPIDx8::AcquireInternalRenderTargets\n" );
	}

	if ( !m_pBackBufferSurface )
	{
		const HRESULT hr = Dx9Device()->GetRenderTarget( 0, &m_pBackBufferSurface );
		if ( FAILED(hr) )
		{
			Assert(false);
			Warning( __FUNCTION__ ": IDirect3DDevice9Ex::GetRenderTarget(index = 0) failed w/e %s.\n",
				se::win::com::com_error_category().message(hr).c_str() );
		}

#ifdef ENABLE_NULLREF_DEVICE_SUPPORT
		if( !m_NullDevice )
#endif
		{
			Assert( m_pBackBufferSurface );
		}
	}

#ifdef ENABLE_NULLREF_DEVICE_SUPPORT
	if ( !m_pZBufferSurface && !m_NullDevice )
#else
	if ( !m_pZBufferSurface )
#endif
	{
		const HRESULT hr = Dx9Device()->GetDepthStencilSurface( &m_pZBufferSurface );
		if ( FAILED(hr) )
		{
			Assert(false);
			Warning( __FUNCTION__ ": IDirect3DDevice9Ex::GetDepthStencilSurface() failed w/e %s.\n",
				se::win::com::com_error_category().message(hr).c_str() );
		}
		Assert( m_pZBufferSurface );
	}
	GLMPRINTF(( "<-A- CShaderAPIDx8::AcquireInternalRenderTargets...complete "));
}


//-----------------------------------------------------------------------------
// Release the render targets
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ReleaseInternalRenderTargets( )
{
	GLMPRINTF(( ">-A- CShaderAPIDx8::ReleaseInternalRenderTargets... "));
	if( mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: CShaderAPIDx8::ReleaseInternalRenderTargets\n" );
	}

	// Note: This function does not release renderable textures created elsewhere
	//       Those should be released separately via the texure manager
	if ( m_pBackBufferSurface )
	{
#if POSIX
		// dxabstract's AddRef/Release have optional args to help track usage
		[[maybe_unused]] ULONG nRetVal = m_pBackBufferSurface->Release( 0, "-B  CShaderAPIDx8::ReleaseInternalRenderTargets public release color buffer");
#else
		m_pBackBufferSurface.Release();
#endif
	}

	if ( m_pZBufferSurface )
	{
#if POSIX
		// dxabstract's AddRef/Release have optional args to help track usage
		[[maybe_unused]] ULONG nRetVal = m_pZBufferSurface->Release( 0, "-B  CShaderAPIDx8::ReleaseInternalRenderTargets public release zbuffer");
#else
		m_pZBufferSurface.Release();
#endif
	}
	GLMPRINTF(( "<-A- CShaderAPIDx8::ReleaseInternalRenderTargets... complete"));
}

//-----------------------------------------------------------------------------
// During init, places the persisted texture back into the back buffer.
// The initial 360 fixup present will then not flash. This routine has to be
// self contained, no other shader api systems are viable during init.
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::RestorePersistedDisplay( bool bUseFrontBuffer )
{
	return false;
}

//-----------------------------------------------------------------------------
// Initialize, shutdown the Device....
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::OnDeviceInit() 
{
	AcquireInternalRenderTargets();
	
	g_pHardwareConfig->CapsForEdit().m_TextureMemorySize = g_pShaderDeviceMgrDx8->GetVidMemBytes( m_nAdapter );

	CreateMatrixStacks();

	// Hide the cursor
	RECORD_COMMAND( DX8_SHOW_CURSOR, 1 );
	RECORD_INT( false );

	Dx9Device()->ShowCursor( false );

	// Initialize the shader manager
	ShaderManager()->Init();

	// Initialize the shader shadow
	ShaderShadow()->Init();

	// Initialize the mesh manager
	MeshMgr()->Init();

	const bool bToolsMode = IsWindows() && CommandLine()->CheckParm( "-tools" );

	// Use fat vertices when running in tools
	MeshMgr()->UseFatVertices( bToolsMode );

	// Initialize the transition table.
	m_TransitionTable.Init();

	// Initialize the render state
	InitRenderState();

	// Clear the z and color buffers
	ClearBuffers( true, true, true, -1, -1 );

	AllocFrameSyncObjects();

	RECORD_COMMAND( DX8_BEGIN_SCENE, 0 );
	const HRESULT hr = Dx9Device()->BeginScene();
	if (FAILED(hr))
	{
		Assert(false);
		Warning( __FUNCTION__ ": IDirect3DDevice9Ex::BeginScene failed w/e %s.\n",
			se::win::com::com_error_category().message(hr).c_str() );
	}

	return true;
}

void CShaderAPIDx8::OnDeviceShutdown() 
{
	if ( !IsActive() )
		return;

	// dimhotepus: Add EndScene() to pair OnDeviceInit as begin/end count must match.
	RECORD_COMMAND( DX8_END_SCENE, 0 );
	const HRESULT hr = Dx9Device()->EndScene();
	if (FAILED(hr))
	{
		Assert(false);
		Warning( __FUNCTION__ ": IDirect3DDevice9Ex::EndScene failed w/e %s.\n",
			se::win::com::com_error_category().message(hr).c_str() );
	}
	
	// Deallocate all textures
	DeleteAllTextures();

	ReleaseAllVertexDecl();
	
	// Free objects that are used for frame syncing.
	FreeFrameSyncObjects();

	// dimhotepus: deinit render state.
	ResetRenderState();

	// Clear the z and color buffers
	ClearBuffers( true, true, true, -1, -1 );

	// Shutdown the transition table.
	m_TransitionTable.Shutdown();

	MeshMgr()->Shutdown();

	ShaderManager()->Shutdown();
	
	RECORD_COMMAND( DX8_SHOW_CURSOR, 1 );
	RECORD_INT( true );

	// dimhotepus: Show cursor as it was hidden in OnDeviceInit 
	Dx9Device()->ShowCursor( true );

	FreeMatrixStacks();

	// Release render targets
	ReleaseInternalRenderTargets();
}


//-----------------------------------------------------------------------------
// Sets the mode...
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::SetMode( void* VD3DHWND, unsigned nAdapter, const ShaderDeviceInfo_t &info )
{
	//
	// FIXME: Note that this entire function is backward compat and will go soon
	//

	bool bRestoreNeeded = false;

	if ( IsActive() )
	{
		ReleaseResources();
		OnDeviceShutdown();
		ShutdownDevice();
		bRestoreNeeded = true;
	}

	LOCK_SHADERAPI();
	Assert( D3D() );
	Assert( nAdapter < g_pShaderDeviceMgr->GetAdapterCount() );

	const HardwareCaps_t& actualCaps = g_pShaderDeviceMgr->GetHardwareCaps( nAdapter );

	ShaderDeviceInfo_t actualInfo = info;
	int nDXLevel = actualInfo.m_nDXLevel ? actualInfo.m_nDXLevel : actualCaps.m_nDXSupportLevel;

	static bool bSetModeOnce = false;
	if ( !bSetModeOnce )
	{
		nDXLevel = MAX( ABSOLUTE_MINIMUM_DXLEVEL, CommandLine()->ParmValue( "-dxlevel", nDXLevel ) );
		bSetModeOnce = true;
	}
	if ( nDXLevel > actualCaps.m_nMaxDXSupportLevel )
	{
		nDXLevel = actualCaps.m_nMaxDXSupportLevel;
	}
	actualInfo.m_nDXLevel = g_pShaderDeviceMgr->GetClosestActualDXLevel( nDXLevel );

	if ( !g_pShaderDeviceMgrDx8->ValidateMode( nAdapter, actualInfo ) )
		return false;

	g_pShaderAPI = this;
	g_pShaderDevice = this;
	g_pShaderShadow = ShaderShadow();
	bool bOk = InitDevice( VD3DHWND, nAdapter, actualInfo );
	if ( !bOk )
		return false;

	if ( !OnDeviceInit() )
		return false;

	if ( bRestoreNeeded && IsPC() )
	{
		ReacquireResources();
	}

	return true;
}


//-----------------------------------------------------------------------------
// Creates the matrix stack
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CreateMatrixStacks()
{
	MEM_ALLOC_D3D_CREDIT();

	for (auto &s : m_pMatrixStack)
	{
		s = new DirectX::MatrixStack();
	}
}


//-----------------------------------------------------------------------------
// Frees the matrix stack
//-----------------------------------------------------------------------------
void CShaderAPIDx8::FreeMatrixStacks()
{
	for (auto &s : m_pMatrixStack)
	{
		delete s;
		s = nullptr;
	}
}


//-----------------------------------------------------------------------------
// Vendor-dependent code to turn on alpha to coverage
//-----------------------------------------------------------------------------
void CShaderAPIDx8::EnableAlphaToCoverage( void )
{
	if( !g_pHardwareConfig->ActualCaps().m_bSupportsAlphaToCoverage || !IsAAEnabled() )
		return;

	D3DRENDERSTATETYPE renderState = (D3DRENDERSTATETYPE)g_pHardwareConfig->Caps().m_AlphaToCoverageState;
	SetRenderState( renderState, g_pHardwareConfig->Caps().m_AlphaToCoverageEnableValue );	// Vendor dependent state
}

//-----------------------------------------------------------------------------
// Vendor-dependent code to turn off alpha to coverage
//-----------------------------------------------------------------------------
void CShaderAPIDx8::DisableAlphaToCoverage()
{
	if( !g_pHardwareConfig->ActualCaps().m_bSupportsAlphaToCoverage || !IsAAEnabled() )
		return;

	D3DRENDERSTATETYPE renderState = (D3DRENDERSTATETYPE)g_pHardwareConfig->Caps().m_AlphaToCoverageState;
	SetRenderState( renderState, g_pHardwareConfig->Caps().m_AlphaToCoverageDisableValue );	// Vendor dependent state
}

//-----------------------------------------------------------------------------
// Determine capabilities
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::DetermineHardwareCaps( )
{
	HardwareCaps_t& actualCaps = g_pHardwareConfig->ActualCapsForEdit();
	if ( !g_pShaderDeviceMgrDx8->ComputeCapsFromD3D( &actualCaps, m_DisplayAdapter ) )
		return false;

	// See if the file tells us otherwise
	g_pShaderDeviceMgrDx8->ReadDXSupportLevels( actualCaps );

	// Read dxsupport.cfg which has config overrides for particular cards.
	g_pShaderDeviceMgrDx8->ReadHardwareCaps( actualCaps, actualCaps.m_nMaxDXSupportLevel );

	// What's in "-shader" overrides dxsupport.cfg
	const char *pShaderParam = CommandLine()->ParmValue( "-shader" );
	if ( pShaderParam )
	{
		Q_strncpy( actualCaps.m_pShaderDLL, pShaderParam, sizeof( actualCaps.m_pShaderDLL ) );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Override caps based on a particular dx level
//-----------------------------------------------------------------------------
void CShaderAPIDx8::OverrideCaps( int nForcedDXLevel )
{
	// Just use the actual caps if we can't use what was requested or if the default is requested
	if ( nForcedDXLevel <= 0 ) 
	{
		nForcedDXLevel = g_pHardwareConfig->ActualCaps().m_nDXSupportLevel;
	}
	nForcedDXLevel = g_pShaderDeviceMgr->GetClosestActualDXLevel( nForcedDXLevel );

	g_pHardwareConfig->SetupHardwareCaps( nForcedDXLevel, g_pHardwareConfig->ActualCaps() );
}


//-----------------------------------------------------------------------------
// Called when the dx support level has changed
//-----------------------------------------------------------------------------
void CShaderAPIDx8::DXSupportLevelChanged()
{
	LOCK_SHADERAPI();
	OverrideCaps( ShaderUtil()->GetConfig().dxSupportLevel );
}


//-----------------------------------------------------------------------------
// FIXME: Remove! Backward compat only
//-----------------------------------------------------------------------------
int CShaderAPIDx8::GetActualTextureStageCount() const
{
	return g_pHardwareConfig->GetActualTextureStageCount();
}

int CShaderAPIDx8::GetActualSamplerCount() const
{
	return g_pHardwareConfig->GetActualSamplerCount();
}

int CShaderAPIDx8::StencilBufferBits() const
{
	return m_bUsingStencil ? m_iStencilBufferBits : 0;
}

bool CShaderAPIDx8::IsAAEnabled() const
{
	bool bAntialiasing = ( m_PresentParameters.MultiSampleType != D3DMULTISAMPLE_NONE );
	return bAntialiasing;
}


//-----------------------------------------------------------------------------
// Methods related to queuing functions to be called per-(pMesh->Draw call) or per-pass
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::IsCommitFuncInUse( CommitFuncType_t func, CommitShaderType_t shader, int nFunc ) const
{
	Assert( nFunc < COMMIT_FUNC_COUNT );
	return ( m_pCommitFlags[func][shader][ nFunc >> 3 ] & ( 1 << ( nFunc & 0x7 ) ) ) != 0;
}

void CShaderAPIDx8::MarkCommitFuncInUse( CommitFuncType_t func, CommitShaderType_t shader, int nFunc )
{
	m_pCommitFlags[func][shader][ nFunc >> 3 ] |= 1 << ( nFunc & 0x7 );
}

void CShaderAPIDx8::AddCommitFunc( CommitFuncType_t func, CommitShaderType_t shader, StateCommitFunc_t f )
{
	m_CommitFuncs[func][shader].AddToTail( f );
}


//-----------------------------------------------------------------------------
// Clears all commit functions
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ClearAllCommitFuncs( CommitFuncType_t func, CommitShaderType_t shader )
{
	memset( m_pCommitFlags[func][shader], 0, COMMIT_FUNC_BYTE_COUNT );
	m_CommitFuncs[func][shader].RemoveAll();
}


//-----------------------------------------------------------------------------
// Calls all commit functions in a particular list
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CallCommitFuncs( CommitFuncType_t func, CommitShaderType_t shader, bool bForce )
{
	// Don't bother committing anything if we're deactivated
	if ( IsDeactivated() )
		return;

	CUtlVector< StateCommitFunc_t > &funcList =	m_CommitFuncs[func][shader];
	intp nCount = funcList.Count();
	if ( nCount == 0 )
		return;

	for ( intp i = 0; i < nCount; ++i )
	{
		funcList[i]( Dx9Device(), m_DesiredState, m_DynamicState, bForce );
	}
	ClearAllCommitFuncs( func, shader );
}


//-----------------------------------------------------------------------------
// Calls all per-mesh draw commit functions
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CallCommitFuncs( CommitFuncType_t func, bool bUsingFixedFunction, bool bForce )
{
	// Fixed-Function only funcs
	if ( IsPC() && ( bUsingFixedFunction || bForce ) )
	{
		CallCommitFuncs( func, COMMIT_FIXED_FUNCTION, bForce );
	}

	// Vertex-shader only funcs
	if ( !bUsingFixedFunction || bForce )
	{
		CallCommitFuncs( func, COMMIT_VERTEX_SHADER, bForce );
	}

	// State set in both FF + VS
	CallCommitFuncs( func, COMMIT_ALWAYS, bForce );
}

//-----------------------------------------------------------------------------
// Sets the sampler state
//-----------------------------------------------------------------------------
static FORCEINLINE void SetSamplerState( IDirect3DDevice9 *pDevice, int stage, D3DSAMPLERSTATETYPE state, DWORD val )
{
	RECORD_SAMPLER_STATE( stage, state, val ); 

	pDevice->SetSamplerState( stage, state, val );
}

inline void CShaderAPIDx8::SetSamplerState( int stage, D3DSAMPLERSTATETYPE state, DWORD val )
{
#ifndef DX_TO_GL_ABSTRACTION
	if ( IsDeactivated() )
		return;
#endif

	::SetSamplerState( Dx9Device(), stage, state, val );
}

//-----------------------------------------------------------------------------
// Sets the texture stage state
//-----------------------------------------------------------------------------
inline void CShaderAPIDx8::SetTextureStageState( int stage, D3DTEXTURESTAGESTATETYPE state, DWORD val )
{
	if ( IsDeactivated() )
		return;

	Dx9Device()->SetTextureStageState( stage, state, val );
}

inline void CShaderAPIDx8::SetRenderState( D3DRENDERSTATETYPE state, DWORD val, bool bFlushIfChanged )
{
#if !defined( DX_TO_GL_ABSTRACTION )
	{
		if ( IsDeactivated() )
			return;
	}
#else
	{
		Assert( state != D3DRS_NOTSUPPORTED ); //Use SetSupportedRenderState() macro to avoid this at compile time
		//if ( state == D3DRS_NOTSUPPORTED )
		//	return;
	}
#endif

	Assert( state >= 0 && ( int )state < MAX_NUM_RENDERSTATES );
	if ( m_DynamicState.m_RenderState[state] != val )
	{
		if ( bFlushIfChanged )
		{
			FlushBufferedPrimitives();
		}
#ifdef DX_TO_GL_ABSTRACTION
		Dx9Device()->SetRenderStateInline( state, val );
#else
		Dx9Device()->SetRenderState( state, val );
#endif
		m_DynamicState.m_RenderState[state] = val;
	}
}

#ifdef DX_TO_GL_ABSTRACTION
	// Purposely always writing the new state (even if it's not changed), in case SetRenderStateConstInline() compiles away to nothing (it sometimes does)
	#define SetRenderStateConstMacro(t, state, val )						\
		do																	\
		{																	\
			Assert( state >= 0 && ( int )state < MAX_NUM_RENDERSTATES );	\
			if ( t->m_DynamicState.m_RenderState[state] != (DWORD)val )		\
			{																\
				Dx9Device()->SetRenderStateConstInline( state, val );		\
			}																\
			t->m_DynamicState.m_RenderState[state] = val;					\
		} while(0)
#else
	#define SetRenderStateConstMacro(t, state, val ) t->SetRenderState( state, val )
#endif

//-----------------------------------------------------------------------------
// Commits viewports
//-----------------------------------------------------------------------------
static void CommitSetScissorRect( IDirect3DDevice9 *pDevice, const DynamicState_t &desiredState, DynamicState_t &currentState, bool bForce )
{
	// Set the enable/disable renderstate

	bool bEnableChanged = desiredState.m_RenderState[D3DRS_SCISSORTESTENABLE] != currentState.m_RenderState[D3DRS_SCISSORTESTENABLE];
	if ( bEnableChanged )
	{
		Dx9Device()->SetRenderState( D3DRS_SCISSORTESTENABLE, desiredState.m_RenderState[D3DRS_SCISSORTESTENABLE] );
		currentState.m_RenderState[D3DRS_SCISSORTESTENABLE] = desiredState.m_RenderState[D3DRS_SCISSORTESTENABLE];
	}

	// Only bother with the rect if we're enabling
	if ( desiredState.m_RenderState[D3DRS_SCISSORTESTENABLE] )
	{
		int nWidth, nHeight;
		ITexture *pTexture = ShaderAPI()->GetRenderTargetEx( 0 );
		if ( pTexture == NULL )
		{
			ShaderAPI()->GetBackBufferDimensions( nWidth, nHeight );
		}
		else
		{
			nWidth  = pTexture->GetActualWidth();
			nHeight = pTexture->GetActualHeight();
		}

		Assert( (desiredState.m_ScissorRect.left <= nWidth) && (desiredState.m_ScissorRect.bottom <= nHeight) &&
			    ( desiredState.m_ScissorRect.top >= 0 ) && (desiredState.m_ScissorRect.left >= 0) );

		Dx9Device()->SetScissorRect( &desiredState.m_ScissorRect );
		currentState.m_ScissorRect = desiredState.m_ScissorRect;
	}
}

// Routine for setting scissor rect
// If pScissorRect is NULL, disable scissoring by setting the render state
// If pScissorRect is non-NULL, set the RECT state in Direct3D AND set the renderstate
inline void CShaderAPIDx8::SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor )
{
	Assert( (nLeft <= nRight) && (nTop <= nBottom) ); //360 craps itself if this isn't true
	if ( !g_pHardwareConfig->Caps().m_bScissorSupported )
		return;

	// If we're turning it on, check the validity of the rect
	if ( bEnableScissor )
	{
		int nWidth, nHeight;
		ITexture *pTexture = GetRenderTargetEx( 0 );
		if ( pTexture == NULL )
		{
			GetBackBufferDimensions( nWidth, nHeight );
		}
		else
		{
			nWidth  = pTexture->GetActualWidth();
			nHeight = pTexture->GetActualHeight();
		}

		Assert( (nRight <= nWidth) && (nBottom <= nHeight) && ( nTop >= 0 ) && (nLeft >= 0) );

		// dimhotepus: Clamp rect if needed.
		// nRight  = clamp( nRight,  0, nWidth );
		// nLeft   = clamp( nLeft,   0, nWidth );
		// nTop    = clamp( nTop,    0, nHeight );
		// nBottom = clamp( nBottom, 0, nHeight );
	}

	DWORD dwEnableScissor = bEnableScissor ? TRUE : FALSE;
	RECT newScissorRect;
	newScissorRect.left = nLeft;
	newScissorRect.top = nTop;
	newScissorRect.right = nRight;
	newScissorRect.bottom = nBottom;

	if ( !m_bResettingRenderState )
	{
		bool bEnableChanged = m_DesiredState.m_RenderState[D3DRS_SCISSORTESTENABLE] != dwEnableScissor;
		bool bRectChanged = memcmp( &newScissorRect, &m_DesiredState.m_ScissorRect, sizeof(RECT) ) != 0;

		if ( !bEnableChanged && !bRectChanged )
			return;
	}

	if ( !IsDeactivated() )
	{
		// Do we need to do this always?
		FlushBufferedPrimitives();
	}

	m_DesiredState.m_RenderState[D3DRS_SCISSORTESTENABLE] = dwEnableScissor;
	memcpy( &m_DesiredState.m_ScissorRect, &newScissorRect, sizeof(RECT) );

	ADD_COMMIT_FUNC( COMMIT_PER_DRAW, COMMIT_ALWAYS, CommitSetScissorRect );
}

inline void CShaderAPIDx8::SetRenderStateForce( D3DRENDERSTATETYPE state, DWORD val )
{
#if !defined( DX_TO_GL_ABSTRACTION )
	{
		if ( IsDeactivated() )
			return;
	}
#else
	{
		Assert( state != D3DRS_NOTSUPPORTED ); //Use SetSupportedRenderStateForce() macro to avoid this at compile time
		//if ( state == D3DRS_NOTSUPPORTED )
		//	return;
	}
#endif

	Dx9Device()->SetRenderState( state, val );
	m_DynamicState.m_RenderState[state] = val;
}


//-----------------------------------------------------------------------------
// Set the values for pixel shader constants that don't change.
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetStandardVertexShaderConstants( float fOverbright )
{
	LOCK_SHADERAPI();
	if ( g_pHardwareConfig->GetDXSupportLevel() < 80 )
		return;

	// Set a couple standard constants....
	Vector4D standardVertexShaderConstant( 0.0f, 1.0f, 2.0f, 0.5f );
	SetVertexShaderConstant( VERTEX_SHADER_MATH_CONSTANTS0, standardVertexShaderConstant.Base(), 1 );

	// [ gamma, overbright, 1/3, 1/overbright ]
	standardVertexShaderConstant.Init( 1.0f/2.2f, fOverbright, 1.0f / 3.0f, 1.0f / fOverbright );
	SetVertexShaderConstant( VERTEX_SHADER_MATH_CONSTANTS1, standardVertexShaderConstant.Base(), 1 );

	int nModelIndex = g_pHardwareConfig->Caps().m_nDXSupportLevel < 90 ? VERTEX_SHADER_MODEL - 10 : VERTEX_SHADER_MODEL;

/*
	if ( g_pHardwareConfig->Caps().m_SupportsVertexShaders_3_0 )
	{
		Vector4D factors[4];
		factors[0].Init( 1, 0, 0, 0 );
		factors[1].Init( 0, 1, 0, 0 );
		factors[2].Init( 0, 0, 1, 0 );
		factors[3].Init( 0, 0, 0, 1 );
		SetVertexShaderConstant( VERTEX_SHADER_DOT_PRODUCT_FACTORS, factors[0].Base(), 4 );
	}
*/

	if ( g_pHardwareConfig->Caps().m_SupportsVertexShaders_2_0 )
	{
		float c[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		SetVertexShaderConstant( VERTEX_SHADER_FLEXSCALE, c, 1 );
	}
	else
	{
		// These point to the lighting and the transforms
		standardVertexShaderConstant.Init( 
			VERTEX_SHADER_LIGHTS,
			VERTEX_SHADER_LIGHTS + 5, 
			// Use COLOR instead of UBYTE4 since Geforce3 does not support it
			// vConst.w should be 3, but due to about hack, mul by 255 and add epsilon
			765.01f,
			nModelIndex );	// DX8 has different constant packing

		SetVertexShaderConstant( VERTEX_SHADER_LIGHT_INDEX, standardVertexShaderConstant.Base(), 1 );
	}
}

//-----------------------------------------------------------------------------
// Initialize vertex and pixel shaders
//-----------------------------------------------------------------------------
void CShaderAPIDx8::InitVertexAndPixelShaders()
{
	// Allocate space for the pixel and vertex shader constants...
	if ( g_pHardwareConfig->Caps().m_SupportsPixelShaders )
	{
		// pixel shaders
		{
			delete[] m_DynamicState.m_pVectorPixelShaderConstant;
			m_DynamicState.m_pVectorPixelShaderConstant = new Vector4D[g_pHardwareConfig->Caps().m_NumPixelShaderConstants];

			delete[] m_DesiredState.m_pVectorPixelShaderConstant;
			m_DesiredState.m_pVectorPixelShaderConstant = new Vector4D[g_pHardwareConfig->Caps().m_NumPixelShaderConstants];

			delete[] m_DynamicState.m_pBooleanPixelShaderConstant;
			m_DynamicState.m_pBooleanPixelShaderConstant = new BOOL[g_pHardwareConfig->Caps().m_NumBooleanPixelShaderConstants];

			delete[] m_DesiredState.m_pBooleanPixelShaderConstant;
			m_DesiredState.m_pBooleanPixelShaderConstant = new BOOL[g_pHardwareConfig->Caps().m_NumBooleanPixelShaderConstants];
		
			delete[] m_DynamicState.m_pIntegerPixelShaderConstant;
			m_DynamicState.m_pIntegerPixelShaderConstant = new IntVector4D[g_pHardwareConfig->Caps().m_NumIntegerPixelShaderConstants];

			delete[] m_DesiredState.m_pIntegerPixelShaderConstant;
			m_DesiredState.m_pIntegerPixelShaderConstant = new IntVector4D[g_pHardwareConfig->Caps().m_NumIntegerPixelShaderConstants];

			// force reset vector pixel constants
			int i;
			for ( i = 0; i < g_pHardwareConfig->Caps().m_NumPixelShaderConstants; ++i )
			{
				m_DesiredState.m_pVectorPixelShaderConstant[i].Init();
			}
			SetPixelShaderConstant( 0, m_DesiredState.m_pVectorPixelShaderConstant[0].Base(), g_pHardwareConfig->Caps().m_NumPixelShaderConstants, true );

			// force reset boolean pixel constants
			int nNumBooleanPixelShaderConstants = g_pHardwareConfig->Caps().m_NumBooleanPixelShaderConstants;
			if ( nNumBooleanPixelShaderConstants )
			{
				for ( i = 0; i < nNumBooleanPixelShaderConstants; ++i )
				{
					m_DesiredState.m_pBooleanPixelShaderConstant[i] = 0;
				}
				SetBooleanPixelShaderConstant( 0, m_DesiredState.m_pBooleanPixelShaderConstant, nNumBooleanPixelShaderConstants, true );
			}

			// force reset integer pixel constants
			int nNumIntegerPixelShaderConstants = g_pHardwareConfig->Caps().m_NumIntegerPixelShaderConstants;
			if ( nNumIntegerPixelShaderConstants )
			{
				for ( i = 0; i < nNumIntegerPixelShaderConstants; ++i )
				{
					m_DesiredState.m_pIntegerPixelShaderConstant[i].Init();
				}
				SetIntegerPixelShaderConstant( 0, m_DesiredState.m_pIntegerPixelShaderConstant[0].Base(), nNumIntegerPixelShaderConstants, true );
			}
		}
		
		// vertex shaders
		{
			delete[] m_DynamicState.m_pVectorVertexShaderConstant;
			m_DynamicState.m_pVectorVertexShaderConstant = new Vector4D[g_pHardwareConfig->Caps().m_NumVertexShaderConstants];

			delete[] m_DesiredState.m_pVectorVertexShaderConstant;
			m_DesiredState.m_pVectorVertexShaderConstant = new Vector4D[g_pHardwareConfig->Caps().m_NumVertexShaderConstants];

			delete[] m_DynamicState.m_pBooleanVertexShaderConstant;
			m_DynamicState.m_pBooleanVertexShaderConstant = new BOOL[g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants];

			delete[] m_DesiredState.m_pBooleanVertexShaderConstant;
			m_DesiredState.m_pBooleanVertexShaderConstant = new BOOL[g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants];

			delete[] m_DynamicState.m_pIntegerVertexShaderConstant;
			m_DynamicState.m_pIntegerVertexShaderConstant = new IntVector4D[g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants];

			delete[] m_DesiredState.m_pIntegerVertexShaderConstant;
			m_DesiredState.m_pIntegerVertexShaderConstant = new IntVector4D[g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants];

			// force reset vector vertex constants
			int i;
			for ( i = 0; i < g_pHardwareConfig->Caps().m_NumVertexShaderConstants; ++i )
			{
				m_DesiredState.m_pVectorVertexShaderConstant[i].Init();
			}
			SetVertexShaderConstant( 0, m_DesiredState.m_pVectorVertexShaderConstant[0].Base(), g_pHardwareConfig->Caps().m_NumVertexShaderConstants, true );

			// force reset boolean vertex constants
			for ( i = 0; i < g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants; ++i )
			{
				m_DesiredState.m_pBooleanVertexShaderConstant[i] = 0;
			}
			SetBooleanVertexShaderConstant( 0, m_DesiredState.m_pBooleanVertexShaderConstant, g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants, true );

			// force reset integer vertex constants
			for ( i = 0; i < g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants; ++i )
			{
				m_DesiredState.m_pIntegerVertexShaderConstant[i].Init();
			}
			SetIntegerVertexShaderConstant( 0, m_DesiredState.m_pIntegerVertexShaderConstant[0].Base(), g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants, true );
		}

		SetStandardVertexShaderConstants( OVERBRIGHT );
	}

	// Set up the vertex and pixel shader stuff
	ShaderManager()->ResetShaderState();
}


//-----------------------------------------------------------------------------
// Initialize render state
//-----------------------------------------------------------------------------
void CShaderAPIDx8::InitRenderState()
{ 
	// Set the default shadow state
	g_pShaderShadowDx8->SetDefaultState();

	// Grab a snapshot of this state; we'll be using it to set the board
	// state to something well defined.
	m_TransitionTable.TakeDefaultStateSnapshot();

	if ( !IsDeactivated() )
	{
		ResetRenderState();
	}
}


//-----------------------------------------------------------------------------
// Commits vertex textures
//-----------------------------------------------------------------------------
static void CommitVertexTextures( IDirect3DDevice9 *pDevice, const DynamicState_t &desiredState, DynamicState_t &currentState, bool bForce )
{
	int nCount = g_pMaterialSystemHardwareConfig->GetVertexTextureCount();
	for ( int i = 0; i < nCount; ++i )
	{
		VertexTextureState_t &currentVTState = currentState.m_VertexTextureState[i];
		ShaderAPITextureHandle_t textureHandle = desiredState.m_VertexTextureState[i].m_BoundTexture;

		Texture_t *pTexture = ( textureHandle != INVALID_SHADERAPI_TEXTURE_HANDLE ) ? &g_ShaderAPIDX8.GetTexture( textureHandle ) : NULL;
		if ( pTexture && ( pTexture->m_Flags & Texture_t::IS_VERTEX_TEXTURE ) == 0 )
		{
			Warning( "Attempting to bind a vertex texture (%s) which was not created as a vertex texture!\n", pTexture->m_DebugName.String() );
		}

		if ( bForce || ( currentVTState.m_BoundTexture != textureHandle ) )
		{
			currentVTState.m_BoundTexture = textureHandle;

// 			RECORD_COMMAND( DX8_SET_VERTEXTEXTURE, 3 );
// 			RECORD_INT( D3DVERTEXTEXTURESAMPLER0 + nStage );
// 			RECORD_INT( pTexture ? pTexture->GetUniqueID() : 0xFFFF );
// 			RECORD_INT( 0 );

			IDirect3DBaseTexture *pD3DTexture = ( textureHandle >= 0 ) ? g_ShaderAPIDX8.GetD3DTexture( textureHandle ) : NULL;

			pDevice->SetTexture( D3DVERTEXTEXTURESAMPLER0 + i, pD3DTexture );
		}

		if ( !pTexture )
			continue;

		D3DTEXTUREADDRESS nNewUTexWrap = pTexture->m_UTexWrap;
		if ( bForce || ( currentVTState.m_UTexWrap != nNewUTexWrap ) )
		{
			currentVTState.m_UTexWrap = nNewUTexWrap;
			SetSamplerState( pDevice, D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_ADDRESSU, currentVTState.m_UTexWrap );
		}

		D3DTEXTUREADDRESS nNewVTexWrap = pTexture->m_VTexWrap;
		if ( bForce || ( currentVTState.m_VTexWrap != nNewVTexWrap ) )
		{
			currentVTState.m_VTexWrap = nNewVTexWrap;
			SetSamplerState( pDevice, D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_ADDRESSV, currentVTState.m_VTexWrap );
		}

		/*
		D3DTEXTUREADDRESS nNewWTexWrap = pTexture->GetWTexWrap();
		if ( bForce || ( currentVTState.m_WTexWrap != nNewWTexWrap ) )
		{
			currentVTState.m_WTexWrap = nNewWTexWrap;
			SetSamplerState( pDevice, D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_ADDRESSW, currentVTState.m_WTexWrap );
		}
		*/

		D3DTEXTUREFILTERTYPE nNewMinFilter = pTexture->m_MinFilter;
		if ( bForce || ( currentVTState.m_MinFilter != nNewMinFilter ) )
		{
			currentVTState.m_MinFilter = nNewMinFilter;
			SetSamplerState( pDevice, D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_MINFILTER, currentVTState.m_MinFilter );
		}

		D3DTEXTUREFILTERTYPE nNewMagFilter = pTexture->m_MagFilter;
		if ( bForce || ( currentVTState.m_MagFilter != nNewMagFilter ) )
		{
			currentVTState.m_MagFilter = nNewMagFilter;
			SetSamplerState( pDevice, D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_MAGFILTER, currentVTState.m_MagFilter );
		}

		D3DTEXTUREFILTERTYPE nNewMipFilter = pTexture->m_MipFilter;
		if ( bForce || ( currentVTState.m_MipFilter != nNewMipFilter ) )
		{
			currentVTState.m_MipFilter = nNewMipFilter;
			SetSamplerState( pDevice, D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_MIPFILTER, currentVTState.m_MipFilter );
		}
	}
}


//-----------------------------------------------------------------------------
// Returns true if the state snapshot is translucent
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::IsTranslucent( StateSnapshot_t id ) const
{
	LOCK_SHADERAPI();
	return m_TransitionTable.GetSnapshot(id).m_AlphaBlendEnable;
}


//-----------------------------------------------------------------------------
// Returns true if the state snapshot is alpha tested
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::IsAlphaTested( StateSnapshot_t id ) const
{
	LOCK_SHADERAPI();
	return m_TransitionTable.GetSnapshot(id).m_AlphaTestEnable;
}


//-----------------------------------------------------------------------------
// Gets at the shadow state for a particular state snapshot
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::IsDepthWriteEnabled( StateSnapshot_t id ) const
{
	LOCK_SHADERAPI();
	return m_TransitionTable.GetSnapshot(id).m_ZWriteEnable;
}



//-----------------------------------------------------------------------------
// Returns true if the state snapshot uses shaders
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::UsesVertexAndPixelShaders( StateSnapshot_t id ) const
{
	LOCK_SHADERAPI();
	return m_TransitionTable.GetSnapshotShader(id).m_VertexShader != INVALID_SHADER;
}


//-----------------------------------------------------------------------------
// Takes a snapshot
//-----------------------------------------------------------------------------
StateSnapshot_t CShaderAPIDx8::TakeSnapshot( )
{
	LOCK_SHADERAPI();

	return m_TransitionTable.TakeSnapshot();
}

void CShaderAPIDx8::ResetDXRenderState( void )
{
	float zero = 0.0f;
	float one = 1.0f;
	DWORD dZero = *((DWORD*)(&zero));
	DWORD dOne = *((DWORD*)(&one));
	
	// Set default values for all dx render states.
	// NOTE: this does not include states encapsulated by the transition table,
	//       which are reset in CTransitionTable::UseDefaultState
    SetSupportedRenderStateForce( D3DRS_FILLMODE, D3DFILL_SOLID );
    SetSupportedRenderStateForce( D3DRS_SHADEMODE, D3DSHADE_GOURAUD );
    SetSupportedRenderStateForce( D3DRS_LASTPIXEL, TRUE );
    SetSupportedRenderStateForce( D3DRS_CULLMODE, D3DCULL_CCW );
    SetSupportedRenderStateForce( D3DRS_DITHERENABLE, FALSE );
    SetSupportedRenderStateForce( D3DRS_FOGENABLE, FALSE );
    SetSupportedRenderStateForce( D3DRS_SPECULARENABLE, FALSE );
    SetSupportedRenderStateForce( D3DRS_FOGCOLOR, 0 );
    SetSupportedRenderStateForce( D3DRS_FOGTABLEMODE, D3DFOG_NONE );
    SetSupportedRenderStateForce( D3DRS_FOGSTART, dZero );
    SetSupportedRenderStateForce( D3DRS_FOGEND, dOne );
    SetSupportedRenderStateForce( D3DRS_FOGDENSITY, dZero );
    SetSupportedRenderStateForce( D3DRS_RANGEFOGENABLE, FALSE );
    SetSupportedRenderStateForce( D3DRS_STENCILENABLE, FALSE);
    SetSupportedRenderStateForce( D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP );
    SetSupportedRenderStateForce( D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP );
    SetSupportedRenderStateForce( D3DRS_STENCILPASS, D3DSTENCILOP_KEEP );
    SetSupportedRenderStateForce( D3DRS_STENCILFUNC, D3DCMP_ALWAYS );
    SetSupportedRenderStateForce( D3DRS_STENCILREF, 0 );
    SetSupportedRenderStateForce( D3DRS_STENCILMASK, 0xFFFFFFFF );
    SetSupportedRenderStateForce( D3DRS_STENCILWRITEMASK, 0xFFFFFFFF );
    SetSupportedRenderStateForce( D3DRS_TEXTUREFACTOR, 0xFFFFFFFF );
    SetSupportedRenderStateForce( D3DRS_WRAP0, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP1, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP2, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP3, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP4, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP5, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP6, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP7, 0 );
    SetSupportedRenderStateForce( D3DRS_CLIPPING, TRUE );
    SetSupportedRenderStateForce( D3DRS_LIGHTING, TRUE );
    SetSupportedRenderStateForce( D3DRS_AMBIENT, 0 );
    SetSupportedRenderStateForce( D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
    SetSupportedRenderStateForce( D3DRS_COLORVERTEX, TRUE );
    SetSupportedRenderStateForce( D3DRS_LOCALVIEWER, TRUE );
    SetSupportedRenderStateForce( D3DRS_NORMALIZENORMALS, FALSE );
    SetSupportedRenderStateForce( D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR2 );
    SetSupportedRenderStateForce( D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL );
    SetSupportedRenderStateForce( D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL );
    SetSupportedRenderStateForce( D3DRS_VERTEXBLEND, D3DVBF_DISABLE );
    SetSupportedRenderStateForce( D3DRS_CLIPPLANEENABLE, 0 );

	// -disable_d3d9_hacks is for debugging. For example, the "CENT" driver hack thing causes the flashlight pass to appear much brighter on NVidia drivers.
	if ( IsPC() && !IsOpenGL() && !CommandLine()->CheckParm( "-disable_d3d9_hacks" ) )
	{	
		if ( g_pHardwareConfig->Caps().m_bNeedsATICentroidHack && ( g_pHardwareConfig->Caps().m_VendorID == VENDORID_ATI ) )
		{
			SetSupportedRenderStateForce( D3DRS_POINTSIZE, MAKEFOURCC( 'C', 'E', 'N', 'T' ) );
		}

		if( g_pHardwareConfig->Caps().m_bDisableShaderOptimizations )
		{
			SetSupportedRenderStateForce( D3DRS_ADAPTIVETESS_Y, MAKEFOURCC( 'C', 'O', 'P', 'M' ) );
		}
	}
	SetSupportedRenderStateForce( D3DRS_POINTSIZE, dOne );
    SetSupportedRenderStateForce( D3DRS_POINTSIZE_MIN, dOne );
    SetSupportedRenderStateForce( D3DRS_POINTSPRITEENABLE, FALSE );
    SetSupportedRenderStateForce( D3DRS_POINTSCALEENABLE, FALSE );
    SetSupportedRenderStateForce( D3DRS_POINTSCALE_A, dOne );
    SetSupportedRenderStateForce( D3DRS_POINTSCALE_B, dZero );
    SetSupportedRenderStateForce( D3DRS_POINTSCALE_C, dZero );
    SetSupportedRenderStateForce( D3DRS_MULTISAMPLEANTIALIAS, TRUE );
    SetSupportedRenderStateForce( D3DRS_MULTISAMPLEMASK, 0xFFFFFFFF );
    SetSupportedRenderStateForce( D3DRS_PATCHEDGESTYLE, D3DPATCHEDGE_DISCRETE );
    SetSupportedRenderStateForce( D3DRS_DEBUGMONITORTOKEN, D3DDMT_ENABLE );
	float sixtyFour = 64.0f;
    SetSupportedRenderStateForce( D3DRS_POINTSIZE_MAX, *((DWORD*)(&sixtyFour)));
    SetSupportedRenderStateForce( D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE );
    SetSupportedRenderStateForce( D3DRS_TWEENFACTOR, dZero );
    SetSupportedRenderStateForce( D3DRS_POSITIONDEGREE, D3DDEGREE_CUBIC );
    SetSupportedRenderStateForce( D3DRS_NORMALDEGREE, D3DDEGREE_LINEAR );
    SetSupportedRenderStateForce( D3DRS_SCISSORTESTENABLE, FALSE);
    SetSupportedRenderStateForce( D3DRS_SLOPESCALEDEPTHBIAS, dZero );
    SetSupportedRenderStateForce( D3DRS_ANTIALIASEDLINEENABLE, FALSE );
    SetSupportedRenderStateForce( D3DRS_MINTESSELLATIONLEVEL, dOne );
    SetSupportedRenderStateForce( D3DRS_MAXTESSELLATIONLEVEL, dOne );
    SetSupportedRenderStateForce( D3DRS_ADAPTIVETESS_X, dZero );
    SetSupportedRenderStateForce( D3DRS_ADAPTIVETESS_Y, dZero );
    SetSupportedRenderStateForce( D3DRS_ADAPTIVETESS_Z, dOne );
    SetSupportedRenderStateForce( D3DRS_ADAPTIVETESS_W, dZero );
    SetSupportedRenderStateForce( D3DRS_ENABLEADAPTIVETESSELLATION, FALSE );
    SetSupportedRenderStateForce( D3DRS_TWOSIDEDSTENCILMODE, FALSE );
    SetSupportedRenderStateForce( D3DRS_CCW_STENCILFAIL, D3DSTENCILOP_KEEP );
    SetSupportedRenderStateForce( D3DRS_CCW_STENCILZFAIL, D3DSTENCILOP_KEEP );
    SetSupportedRenderStateForce( D3DRS_CCW_STENCILPASS, D3DSTENCILOP_KEEP );
    SetSupportedRenderStateForce( D3DRS_CCW_STENCILFUNC, D3DCMP_ALWAYS );
    SetSupportedRenderStateForce( D3DRS_COLORWRITEENABLE1, 0x0000000f );
    SetSupportedRenderStateForce( D3DRS_COLORWRITEENABLE2, 0x0000000f );
    SetSupportedRenderStateForce( D3DRS_COLORWRITEENABLE3, 0x0000000f );
    SetSupportedRenderStateForce( D3DRS_BLENDFACTOR, 0xffffffff );
    SetSupportedRenderStateForce( D3DRS_SRGBWRITEENABLE, 0);
    SetSupportedRenderStateForce( D3DRS_DEPTHBIAS, dZero );
    SetSupportedRenderStateForce( D3DRS_WRAP8, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP9, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP10, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP11, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP12, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP13, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP14, 0 );
    SetSupportedRenderStateForce( D3DRS_WRAP15, 0 );
    SetSupportedRenderStateForce( D3DRS_BLENDOP, D3DBLENDOP_ADD );
    SetSupportedRenderStateForce( D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD );
}

//-----------------------------------------------------------------------------
// Own GPU Resources.  Return previous state.
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::OwnGPUResources( bool bEnable )
{
	return false;
}

//-----------------------------------------------------------------------------
// Reset render state (to its initial value)
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ResetRenderState( bool bFullReset )
{
	LOCK_SHADERAPI();
	RECORD_DEBUG_STRING( "BEGIN ResetRenderState" );

	if ( !CanDownloadTextures() )
	{
		QueueResetRenderState();
		return;
	}

	m_bResettingRenderState = true;

	ResetDXRenderState();

	// We're not currently rendering anything
	m_nCurrentSnapshot = -1;

	m_CachedPolyOffsetProjectionMatrix = DirectX::XMMatrixIdentity();
	m_CachedFastClipProjectionMatrix = DirectX::XMMatrixIdentity();
	m_CachedFastClipPolyOffsetProjectionMatrix = DirectX::XMMatrixIdentity();
	m_UsingTextureRenderTarget = false;

	m_SceneFogColor[0] = 0;
	m_SceneFogColor[1] = 0;
	m_SceneFogColor[2] = 0;
	m_SceneFogMode = MATERIAL_FOG_NONE;

	// This is state that isn't part of the snapshot per-se, because we
	// don't need it when it's actually time to render. This just helps us
	// to construct the shadow state.
	m_DynamicState.m_ClearColor = D3DCOLOR_XRGB(0,0,0);

	if ( bFullReset )
	{
		InitVertexAndPixelShaders();
	}
	else
	{
		// just need to dirty the dynamic state, desired state gets copied into below
		Q_memset( m_DynamicState.m_pVectorPixelShaderConstant, 0, g_pHardwareConfig->Caps().m_NumPixelShaderConstants * sizeof( Vector4D ) );
		Q_memset( m_DynamicState.m_pBooleanPixelShaderConstant, 0, g_pHardwareConfig->Caps().m_NumBooleanPixelShaderConstants * sizeof( BOOL ) );
		Q_memset( m_DynamicState.m_pIntegerPixelShaderConstant, 0, g_pHardwareConfig->Caps().m_NumIntegerPixelShaderConstants * sizeof( IntVector4D ) );

		Q_memset( m_DynamicState.m_pVectorVertexShaderConstant, 0, g_pHardwareConfig->Caps().m_NumVertexShaderConstants * sizeof( Vector4D ) );
		Q_memset( m_DynamicState.m_pBooleanVertexShaderConstant, 0, g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants * sizeof( BOOL ) );
		Q_memset( m_DynamicState.m_pIntegerVertexShaderConstant, 0, g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants * sizeof( IntVector4D ) );

		SetStandardVertexShaderConstants( OVERBRIGHT );
	}

	//Set the default compressed depth range written to dest alpha. Only need to compress it for 8bit alpha to get a useful gradient.
	m_DynamicState.m_DestAlphaDepthRange = (g_pHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT) ? 8192.0f : 192.0f;

	m_CachedAmbientLightCube = STATE_CHANGED;

	// Set the constant color
	m_DynamicState.m_ConstantColor = 0xFFFFFFFF;
	Color4ub( 255, 255, 255, 255 );

	// Ambient light color
	m_DynamicState.m_Ambient = 0;
	SetSupportedRenderState( D3DRS_AMBIENT, m_DynamicState.m_Ambient );

	// Fog
	m_VertexShaderFogParams[0] = m_VertexShaderFogParams[1] = 0.0f;
	m_WorldSpaceCameraPositon.Init( 0, 0, 0.01f, 0 ); // Don't let z be zero, as some pixel shaders will divide by this
	m_DynamicState.m_FogColor = 0xFFFFFFFF;
	m_DynamicState.m_PixelFogColor[0] = m_DynamicState.m_PixelFogColor[1] = 
		m_DynamicState.m_PixelFogColor[2] = m_DynamicState.m_PixelFogColor[3] = 0.0f;
	m_DynamicState.m_bFogGammaCorrectionDisabled = false;
	m_DynamicState.m_FogEnable = false;
	m_DynamicState.m_SceneFog = MATERIAL_FOG_NONE;
	m_DynamicState.m_FogMode = D3DFOG_NONE;
	m_DynamicState.m_FogStart = -1;
	m_DynamicState.m_FogEnd = -1;
	m_DynamicState.m_FogMaxDensity = -1.0f;
	m_DynamicState.m_FogZ = 0.0f;

	SetSupportedRenderState( D3DRS_FOGCOLOR, m_DynamicState.m_FogColor );
	SetSupportedRenderState( D3DRS_FOGENABLE, m_DynamicState.m_FogEnable );
	SetSupportedRenderState( D3DRS_FOGTABLEMODE, D3DFOG_NONE );
	SetSupportedRenderState( D3DRS_FOGVERTEXMODE, D3DFOG_NONE );
	SetSupportedRenderState( D3DRS_RANGEFOGENABLE, false );

	FogStart( 0 );
	FogEnd( 0 );
	FogMaxDensity( 1.0f );

	m_DynamicState.m_bSRGBWritesEnabled = false;

	// Set the cull mode
	m_DynamicState.m_bCullEnabled = true;
	m_DynamicState.m_CullMode = D3DCULL_CCW;
	m_DynamicState.m_DesiredCullMode = D3DCULL_CCW;
	SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

	// No shade mode yet
	m_DynamicState.m_ShadeMode = (D3DSHADEMODE)-1;
	ShadeMode( SHADER_SMOOTH );

	m_DynamicState.m_bHWMorphingEnabled = false;

	// Skinning...
	m_DynamicState.m_NumBones = 0;
	m_DynamicState.m_VertexBlend = (D3DVERTEXBLENDFLAGS)-1;
	SetSupportedRenderState( D3DRS_VERTEXBLEND, D3DVBF_DISABLE );
	SetSupportedRenderState( D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE );

	// No normal normalization
	m_DynamicState.m_NormalizeNormals = false;
	SetSupportedRenderState( D3DRS_NORMALIZENORMALS, m_DynamicState.m_NormalizeNormals );

	bool bAntialiasing = ( m_PresentParameters.MultiSampleType != D3DMULTISAMPLE_NONE );
	if ( g_pHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT )
	{
		bAntialiasing = false;
	}
	SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, bAntialiasing );
	
	// Anisotropic filtering is disabled by default
	if ( bFullReset )
	{
		SetAnisotropicLevel( 1 );
	}
	
	int i;
	for ( i = 0; i < g_pHardwareConfig->ActualCaps().m_NumTextureStages; ++i )
	{
		TextureStage(i).m_TextureTransformFlags = D3DTTFF_DISABLE;
		TextureStage(i).m_BumpEnvMat00 = 1.0f;
		TextureStage(i).m_BumpEnvMat01 = 0.0f;
		TextureStage(i).m_BumpEnvMat10 = 0.0f;
		TextureStage(i).m_BumpEnvMat11 = 1.0f;

		SetTextureStageState( i, D3DTSS_TEXTURETRANSFORMFLAGS, TextureStage(i).m_TextureTransformFlags );
		SetTextureStageState( i, D3DTSS_BUMPENVMAT00, *( ( LPDWORD ) (&TextureStage(i).m_BumpEnvMat00) ) );
		SetTextureStageState( i, D3DTSS_BUMPENVMAT01, *( ( LPDWORD ) (&TextureStage(i).m_BumpEnvMat01) ) );
		SetTextureStageState( i, D3DTSS_BUMPENVMAT10, *( ( LPDWORD ) (&TextureStage(i).m_BumpEnvMat10) ) );
		SetTextureStageState( i, D3DTSS_BUMPENVMAT11, *( ( LPDWORD ) (&TextureStage(i).m_BumpEnvMat11) ) );
	}

	for ( i = 0; i < g_pHardwareConfig->ActualCaps().m_NumSamplers; ++i )
	{
		SamplerState(i).m_BoundTexture = INVALID_SHADERAPI_TEXTURE_HANDLE;						  
		SamplerState(i).m_UTexWrap = D3DTADDRESS_WRAP;
		SamplerState(i).m_VTexWrap = D3DTADDRESS_WRAP;
		SamplerState(i).m_WTexWrap = D3DTADDRESS_WRAP;
		SamplerState(i).m_MagFilter = D3DTEXF_POINT;
		SamplerState(i).m_MinFilter = D3DTEXF_POINT;
		SamplerState(i).m_MipFilter = D3DTEXF_NONE;
		SamplerState(i).m_FinestMipmapLevel = 0;
		SamplerState(i).m_LodBias = 0.0f;
		SamplerState(i).m_TextureEnable = false;
		SamplerState(i).m_SRGBReadEnable = false;

		// Just some initial state...
		Dx9Device()->SetTexture( i, 0 );

		SetSamplerState( i, D3DSAMP_ADDRESSU, SamplerState(i).m_UTexWrap );
		SetSamplerState( i, D3DSAMP_ADDRESSV, SamplerState(i).m_VTexWrap ); 
		SetSamplerState( i, D3DSAMP_ADDRESSW, SamplerState(i).m_WTexWrap ); 
		SetSamplerState( i, D3DSAMP_MINFILTER, SamplerState(i).m_MinFilter );
		SetSamplerState( i, D3DSAMP_MAGFILTER, SamplerState(i).m_MagFilter );
		SetSamplerState( i, D3DSAMP_MIPFILTER, SamplerState(i).m_MipFilter );
		SetSamplerState( i, D3DSAMP_MAXMIPLEVEL, SamplerState(i).m_FinestMipmapLevel );
		SetSamplerState( i, D3DSAMP_MIPMAPLODBIAS, SamplerState(i).m_LodBias );

		SetSamplerState( i, D3DSAMP_BORDERCOLOR, RGB( 0,0,0 ) );
	}

	// FIXME!!!!! : This barfs with the debug runtime on 6800.
	for( i = 0; i < g_pHardwareConfig->ActualCaps().m_nVertexTextureCount; i++ )
	{
		m_DynamicState.m_VertexTextureState[i].m_BoundTexture = INVALID_SHADERAPI_TEXTURE_HANDLE;
		Dx9Device()->SetTexture( D3DVERTEXTEXTURESAMPLER0 + i, NULL );

		m_DynamicState.m_VertexTextureState[i].m_UTexWrap = D3DTADDRESS_CLAMP;
		m_DynamicState.m_VertexTextureState[i].m_VTexWrap = D3DTADDRESS_CLAMP;
//		m_DynamicState.m_VertexTextureState[i].m_WTexWrap = D3DTADDRESS_CLAMP;
		m_DynamicState.m_VertexTextureState[i].m_MinFilter = D3DTEXF_POINT;
		m_DynamicState.m_VertexTextureState[i].m_MagFilter = D3DTEXF_POINT;
		m_DynamicState.m_VertexTextureState[i].m_MipFilter = D3DTEXF_POINT;
		SetSamplerState( D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_ADDRESSU, m_DynamicState.m_VertexTextureState[i].m_UTexWrap );
		SetSamplerState( D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_ADDRESSV, m_DynamicState.m_VertexTextureState[i].m_VTexWrap );
//		SetSamplerState( D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_ADDRESSW, m_DynamicState.m_VertexTextureState[i].m_WTexWrap );
		SetSamplerState( D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_MINFILTER, m_DynamicState.m_VertexTextureState[i].m_MinFilter );
		SetSamplerState( D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_MAGFILTER, m_DynamicState.m_VertexTextureState[i].m_MagFilter );
		SetSamplerState( D3DVERTEXTEXTURESAMPLER0 + i, D3DSAMP_MIPFILTER, m_DynamicState.m_VertexTextureState[i].m_MipFilter );
	}

	m_DynamicState.m_NumLights = 0;
	for ( i = 0; i < MAX_NUM_LIGHTS; ++i)
	{
		m_DynamicState.m_LightEnable[i] = false;
		m_DynamicState.m_LightChanged[i] = STATE_CHANGED;
		m_DynamicState.m_LightEnableChanged[i] = STATE_CHANGED;
	}

	for ( i = 0; i < NUM_MATRIX_MODES; ++i)
	{
		// By setting this to *not* be identity, we force an update...
		m_DynamicState.m_TransformType[i] = TRANSFORM_IS_GENERAL;
		m_DynamicState.m_TransformChanged[i] = STATE_CHANGED;
	}

	// set the board state to match the default state
	m_TransitionTable.UseDefaultState();

	// Set the default render state
	SetDefaultState();

	// Constant for all time
	SetSupportedRenderState( D3DRS_CLIPPING, TRUE );
	SetSupportedRenderState( D3DRS_LOCALVIEWER, TRUE );
	SetSupportedRenderState( D3DRS_POINTSCALEENABLE, FALSE );
	SetSupportedRenderState( D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL );
	SetSupportedRenderState( D3DRS_SPECULARMATERIALSOURCE, D3DMCS_COLOR2 );
	SetSupportedRenderState( D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL );
	SetSupportedRenderState( D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL );
	SetSupportedRenderState( D3DRS_COLORVERTEX, TRUE ); // This defaults to true anyways. . . 

	// Set a default identity material.
	SetDefaultMaterial();

	if ( bFullReset )
	{
		// Set the modelview matrix to identity too
		for ( i = 0; i < NUM_MODEL_TRANSFORMS; ++i )
		{
			SetIdentityMatrix( m_boneMatrix[i] );
		}
		MatrixMode( MATERIAL_VIEW );
		LoadIdentity();
		MatrixMode( MATERIAL_PROJECTION );
		LoadIdentity();
	}

	m_DynamicState.m_Viewport.X = m_DynamicState.m_Viewport.Y = 
		m_DynamicState.m_Viewport.Width = m_DynamicState.m_Viewport.Height = 0xFFFFFFFF;
	m_DynamicState.m_Viewport.MinZ = m_DynamicState.m_Viewport.MaxZ = 0.0;

	// Be sure scissoring is off
	m_DynamicState.m_RenderState[D3DRS_SCISSORTESTENABLE] = FALSE;
	SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE );
	m_DynamicState.m_ScissorRect.left   = -1;
	m_DynamicState.m_ScissorRect.top    = -1;
	m_DynamicState.m_ScissorRect.right  = -1;
	m_DynamicState.m_ScissorRect.bottom = -1;

	//SetHeightClipMode( MATERIAL_HEIGHTCLIPMODE_DISABLE );
	EnableFastClip( false );
	float fFakePlane[4];
	unsigned int iFakePlaneVal = 0xFFFFFFFF;
	fFakePlane[0] = fFakePlane[1] = fFakePlane[2] = fFakePlane[3] = *((float *)&iFakePlaneVal);
	SetFastClipPlane( fFakePlane ); //doing this to better wire up plane change detection

	float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	// Make sure that our state is dirty.
	m_DynamicState.m_UserClipPlaneEnabled = 0;
	m_DynamicState.m_UserClipPlaneChanged = 0;
	m_DynamicState.m_UserClipLastUpdatedUsingFixedFunction = false;
	for( i = 0; i < g_pHardwareConfig->MaxUserClipPlanes(); i++ )
	{
		// Make sure that our state is dirty.
		m_DynamicState.m_UserClipPlaneWorld[i] = DirectX::XMVectorSetX(m_DynamicState.m_UserClipPlaneWorld[i], -1.0f);
		m_DynamicState.m_UserClipPlaneProj[i] = DirectX::XMVectorSetX(m_DynamicState.m_UserClipPlaneProj[i], -9999.0f);
		m_DynamicState.m_UserClipPlaneEnabled |= ( 1 << i );
		SetClipPlane( i, zero );
		EnableClipPlane( i, false );
		Assert( m_DynamicState.m_UserClipPlaneEnabled == 0 );
	}
	Assert( m_DynamicState.m_UserClipPlaneChanged == ((1 << g_pHardwareConfig->MaxUserClipPlanes()) - 1) );

	m_DynamicState.m_FastClipEnabled = false;
	m_DynamicState.m_bFastClipPlaneChanged = true;

	// User clip override
	m_DynamicState.m_bUserClipTransformOverride = false;
	m_DynamicState.m_UserClipTransform = DirectX::XMMatrixIdentity();

	// Viewport defaults to the window size
	RECT windowRect;
#if !defined( DX_TO_GL_ABSTRACTION )
	GetClientRect( (HWND)m_hWnd, &windowRect );
#else
	toglGetClientRect( (VD3DHWND)m_hWnd, &windowRect );
#endif

	ShaderViewport_t viewport;
	viewport.Init( windowRect.left, windowRect.top, 
		windowRect.right - windowRect.left,	windowRect.bottom - windowRect.top );
	SetViewports( 1, &viewport );

	// No render mesh
	m_pRenderMesh = 0;

	// Reset cached vertex decl
	if ( m_DynamicState.m_pVertexDecl )
	{
		m_DynamicState.m_pVertexDecl.Release();
	}
	
	AcquireInternalRenderTargets();
	SetRenderTarget();

	// Maintain vertex + pixel shader constant buffers
	Vector4D *pVectorPixelShaderConstants = m_DesiredState.m_pVectorPixelShaderConstant;
	int *pBooleanPixelShaderConstants = m_DesiredState.m_pBooleanPixelShaderConstant;
	IntVector4D *pIntegerPixelShaderConstants = m_DesiredState.m_pIntegerPixelShaderConstant;
	Vector4D *pVectorVertexShaderConstants = m_DesiredState.m_pVectorVertexShaderConstant;
	int *pBooleanVertexShaderConstants = m_DesiredState.m_pBooleanVertexShaderConstant;
	IntVector4D *pIntegerVertexShaderConstants = m_DesiredState.m_pIntegerVertexShaderConstant;
	m_DesiredState = m_DynamicState;
	m_DesiredState.m_pVectorPixelShaderConstant = pVectorPixelShaderConstants;
	m_DesiredState.m_pBooleanPixelShaderConstant = pBooleanPixelShaderConstants;
	m_DesiredState.m_pIntegerPixelShaderConstant = pIntegerPixelShaderConstants;
	m_DesiredState.m_pVectorVertexShaderConstant = pVectorVertexShaderConstants;
	m_DesiredState.m_pBooleanVertexShaderConstant = pBooleanVertexShaderConstants;
	m_DesiredState.m_pIntegerVertexShaderConstant = pIntegerVertexShaderConstants;
	if ( g_pHardwareConfig->Caps().m_SupportsPixelShaders )
	{
		if ( !bFullReset )
		{
			//Full resets init the values to defaults. Normal resets just leave them dirty.
			if( g_pHardwareConfig->Caps().m_NumVertexShaderConstants != 0 )
				SetVertexShaderConstant( 0, m_DesiredState.m_pVectorVertexShaderConstant[0].Base(), g_pHardwareConfig->Caps().m_NumVertexShaderConstants, true );
			
			if( g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants != 0 )
				SetIntegerVertexShaderConstant( 0, (int *)m_DesiredState.m_pIntegerVertexShaderConstant, g_pHardwareConfig->Caps().m_NumIntegerVertexShaderConstants, true );
			
			if( g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants != 0 )
				SetBooleanVertexShaderConstant( 0, m_DesiredState.m_pBooleanVertexShaderConstant, g_pHardwareConfig->Caps().m_NumBooleanVertexShaderConstants, true );


			if( g_pHardwareConfig->Caps().m_NumPixelShaderConstants != 0 )
				SetPixelShaderConstant( 0, m_DesiredState.m_pVectorPixelShaderConstant[0].Base(), g_pHardwareConfig->Caps().m_NumPixelShaderConstants, true );

			if( g_pHardwareConfig->Caps().m_NumIntegerPixelShaderConstants != 0 )
				SetIntegerPixelShaderConstant( 0, (int *)m_DesiredState.m_pIntegerPixelShaderConstant, g_pHardwareConfig->Caps().m_NumIntegerPixelShaderConstants, true );

			if( g_pHardwareConfig->Caps().m_NumBooleanPixelShaderConstants != 0 )
				SetBooleanPixelShaderConstant( 0, m_DesiredState.m_pBooleanPixelShaderConstant, g_pHardwareConfig->Caps().m_NumBooleanPixelShaderConstants, true );
		}
	}

	RECORD_DEBUG_STRING( "END ResetRenderState" );

	m_bResettingRenderState = false;
}

//-----------------------------------------------------------------------------
// Sets the default render state
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetDefaultState()
{
	LOCK_SHADERAPI();

	// NOTE: This used to be in the material system, but I want to avoid all the per pass/batch
	// virtual function calls.
	int numTextureStages = g_pHardwareConfig->GetTextureStageCount();

	// FIXME: This is a brutal hack. We only need to load these transforms for fixed-function
	// hardware. Cap the max here to 4.
	if ( IsPC() )
	{
		numTextureStages = min( numTextureStages, 4 );
		int i;
		for( i = 0; i < numTextureStages; i++ )
		{
			CShaderAPIDx8::DisableTextureTransform( (TextureStage_t)i );
			CShaderAPIDx8::MatrixMode( (MaterialMatrixMode_t)(MATERIAL_TEXTURE0 + i) );
			CShaderAPIDx8::LoadIdentity( );
		}
	}
	CShaderAPIDx8::MatrixMode( MATERIAL_MODEL );

	CShaderAPIDx8::Color4ub( 255, 255, 255, 255 );
	CShaderAPIDx8::ShadeMode( SHADER_SMOOTH );
	CShaderAPIDx8::SetVertexShaderIndex( );
	CShaderAPIDx8::SetPixelShaderIndex( );

	MeshMgr()->MarkUnusedVertexFields( 0, 0, NULL );
}



//-----------------------------------------------------------------------------
//
// Methods related to vertex format
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Sets the vertex
//-----------------------------------------------------------------------------
inline void CShaderAPIDx8::SetVertexDecl( VertexFormat_t vertexFormat, bool bHasColorMesh, bool bUsingFlex, bool bUsingMorph )
{
	VPROF("CShaderAPIDx8::SetVertexDecl");

	se::win::com::com_ptr<IDirect3DVertexDeclaration9> pDecl = FindOrCreateVertexDecl( Dx9Device(), vertexFormat, bHasColorMesh, bUsingFlex, bUsingMorph );
	Assert( pDecl );

	if ( pDecl != m_DynamicState.m_pVertexDecl && pDecl )
	{
		const HRESULT hr = Dx9Device()->SetVertexDeclaration( pDecl );
		if ( SUCCEEDED( hr ) )
		{
			m_DynamicState.m_pVertexDecl = std::move( pDecl );
			return;
		}

		Assert( false );
		Warning( __FUNCTION__ ": IDirect3DDevice9Ex::SetVertexDeclaration() failed w/e %s.\n",
			se::win::com::com_error_category().message(hr).c_str() );
	}
}



//-----------------------------------------------------------------------------
//
// Methods related to vertex buffers
//
//-----------------------------------------------------------------------------

IMesh *CShaderAPIDx8::GetFlexMesh()
{
	LOCK_SHADERAPI();
	return MeshMgr()->GetFlexMesh();
}

//-----------------------------------------------------------------------------
// Gets the dynamic mesh
//-----------------------------------------------------------------------------
IMesh* CShaderAPIDx8::GetDynamicMesh( IMaterial* pMaterial, int nHWSkinBoneCount, bool buffered,
								IMesh* pVertexOverride, IMesh* pIndexOverride )
{
	Assert( (pMaterial == NULL) || ((IMaterialInternal *)pMaterial)->IsRealTimeVersion() );

	LOCK_SHADERAPI();
	return MeshMgr()->GetDynamicMesh( pMaterial, 0, nHWSkinBoneCount, buffered, pVertexOverride, pIndexOverride );
}

IMesh* CShaderAPIDx8::GetDynamicMeshEx( IMaterial* pMaterial, VertexFormat_t vertexFormat, int nHWSkinBoneCount, 
	bool bBuffered, IMesh* pVertexOverride, IMesh* pIndexOverride )
{
	Assert( (pMaterial == NULL) || ((IMaterialInternal *)pMaterial)->IsRealTimeVersion() );

	LOCK_SHADERAPI();
	return MeshMgr()->GetDynamicMesh( pMaterial, vertexFormat, nHWSkinBoneCount, bBuffered, pVertexOverride, pIndexOverride );
}

//-----------------------------------------------------------------------------
// Returns the number of vertices we can render using the dynamic mesh
//-----------------------------------------------------------------------------
void CShaderAPIDx8::GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices )
{
	LOCK_SHADERAPI();
	MeshMgr()->GetMaxToRender( pMesh, bMaxUntilFlush, pMaxVerts, pMaxIndices );
}

int CShaderAPIDx8::GetMaxVerticesToRender( IMaterial *pMaterial )
{
	pMaterial = ((IMaterialInternal *)pMaterial)->GetRealTimeVersion(); //always work with the realtime version internally

	LOCK_SHADERAPI();
	return MeshMgr()->GetMaxVerticesToRender( pMaterial );
}

int CShaderAPIDx8::GetMaxIndicesToRender( )
{
	LOCK_SHADERAPI();
	return MeshMgr()->GetMaxIndicesToRender( );
}

void CShaderAPIDx8::MarkUnusedVertexFields( unsigned int nFlags, int nTexCoordCount, bool *pUnusedTexCoords )
{
	LOCK_SHADERAPI();
	MeshMgr()->MarkUnusedVertexFields( nFlags, nTexCoordCount, pUnusedTexCoords );
}

//-----------------------------------------------------------------------------
// Draws the mesh
//-----------------------------------------------------------------------------
void CShaderAPIDx8::DrawMesh( CMeshBase *pMesh )
{
	VPROF("CShaderAPIDx8::DrawMesh");
	if ( ShaderUtil()->GetConfig().m_bSuppressRendering )
		return;

#if defined( PIX_INSTRUMENTATION ) || defined( NVPERFHUD )
	PIXifyName( s_pPIXMaterialName, sizeof( s_pPIXMaterialName ), m_pMaterial->GetName() );
	BeginPIXEvent( PIX_VALVE_ORANGE, s_pPIXMaterialName );
#endif

	m_pRenderMesh = pMesh;
	VertexFormat_t vertexFormat = m_pRenderMesh->GetVertexFormat();
	SetVertexDecl( vertexFormat, m_pRenderMesh->HasColorMesh(), m_pRenderMesh->HasFlexMesh(), m_pMaterial->IsUsingVertexID() );
	CommitStateChanges();
	Assert( m_pRenderMesh && m_pMaterial );
	m_pMaterial->DrawMesh( CompressionType( vertexFormat ) );
	m_pRenderMesh = NULL;

#if defined( PIX_INSTRUMENTATION ) || defined( NVPERFHUD )
	EndPIXEvent();
#endif
}

void CShaderAPIDx8::DrawWithVertexAndIndexBuffers( void )
{
	VPROF("CShaderAPIDx8::DrawWithVertexAndIndexBuffers");
	if ( ShaderUtil()->GetConfig().m_bSuppressRendering )
		return;

#if defined( PIX_INSTRUMENTATION ) || defined( NVPERFHUD )
	PIXifyName( s_pPIXMaterialName, sizeof( s_pPIXMaterialName ), m_pMaterial->GetName());
	BeginPIXEvent( PIX_VALVE_ORANGE, s_pPIXMaterialName );
#endif

//	m_pRenderMesh = pMesh;
	// FIXME: need to make this deal with multiple streams, etc.
	VertexFormat_t vertexFormat = MeshMgr()->GetCurrentVertexFormat();
	SetVertexDecl( vertexFormat, false /*m_pRenderMesh->HasColorMesh()*/, 
		false /*m_pRenderMesh->HasFlexMesh()*/, false /*m_pRenderMesh->IsUsingMorphData()*/ );
	CommitStateChanges();
	if ( m_pMaterial )
	{
		m_pMaterial->DrawMesh( CompressionType( vertexFormat ) );
	}
	else
	{
		MeshMgr()->RenderPassWithVertexAndIndexBuffers();
	}
//	m_pRenderMesh = NULL;

#if defined( PIX_INSTRUMENTATION ) || defined( NVPERFHUD )
	EndPIXEvent();
#endif
}

//-----------------------------------------------------------------------------
// Discards the vertex buffers
//-----------------------------------------------------------------------------
void CShaderAPIDx8::DiscardVertexBuffers()
{
	MeshMgr()->DiscardVertexBuffers();
}

void CShaderAPIDx8::ForceHardwareSync_WithManagedTexture()
{
	if ( !m_pFrameSyncTexture )
		return;

	// Set the default state for everything so we don't get more than we ask for here!
	SetDefaultState();

	D3DLOCKED_RECT rect;

	tmZone( TELEMETRY_LEVEL1, TMZF_NONE, "%s", __FUNCTION__ );

	HRESULT hr = m_pFrameSyncTexture->LockRect( 0, &rect, NULL, 0 );
	if ( SUCCEEDED( hr ) )
	{
		// modify..
		unsigned long *pData = (unsigned long*)rect.pBits;
		(*pData)++; 

		m_pFrameSyncTexture->UnlockRect( 0 );

		// Now draw something with this texture.
		DWORD iStage = 0;
		IDirect3DBaseTexture9 *pOldTexture;
		hr = Dx9Device()->GetTexture( iStage, &pOldTexture );
		if ( SUCCEEDED( hr ) )
		{
			Dx9Device()->SetTexture( iStage, m_pFrameSyncTexture );
			// Remember the old FVF.
			DWORD oldFVF;
			hr = Dx9Device()->GetFVF( &oldFVF );
			if ( SUCCEEDED( hr ) )
			{
				// Set the new FVF.
				Dx9Device()->SetFVF( D3DFVF_XYZ );
				// Now, draw the simplest primitive D3D has ever seen.
				unsigned short indices[3] = { 0, 1, 2 };
				Vector verts[3] = {vec3_origin, vec3_origin, vec3_origin};
				
				tmZone( TELEMETRY_LEVEL1, TMZF_NONE, "DrawIndexedPrimitiveUP" );

				Dx9Device()->DrawIndexedPrimitiveUP(
					D3DPT_TRIANGLELIST,
					0,				// Min vertex index
					3,				// Num vertices used
					1,				// # primitives
					indices,		// indices
					D3DFMT_INDEX16,	// index format
					verts,			// Vertices
					sizeof( Vector )// Vertex stride
					);
				
				Dx9Device()->SetFVF( oldFVF );
			}
			Dx9Device()->SetTexture( iStage, pOldTexture );
		}
	}
	// If this assert fails, then we failed somewhere above.
	AssertOnce( SUCCEEDED( hr ) );
}

void CShaderAPIDx8::UpdateFrameSyncQuery( int queryIndex, bool bIssue )
{
	Assert(queryIndex < NUM_FRAME_SYNC_QUERIES);
	// wait if already issued
	if ( m_bQueryIssued[queryIndex] )
	{
		tmZone( TELEMETRY_LEVEL1, TMZF_NONE, "%s", __FUNCTION__ );

		double flStartTime = Plat_FloatTime();
		BOOL dummyData = 0;
		HRESULT hr = S_OK;
		// NOTE: This fix helps out motherboards that are a little freaky.
		// On such boards, sometimes the driver has to reset itself (an event which takes several seconds)
		// and when that happens, the frame sync query object gets lost
		for (;;)
		{
			hr = m_pFrameSyncQueryObject[queryIndex]->GetData( &dummyData, sizeof( dummyData ), D3DGETDATA_FLUSH );
			if ( hr != S_FALSE )
				break;
			double flCurrTime = Plat_FloatTime();
			// don't wait more than 200ms (5fps) for these
			if ( flCurrTime - flStartTime > 0.200f )
				break;
			// Avoid burning a full core while waiting for the query. Spinning can actually harm performance
			// because there might be driver threads that are trying to do work that end up starved, and the
			// power drawn by the CPU may take away from the power available to the integrated graphics chip.
			// A sleep of one millisecond should never be long enough to affect performance, especially since
			// this should only trigger when the CPU is already ahead of the GPU.
			// On L4D2/TF2 in GL mode this spinning was causing slowdowns.
			ThreadSleep( 1 );
		}
		m_bQueryIssued[queryIndex] = false;
		Assert(hr == S_OK || hr == D3DERR_DEVICELOST);

		if ( hr == D3DERR_DEVICELOST )
		{
			MarkDeviceLost( );
			return;
		}
	}
	if ( bIssue )
	{
		m_pFrameSyncQueryObject[queryIndex]->Issue( D3DISSUE_END );
		m_bQueryIssued[queryIndex] = true;
	}
}

void CShaderAPIDx8::ForceHardwareSync( void )
{
	LOCK_SHADERAPI();
	VPROF( "CShaderAPIDx8::ForceHardwareSync" );

#ifdef DX_TO_GL_ABSTRACTION
	if ( true )
#else
	if ( !mat_frame_sync_enable.GetInt() )
#endif
		return;

	// need to flush the dynamic buffer	and make sure the entire image is there
	FlushBufferedPrimitives();

	RECORD_COMMAND( DX8_HARDWARE_SYNC, 0 );

	// How do you query dx9 for how many frames behind the hardware is or, alternatively, how do you tell the hardware to never be more than N frames behind?
	// 1) The old QueryPendingFrameCount design was removed.  It was
	// a simple transaction with the driver through the 
	// GetDriverState, trivial for the drivers to lie.  We came up 
	// with a much better scheme for tracking pending frames where 
	// the driver can not lie without a possible performance loss:  
	// use the asynchronous query system with D3DQUERYTYPE_EVENT and 
	// data size 0.  When GetData returns S_OK for the query, you 
	// know that frame has finished.
	if ( mat_frame_sync_force_texture.GetBool() )
	{
		ForceHardwareSync_WithManagedTexture();
	}
	else if ( m_pFrameSyncQueryObject[0] )
	{
		// FIXME: Could install a callback into the materialsystem to do something while waiting for
		// the frame to finish (update sound, etc.)
		
		// Disable VCR mode here or else it'll screw up (and we don't really care if this part plays back in exactly the same amount of time).
		VCRSetEnabled( false );

		m_currentSyncQuery ++;
		if ( m_currentSyncQuery >= ssize(m_pFrameSyncQueryObject) )
		{
			m_currentSyncQuery = 0;
		}
		int waitIndex = ((m_currentSyncQuery + NUM_FRAME_SYNC_QUERIES) - (NUM_FRAME_SYNC_FRAMES_LATENCY+1)) % NUM_FRAME_SYNC_QUERIES;
		UpdateFrameSyncQuery( waitIndex, false );
		UpdateFrameSyncQuery( m_currentSyncQuery, true );
		VCRSetEnabled( true );
	}
}


//-----------------------------------------------------------------------------
// Needs render state
//-----------------------------------------------------------------------------
void CShaderAPIDx8::QueueResetRenderState()
{
	m_bResetRenderStateNeeded = true;
}


//-----------------------------------------------------------------------------
// Use this to begin and end the frame
//-----------------------------------------------------------------------------
void CShaderAPIDx8::BeginFrame()
{
	LOCK_SHADERAPI();

	if ( m_bResetRenderStateNeeded )
	{
		ResetRenderState( false );
		m_bResetRenderStateNeeded = false;
	}

#if ALLOW_SMP_ACCESS
	Dx9Device()->SetASyncMode( mat_use_smp.GetInt() != 0 );
#endif

	++m_CurrentFrame;
	m_nTextureMemoryUsedLastFrame = 0;
}

void CShaderAPIDx8::EndFrame()
{
	LOCK_SHADERAPI();

	MEMCHECK;

	ExportTextureList();
}


void CShaderAPIDx8::AddBufferToTextureList( const char *pName, D3DSURFACE_DESC &desc )
{
//	ImageFormat imageFormat;
//	imageFormat = ImageLoader::D3DFormatToImageFormat( desc.Format );
//	if( imageFormat < 0 )
//	{
//		Assert( 0 );
//		return;
//	}
	KeyValues *pSubKey = m_pDebugTextureList->CreateNewKey();
	pSubKey->SetString( "Name", pName );
	pSubKey->SetString( "TexGroup", TEXTURE_GROUP_RENDER_TARGET );
	pSubKey->SetInt( "Size", 
//		ImageLoader::SizeInBytes( imageFormat ) * desc.Width * desc.Height );
		4 * desc.Width * desc.Height );
	pSubKey->SetString( "Format", "32 bit buffer (hack)" );//ImageLoader::GetName( imageFormat ) );
	pSubKey->SetInt( "Width", desc.Width );
	pSubKey->SetInt( "Height", desc.Height );
	
	pSubKey->SetInt( "BindsMax", 1 );
	pSubKey->SetInt( "BindsFrame", 1 );
}

void CShaderAPIDx8::ExportTextureList()
{
	if ( !m_bEnableDebugTextureList )
		return;

	if ( !m_pBackBufferSurface || !m_pZBufferSurface )
		// Device vanished...
		return;

	m_nDebugDataExportFrame = m_CurrentFrame;

	if ( m_pDebugTextureList )
		m_pDebugTextureList->deleteThis();

	m_pDebugTextureList = new KeyValues( "TextureList" );

	m_nTextureMemoryUsedTotal = 0;
	m_nTextureMemoryUsedPicMip1 = 0;
	m_nTextureMemoryUsedPicMip2 = 0;
	for ( ShaderAPITextureHandle_t hTexture = m_Textures.Head() ; hTexture != m_Textures.InvalidIndex(); hTexture = m_Textures.Next( hTexture ) )
	{
		Texture_t &tex = m_Textures[hTexture];
		
		if ( !( tex.m_Flags & Texture_t::IS_ALLOCATED ) )
			continue;

		// Compute total texture memory usage
		m_nTextureMemoryUsedTotal += tex.GetMemUsage();

		// Compute picmip memory usage
		{
			intp numBytes = tex.GetMemUsage();

			if ( tex.m_NumLevels > 1 )
			{
				if ( tex.GetWidth() > 4 || tex.GetHeight() > 4 || tex.GetDepth() > 4 )
				{
					intp topmipsize = ImageLoader::GetMemRequired( tex.GetWidth(), tex.GetHeight(), tex.GetDepth(), tex.GetImageFormat(), false );
					numBytes -= topmipsize;

					m_nTextureMemoryUsedPicMip1 += numBytes;

					if ( tex.GetWidth() > 8 || tex.GetHeight() > 8 || tex.GetDepth() > 8 )
					{
						int othermipsizeRatio = ( ( tex.GetWidth() > 8 ) ? 2 : 1 ) * ( ( tex.GetHeight() > 8 ) ? 2 : 1 ) * ( ( tex.GetDepth() > 8 ) ? 2 : 1 );
						intp othermipsize = topmipsize / othermipsizeRatio;
						numBytes -= othermipsize;
					}

					m_nTextureMemoryUsedPicMip1 += numBytes;
				}
				else
				{
					m_nTextureMemoryUsedPicMip1 += numBytes;
					m_nTextureMemoryUsedPicMip2 += numBytes;
				}
			}
			else
			{
				m_nTextureMemoryUsedPicMip1 += numBytes;
				m_nTextureMemoryUsedPicMip2 += numBytes;
			}
		}

		if ( !m_bDebugGetAllTextures &&
				tex.m_LastBoundFrame != m_CurrentFrame )
			continue;

		if ( tex.m_LastBoundFrame != m_CurrentFrame )
			tex.m_nTimesBoundThisFrame = 0;

		KeyValues *pSubKey = m_pDebugTextureList->CreateNewKey();
		pSubKey->SetString( "Name", tex.m_DebugName.String() );
		pSubKey->SetString( "TexGroup", tex.m_TextureGroupName.String() );
#ifdef PLATFORM_64BITS
		pSubKey->SetUint64( "Size", tex.GetMemUsage() );
#else
		pSubKey->SetInt( "Size", tex.GetMemUsage() );
#endif
		if ( tex.GetCount() > 1 )
			pSubKey->SetInt( "Count", tex.GetCount() );
		pSubKey->SetString( "Format", ImageLoader::GetName( tex.GetImageFormat() ) );
		pSubKey->SetInt( "Width", tex.GetWidth() );
		pSubKey->SetInt( "Height", tex.GetHeight() );

		pSubKey->SetInt( "BindsMax", tex.m_nTimesBoundMax );
		pSubKey->SetInt( "BindsFrame", tex.m_nTimesBoundThisFrame );
	}

	D3DSURFACE_DESC desc;
	m_pBackBufferSurface->GetDesc( &desc );
	AddBufferToTextureList( "BACKBUFFER", desc );
	AddBufferToTextureList( "FRONTBUFFER", desc );
//	ImageFormat imageFormat = ImageLoader::D3DFormatToImageFormat( desc.Format );
//	if( imageFormat >= 0 )
	{
		VPROF_INCREMENT_GROUP_COUNTER( "TexGroup_frame_" TEXTURE_GROUP_RENDER_TARGET, 
			COUNTER_GROUP_TEXTURE_PER_FRAME, 
//			ImageLoader::SizeInBytes( imageFormat ) * desc.Width * desc.Height );
			2 * 4 * desc.Width * desc.Height ); // hack (times 2 for front and back buffer)
	}

	m_pZBufferSurface->GetDesc( &desc );
	AddBufferToTextureList( "DEPTHBUFFER", desc );
//	imageFormat = ImageLoader::D3DFormatToImageFormat( desc.Format );
//	if( imageFormat >= 0 )
	{
		VPROF_INCREMENT_GROUP_COUNTER( "TexGroup_frame_" TEXTURE_GROUP_RENDER_TARGET, 
			COUNTER_GROUP_TEXTURE_PER_FRAME, 
//			ImageLoader::SizeInBytes( imageFormat ) * desc.Width * desc.Height );
			4 * desc.Width * desc.Height ); // hack
	}
}


//-----------------------------------------------------------------------------
// Releases/reloads resources when other apps want some memory
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ReleaseShaderObjects()
{
	ReleaseInternalRenderTargets();
	EvictManagedResourcesInternal();	

	// FIXME: Move into shaderdevice when textures move over.

#ifdef _DEBUG
	// Helps to find the unreleased textures.
	if ( TextureCount() > 0 )
	{
		ShaderAPITextureHandle_t hTexture;
		for ( hTexture = m_Textures.Head(); hTexture != m_Textures.InvalidIndex(); hTexture = m_Textures.Next( hTexture ) )
		{
			if ( GetTexture( hTexture ).m_NumCopies == 1 )
			{
				if ( GetTexture( hTexture ).GetTexture() )
				{
					Warning( "Didn't correctly clean up texture 0x%8.8zx (%s)\n", hTexture, GetTexture( hTexture ).m_DebugName.String() ); 
				}
			}
			else
			{
				for ( int k = GetTexture( hTexture ).m_NumCopies; --k >= 0; )
				{
					if ( GetTexture( hTexture ).GetTexture( k ) != 0 )
					{
						Warning( "Didn't correctly clean up texture 0x%8.8zx (%s)\n", hTexture, GetTexture( hTexture ).m_DebugName.String() ); 
						break;
					}
				}
			}
		}
	}
#endif

	Assert( TextureCount() == 0 );
}

void CShaderAPIDx8::RestoreShaderObjects()
{
	AcquireInternalRenderTargets();
	SetRenderTarget();
}


//--------------------------------------------------------------------
// PIX instrumentation routines
// Windows only for now.  Turn these on with PIX_INSTRUMENTATION above
//--------------------------------------------------------------------

#if 0	// hack versions for OSX to be able to PIX log even when not built debug...
void CShaderAPIDx8::BeginPIXEvent( unsigned long color, const char* szName )
	{
		LOCK_SHADERAPI();
		GLMBeginPIXEvent( szName );	// direct call no macro
		return;
	}

	void CShaderAPIDx8::EndPIXEvent( void )
	{
		LOCK_SHADERAPI();
		GLMEndPIXEvent();			// direct call no macro
		return;
	}
	
#else

#if defined( PIX_INSTRUMENTATION ) || defined( NVPERFHUD )
ConVar pix_break_on_event( "pix_break_on_event", "" );
#endif

void CShaderAPIDx8::BeginPIXEvent( unsigned long color, const char* szName )
{
#if ( defined( PIX_INSTRUMENTATION ) || defined( NVPERFHUD ) )
	//LOCK_SHADERAPI();

	const char *p = pix_break_on_event.GetString();
	if ( p && V_strlen( p ) )
	{
		if ( V_stristr( szName, p ) != NULL )
		{
			DebuggerBreak();
		}
	}

#if defined ( DX_TO_GL_ABSTRACTION )
	GLMBeginPIXEvent( szName );

#if defined( _WIN32 )
	// AMD PerfStudio integration: Call into D3D9.DLL's D3DPERF_BeginEvent() (this gets intercepted by PerfStudio even in GL mode).
	if ( g_pShaderDeviceMgrDx8->m_pBeginEvent )
	{
		wchar_t wszName[128];
		mbstowcs( wszName, szName, 128 );

		g_pShaderDeviceMgrDx8->m_pBeginEvent( 0x2F2F2F2F, wszName );
	}
#endif
#elif defined(_X360 )
#ifndef _DEBUG
	char szPIXEventName[32];
	PIXifyName( szPIXEventName, szName );
	PIXBeginNamedEvent( color, szPIXEventName );
#endif
#else // PC
	if ( PIXError() )
		return;

	wchar_t wszName[128];
	mbstowcs( wszName, szName, 128 );

	// Fire the PIX event, trapping for errors...
	if ( D3DPERF_BeginEvent( color, wszName ) < 0 )
	{
		Warning( "PIX error Beginning %s event\n", szName );
		IncrementPIXError();
	}
#endif
#endif // #if defined( PIX_INSTRUMENTATION )
}

void CShaderAPIDx8::EndPIXEvent( void )
{
#if ( defined( PIX_INSTRUMENTATION ) || defined( NVPERFHUD ) )
	//LOCK_SHADERAPI();

#if defined ( DX_TO_GL_ABSTRACTION )
	GLMEndPIXEvent();

#if defined( _WIN32 )
	// AMD PerfStudio integration: Call into D3D9.DLL's D3DPERF_EndEvent() (this gets intercepted by PerfStudio even in GL mode).
	if ( g_pShaderDeviceMgrDx8->m_pEndEvent )
	{
		g_pShaderDeviceMgrDx8->m_pEndEvent();
	}
#endif
#elif defined( _X360 )
#ifndef _DEBUG
	PIXEndNamedEvent();
#endif
#else // PC
	if ( PIXError() )
		return;

#if !defined( NVPERFHUD )
	// Fire the PIX event, trapping for errors...
	if ( D3DPERF_EndEvent() < 0 )
	{
		Warning("PIX error ending event\n");
		IncrementPIXError();
	}
#endif
#endif
#endif // #if defined( PIX_INSTRUMENTATION )
}

#endif
	
void CShaderAPIDx8::AdvancePIXFrame()
{
#if defined( PIX_INSTRUMENTATION )
	// Ping PIX when this bool goes from false to true
	if ( r_pix_start.GetBool() && (!m_bPixCapturing) )
	{
		StartPIXInstrumentation();
		m_bPixCapturing = true;
	}

	// If we want to record frames...
	if ( r_pix_recordframes.GetInt() )
	{
		if ( m_nPixFrame == 0 )									// First frame to record
		{
			StartPIXInstrumentation();
			m_nPixFrame++;
		}
		else if( m_nPixFrame == r_pix_recordframes.GetInt() )	// Last frame to record
		{
			EndPIXInstrumentation();
			r_pix_recordframes.SetValue(0);
			m_nPixFrame = 0;
		}
		else
		{
			m_nPixFrame++;										// Recording frames...
		}
	}
#endif
}

// No begin-end for this...use this to put discrete markers in the PIX stream
void CShaderAPIDx8::SetPIXMarker( unsigned long color, const char* szName )
{
#if defined( PIX_INSTRUMENTATION )
	LOCK_SHADERAPI();

#if defined( DX_TO_GL_ABSTRACTION )
	if ( g_pShaderDeviceMgrDx8->m_pSetMarker )
	{
		wchar_t wszName[128];
		mbstowcs(wszName, szName, 128 );
		g_pShaderDeviceMgrDx8->m_pSetMarker( 0x2F2F2F2F, wszName );
	}
#elif defined( _X360 )
#ifndef _DEBUG
	char szPIXMarkerName[32];
	PIXifyName( szPIXMarkerName, szName );
	PIXSetMarker( color, szPIXMarkerName );
#endif
#else // PC
	if ( PIXError() )
		return;
	wchar_t wszName[128];
	mbstowcs(wszName, szName, 128 );
	D3DPERF_SetMarker( color, wszName );
#endif

#endif  // PIX_INSTRUMENTATION
}

void CShaderAPIDx8::StartPIXInstrumentation()
{
#if defined( PIX_INSTRUMENTATION )
	SetPIXMarker( PIX_VALVE_ORANGE, "Valve_PIX_Capture_Start" );
#endif
}

void CShaderAPIDx8::EndPIXInstrumentation()
{
#if defined( PIX_INSTRUMENTATION )
	SetPIXMarker( PIX_VALVE_ORANGE, "Valve_PIX_Capture_End" );
#endif
}

void CShaderAPIDx8::IncrementPIXError()
{
#if defined( PIX_INSTRUMENTATION ) && !defined( NVPERFHUD )
	m_nPIXErrorCount++;
	if ( m_nPIXErrorCount >= MAX_PIX_ERRORS )
	{
		Warning( "Source engine built with PIX instrumentation, but PIX doesn't seem to have been used to instantiate the game, which is necessary on PC.\n" );
	}
#endif
}

// Have we already hit several PIX errors?
bool CShaderAPIDx8::PIXError()
{
#if defined( PIX_INSTRUMENTATION ) && !defined( NVPERFHUD )
	return m_nPIXErrorCount >= MAX_PIX_ERRORS;
#else
	return false;
#endif
}


//-----------------------------------------------------------------------------
// Check for device lost
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ChangeVideoMode( const ShaderDeviceInfo_t &info )
{
	LOCK_SHADERAPI();

	m_PendingVideoModeChangeConfig = info;
	m_bPendingVideoModeChange = true;

	if ( info.m_DisplayMode.m_nWidth != 0 && info.m_DisplayMode.m_nHeight != 0 )
	{
		m_nWindowWidth = info.m_DisplayMode.m_nWidth;
		m_nWindowHeight = info.m_DisplayMode.m_nHeight;
	}
}


//-----------------------------------------------------------------------------
// Compute fill rate
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ComputeFillRate()
{
	static unsigned char* pBuf = 0;

	int width, height;
	GetWindowSize( width, height );
	// Snapshot; look at total # pixels drawn...
	if ( !pBuf )
	{
		intp memSize = ShaderUtil()->GetMemRequired(
									width, 
									height, 
									1, 
									IMAGE_FORMAT_RGB888, 
									false ) + 4;

		pBuf = (unsigned char*)malloc( memSize );
	}

	ReadPixels( 
		0, 
		0, 
		width, 
		height, 
		pBuf, 
		IMAGE_FORMAT_RGB888 );

	int mask = 0xFF;
	int count = 0;
	unsigned char* pRead = pBuf;
	for (int i = 0; i < height; ++i)
	{
		for (int j = 0; j < width; ++j)
		{
			int val = *(int*)pRead;
			count += (val & mask);
			pRead += 3;
		}
	}
}

//-----------------------------------------------------------------------------
// Use this to get the mesh builder that allows us to modify vertex data
//-----------------------------------------------------------------------------
CMeshBuilder* CShaderAPIDx8::GetVertexModifyBuilder()
{
	return &m_ModifyBuilder;
}

bool CShaderAPIDx8::InFlashlightMode() const
{
	return ShaderUtil()->InFlashlightMode();
}

bool CShaderAPIDx8::InEditorMode() const
{
	return ShaderUtil()->InEditorMode();
}

//-----------------------------------------------------------------------------
// Gets the bound morph's vertex format; returns 0 if no morph is bound
//-----------------------------------------------------------------------------
MorphFormat_t CShaderAPIDx8::GetBoundMorphFormat()
{
	return ShaderUtil()->GetBoundMorphFormat();
}

//-----------------------------------------------------------------------------
// returns the current time in seconds...
//-----------------------------------------------------------------------------
double CShaderAPIDx8::CurrentTime() const
{
	// FIXME: Return game time instead of real time!
	// Or eliminate this altogether and put it into a material var
	// (this is used by vertex modifiers in shader code at the moment)
	return Plat_FloatTime();
}


//-----------------------------------------------------------------------------
// Methods called by the transition table that use dynamic state...
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ApplyZBias( const ShadowState_t& shaderState )
{
	MaterialSystem_Config_t &config = ShaderUtil()->GetConfig();
    float a = (config.m_SlopeScaleDepthBias_Decal != 0.0f) ? 1.0f / config.m_SlopeScaleDepthBias_Decal : 0.0f;
    float b = (config.m_SlopeScaleDepthBias_Normal != 0.0f) ? 1.0f / config.m_SlopeScaleDepthBias_Normal : 0.0f;
    float c = (config.m_DepthBias_Decal != 0.0f) ? 1.0f / config.m_DepthBias_Decal : 0.0f;
    float d = (config.m_DepthBias_Normal != 0.0f) ? 1.0f / config.m_DepthBias_Normal : 0.0f;

	// FIXME: No longer necessary; may be necessary if you want to use cat 4.3 drivers?
	// GR - hack for R200
	bool bPS14Only = g_pHardwareConfig->Caps().m_SupportsPixelShaders_1_4 && !g_pHardwareConfig->Caps().m_SupportsPixelShaders_2_0;
	if( ( g_pHardwareConfig->Caps().m_VendorID == 0x1002 ) && bPS14Only )
	{
		// Slam to m_SlopeScaleDepthBias_Decal = 0, m_DepthBias_Decal = -4096
		// which empirically is what appears to look good on r200
		// NOTE: Slamming to 0 instead of -1.0 / 4096 because on Cat 4.9, WinXP, 8500,
		// this causes the z values to be more than 50 units away from the original z values

		a = 0.0f;
		c = -1.0/4096.0;
	}

	// bias = (s * D3DRS_SLOPESCALEDEPTHBIAS) + D3DRS_DEPTHBIAS, where s is the maximum depth slope of the triangle being rendered
    if ( g_pHardwareConfig->Caps().m_ZBiasAndSlopeScaledDepthBiasSupported )
    {
		float fSlopeScaleDepthBias, fDepthBias;
		if ( shaderState.m_ZBias == SHADER_POLYOFFSET_DECAL )
		{
			fSlopeScaleDepthBias = a;
			fDepthBias = c;
		}
		else if ( shaderState.m_ZBias == SHADER_POLYOFFSET_SHADOW_BIAS )
		{
			fSlopeScaleDepthBias = m_fShadowSlopeScaleDepthBias;
			fDepthBias = m_fShadowDepthBias;
		}
		else // assume SHADER_POLYOFFSET_DISABLE
		{
			fSlopeScaleDepthBias = b;
			fDepthBias = d;
		}

		if( ReverseDepthOnX360() )
		{
			fSlopeScaleDepthBias = -fSlopeScaleDepthBias;
			fDepthBias = -fDepthBias;
		}

		SetRenderStateConstMacro( this, D3DRS_SLOPESCALEDEPTHBIAS, *((DWORD*) (&fSlopeScaleDepthBias)) );
		SetRenderStateConstMacro( this, D3DRS_DEPTHBIAS, *((DWORD*) (&fDepthBias)) );
	} 
	else
	{
		MarkAllUserClipPlanesDirty();
		m_DynamicState.m_TransformChanged[MATERIAL_PROJECTION] |= 
			STATE_CHANGED_VERTEX_SHADER | STATE_CHANGED_FIXED_FUNCTION;
	}
}

void CShaderAPIDx8::ApplyTextureEnable( const ShadowState_t& state, int nSampler )
{
	if ( state.m_SamplerState[nSampler].m_TextureEnable == SamplerState(nSampler).m_TextureEnable )
		return;

	if ( state.m_SamplerState[nSampler].m_TextureEnable )
	{
		SamplerState( nSampler ).m_TextureEnable = true;

		// Should not be necessary/possible (SetTextureState() calls D3D9/DXAbstract, so the calling thread must already own the device.
		//LOCK_SHADERAPI();

		// Don't do this here!!  It ends up giving us extra texture sets.
		// We'll Assert in debug mode if you enable a texture stage
		// but don't bind a texture.
		// see CShaderAPIDx8::RenderPass() for this check.
		// NOTE: We aren't doing this optimization quite yet.  There are situations
		// where you want a texture stage enabled for its texture coordinates, but
		// you don't actually bind a texture (texmvspec for example.)
		SetTextureState( (Sampler_t)nSampler, SamplerState(nSampler).m_BoundTexture, true );
	}
	else
	{
		SamplerState( nSampler ).m_TextureEnable = false;
		SetTextureState( (Sampler_t)nSampler, INVALID_SHADERAPI_TEXTURE_HANDLE );
	}
}


//-----------------------------------------------------------------------------
// Used to clear the transition table when we know it's become invalid.
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ClearSnapshots()
{
	LOCK_SHADERAPI();
	FlushBufferedPrimitives();
	m_TransitionTable.Reset();
	InitRenderState();
}


//-----------------------------------------------------------------------------
// Gets the vertex format for a particular snapshot id
//-----------------------------------------------------------------------------
VertexFormat_t CShaderAPIDx8::ComputeVertexUsage( int num, StateSnapshot_t* pIds ) const
{
	LOCK_SHADERAPI();
	if (num == 0)
		return 0;

	// We don't have to all sorts of crazy stuff if there's only one snapshot
	if ( num == 1 )
	{
		const ShadowShaderState_t& state = m_TransitionTable.GetSnapshotShader( pIds[0] );
		return state.m_VertexUsage;
	}

	Assert( pIds );

	// Aggregating vertex formats is a little tricky;
	// For example, what do we do when two passes want user data? 
	// Can we assume they are the same? For now, I'm going to
	// just print a warning in debug.

	VertexCompressionType_t compression = VERTEX_COMPRESSION_INVALID;
	int userDataSize = 0;
	int numBones = 0;
	int texCoordSize[VERTEX_MAX_TEXTURE_COORDINATES] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	int flags = 0;

	for (int i = num; --i >= 0; )
	{
		const ShadowShaderState_t& state = m_TransitionTable.GetSnapshotShader( pIds[i] );
		VertexFormat_t fmt = state.m_VertexUsage;
		flags |= VertexFlags(fmt);

		VertexCompressionType_t newCompression = CompressionType( fmt );
		if ( ( compression != newCompression ) && ( compression != VERTEX_COMPRESSION_INVALID ) )
		{
			Warning("Encountered a material with two passes that specify different vertex compression types!\n");
			compression = VERTEX_COMPRESSION_NONE; // Be safe, disable compression
		}

		int newNumBones = NumBoneWeights(fmt);
		if ((numBones != newNumBones) && (newNumBones != 0))
		{
			if (numBones != 0)
			{
				Warning("Encountered a material with two passes that use different numbers of bones!\n");
			}
			numBones = newNumBones;
		}

		int newUserSize = UserDataSize(fmt);
		if ((userDataSize != newUserSize) && (newUserSize != 0))
		{
			if (userDataSize != 0)
			{
				Warning("Encountered a material with two passes that use different user data sizes!\n");
			}
			userDataSize = newUserSize;
		}

		for ( int j = 0; j < VERTEX_MAX_TEXTURE_COORDINATES; ++j )
		{
			int newSize = TexCoordSize( (TextureStage_t)j, fmt );
			if ( ( texCoordSize[j] != newSize ) && ( newSize != 0 ) )
			{
				if ( texCoordSize[j] != 0 ) 
				{
					Warning("Encountered a material with two passes that use different texture coord sizes!\n");
				}
				if ( texCoordSize[j] < newSize )
				{
					texCoordSize[j] = newSize;
				}
			}
		}
	}

	return MeshMgr()->ComputeVertexFormat( flags, VERTEX_MAX_TEXTURE_COORDINATES, 
		texCoordSize, numBones, userDataSize );
}

VertexFormat_t CShaderAPIDx8::ComputeVertexFormat( int num, StateSnapshot_t* pIds ) const
{
	LOCK_SHADERAPI();
	VertexFormat_t fmt = ComputeVertexUsage( num, pIds );
	return fmt;
}


//-----------------------------------------------------------------------------
// What fields in the morph do we actually use?
//-----------------------------------------------------------------------------
MorphFormat_t CShaderAPIDx8::ComputeMorphFormat( int numSnapshots, StateSnapshot_t* pIds ) const
{
	LOCK_SHADERAPI();
	MorphFormat_t format = 0;
	for ( int i = 0; i < numSnapshots; ++i )
	{
		MorphFormat_t fmt = m_TransitionTable.GetSnapshotShader( pIds[i] ).m_MorphUsage;
		format |= VertexFlags(fmt);
	}
	return format;
}

//-----------------------------------------------------------------------------
// Commits a range of vertex shader constants
//-----------------------------------------------------------------------------
static void CommitVertexShaderConstantRange( IDirect3DDevice9 *pDevice, const DynamicState_t &desiredState,
	DynamicState_t &currentState, bool bForce, int nFirstConstant, int nCount )
{
	int nFirstCommit = nFirstConstant;
	int nCommitCount = 0;

	for ( int i = 0; i < nCount; ++i )
	{
		int nVar = nFirstConstant + i; 

		bool bDifferentValue = bForce || ( desiredState.m_pVectorVertexShaderConstant[nVar] != currentState.m_pVectorVertexShaderConstant[nVar] );
		if ( !bDifferentValue )
		{
			if ( nCommitCount != 0 )
			{
				// flush the prior range
				pDevice->SetVertexShaderConstantF( nFirstCommit, desiredState.m_pVectorVertexShaderConstant[nFirstCommit].Base(), nCommitCount );

				memcpy( &currentState.m_pVectorVertexShaderConstant[nFirstCommit], 
					&desiredState.m_pVectorVertexShaderConstant[nFirstCommit], nCommitCount * 4 * sizeof(float) );
			}

			// start of new range
			nFirstCommit = nVar + 1;
			nCommitCount = 0;
		}
		else
		{
			++nCommitCount;
		}
	}

	if ( nCommitCount != 0 )
	{
		// flush range
		pDevice->SetVertexShaderConstantF( nFirstCommit, desiredState.m_pVectorVertexShaderConstant[nFirstCommit].Base(), nCommitCount );

		memcpy( &currentState.m_pVectorVertexShaderConstant[nFirstCommit], 
			&desiredState.m_pVectorVertexShaderConstant[nFirstCommit], nCommitCount * 4 * sizeof(float) );
	}
}


//-----------------------------------------------------------------------------
// Gets the current buffered state... (debug only)
//-----------------------------------------------------------------------------
void CShaderAPIDx8::GetBufferedState( BufferedState_t& state )
{
	memcpy( &state.m_Transform[0], &GetTransform(MATERIAL_MODEL), sizeof(state.m_Transform[0]) );
	memcpy( &state.m_Transform[1], &GetTransform(MATERIAL_VIEW), sizeof(state.m_Transform[1]) );
	memcpy( &state.m_Transform[2], &GetTransform(MATERIAL_PROJECTION), sizeof(state.m_Transform[2]) );
	memcpy( &state.m_Viewport, &m_DynamicState.m_Viewport, sizeof(state.m_Viewport) );
	state.m_PixelShader = ShaderManager()->GetCurrentPixelShader();
	state.m_VertexShader = ShaderManager()->GetCurrentVertexShader();
	for (int i = 0; i < g_pHardwareConfig->GetSamplerCount(); ++i)
	{
		state.m_BoundTexture[i] = m_DynamicState.m_SamplerState[i].m_BoundTexture;
	}
}


//-----------------------------------------------------------------------------
// constant color methods
//-----------------------------------------------------------------------------

void CShaderAPIDx8::Color3f( float r, float g, float b )
{
	unsigned int color = D3DCOLOR_ARGB( 255, (int)(r * 255), 
		(int)(g * 255), (int)(b * 255) );
	if (color != m_DynamicState.m_ConstantColor)
	{
		m_DynamicState.m_ConstantColor = color;
		SetSupportedRenderState( D3DRS_TEXTUREFACTOR, color );
	}
}

void CShaderAPIDx8::Color4f( float r, float g, float b, float a )
{
	unsigned int color = D3DCOLOR_ARGB( (int)(a * 255), (int)(r * 255), 
		(int)(g * 255), (int)(b * 255) );
	if (color != m_DynamicState.m_ConstantColor)
	{
		m_DynamicState.m_ConstantColor = color;
		SetSupportedRenderState( D3DRS_TEXTUREFACTOR, color );
	}
}

void CShaderAPIDx8::Color3fv( float const *c )
{
	Assert( c );
	unsigned int color = D3DCOLOR_ARGB( 255, (int)(c[0] * 255), 
		(int)(c[1] * 255), (int)(c[2] * 255) );
	if (color != m_DynamicState.m_ConstantColor)
	{
		m_DynamicState.m_ConstantColor = color;
		SetSupportedRenderState( D3DRS_TEXTUREFACTOR, color );
	}
}

void CShaderAPIDx8::Color4fv( float const *c )
{
	Assert( c );
	unsigned int color = D3DCOLOR_ARGB( (int)(c[3] * 255), (int)(c[0] * 255), 
		(int)(c[1] * 255), (int)(c[2] * 255) );
	if (color != m_DynamicState.m_ConstantColor)
	{
		m_DynamicState.m_ConstantColor = color;
		SetSupportedRenderState( D3DRS_TEXTUREFACTOR, color );
	}
}

void CShaderAPIDx8::Color3ub( unsigned char r, unsigned char g, unsigned char b )
{
	unsigned int color = D3DCOLOR_ARGB( 255, r, g, b ); 
	if (color != m_DynamicState.m_ConstantColor)
	{
		m_DynamicState.m_ConstantColor = color;
		SetSupportedRenderState( D3DRS_TEXTUREFACTOR, color );
	}
}

void CShaderAPIDx8::Color3ubv( unsigned char const* pColor )
{
	Assert( pColor );
	unsigned int color = D3DCOLOR_ARGB( 255, pColor[0], pColor[1], pColor[2] ); 
	if (color != m_DynamicState.m_ConstantColor)
	{
		m_DynamicState.m_ConstantColor = color;
		SetSupportedRenderState( D3DRS_TEXTUREFACTOR, color );
	}
}

void CShaderAPIDx8::Color4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{
	unsigned int color = D3DCOLOR_ARGB( a, r, g, b );
	if (color != m_DynamicState.m_ConstantColor)
	{
		m_DynamicState.m_ConstantColor = color;
		SetSupportedRenderState( D3DRS_TEXTUREFACTOR, color );
	}
}

void CShaderAPIDx8::Color4ubv( unsigned char const* pColor )
{
	Assert( pColor );
	unsigned int color = D3DCOLOR_ARGB( pColor[3], pColor[0], pColor[1], pColor[2] ); 
	if (color != m_DynamicState.m_ConstantColor)
	{
		m_DynamicState.m_ConstantColor = color;
		SetSupportedRenderState( D3DRS_TEXTUREFACTOR, color );
	}
}


//-----------------------------------------------------------------------------
// The shade mode
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ShadeMode( ShaderShadeMode_t mode )
{
	LOCK_SHADERAPI();
	D3DSHADEMODE shadeMode = (mode == SHADER_FLAT) ? D3DSHADE_FLAT : D3DSHADE_GOURAUD;
	if (m_DynamicState.m_ShadeMode != shadeMode)
	{
		m_DynamicState.m_ShadeMode = shadeMode;
		SetRenderStateConstMacro( this, D3DRS_SHADEMODE, shadeMode );
	}
}


//-----------------------------------------------------------------------------
// Buffering 2 frames ahead
//-----------------------------------------------------------------------------
void CShaderAPIDx8::EnableBuffer2FramesAhead( bool bEnable )
{
}

void CShaderAPIDx8::SetDepthFeatheringPixelShaderConstant( int iConstant, float fDepthBlendScale )
{
	float fConstantValues[4];

	fConstantValues[0] = m_DynamicState.m_DestAlphaDepthRange / fDepthBlendScale;
	fConstantValues[1] = fConstantValues[2] = fConstantValues[3] = 0.0f; //empty

	SetPixelShaderConstant( iConstant, fConstantValues );
}


//-----------------------------------------------------------------------------
// Cull mode..
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetCullModeState( bool bEnable, D3DCULL nDesiredCullMode )
{ 
	D3DCULL nCullMode = bEnable ? nDesiredCullMode : D3DCULL_NONE;
	if ( nCullMode != m_DynamicState.m_CullMode )
	{
		SetRenderStateConstMacro( this, D3DRS_CULLMODE, nCullMode );
		m_DynamicState.m_CullMode = nCullMode;
	}
}

void CShaderAPIDx8::ApplyCullEnable( bool bEnable )
{
	m_DynamicState.m_bCullEnabled = bEnable;
	SetCullModeState( m_DynamicState.m_bCullEnabled, m_DynamicState.m_DesiredCullMode );
}

void CShaderAPIDx8::CullMode( MaterialCullMode_t nCullMode )
{
	LOCK_SHADERAPI();
	D3DCULL nNewCullMode;
	switch( nCullMode )
	{
	case MATERIAL_CULLMODE_CCW:
		// Culls backfacing polys (normal)
		nNewCullMode = D3DCULL_CCW;
		break;

	case MATERIAL_CULLMODE_CW:
		// Culls frontfacing polys
		nNewCullMode = D3DCULL_CW;
		break;

	default:
		Warning( "CullMode: invalid cullMode\n" );
		return;
	}

	if (m_DynamicState.m_DesiredCullMode != nNewCullMode)
	{
		FlushBufferedPrimitives();
		m_DynamicState.m_DesiredCullMode = nNewCullMode;
		SetCullModeState( m_DynamicState.m_bCullEnabled, m_DynamicState.m_DesiredCullMode );
	}
}

static ConVar mat_alphacoverage( "mat_alphacoverage", "1" );
void CShaderAPIDx8::ApplyAlphaToCoverage( bool bEnable )
{
	if ( mat_alphacoverage.GetBool() )
	{
		if ( bEnable )
			EnableAlphaToCoverage();
		else
			DisableAlphaToCoverage();
	}
	else
	{
		DisableAlphaToCoverage();
	}
}

//-----------------------------------------------------------------------------
// Returns the current cull mode of the current material (for selection mode only)
//-----------------------------------------------------------------------------
D3DCULL CShaderAPIDx8::GetCullMode() const
{
	Assert( m_pMaterial );
	if ( m_pMaterial->GetMaterialVarFlag( MATERIAL_VAR_NOCULL ) )
		return D3DCULL_NONE;
	return m_DynamicState.m_DesiredCullMode;
}

void CShaderAPIDx8::SetRasterState( const ShaderRasterState_t& state )
{
	// FIXME: Implement!
}


void CShaderAPIDx8::ForceDepthFuncEquals( bool bEnable )
{
	LOCK_SHADERAPI();
	if ( !g_pShaderDeviceDx8->IsDeactivated() )
	{
		m_TransitionTable.ForceDepthFuncEquals( bEnable );
	}
}

void CShaderAPIDx8::OverrideDepthEnable( bool bEnable, bool bDepthEnable )
{
	LOCK_SHADERAPI();
	if ( !g_pShaderDeviceDx8->IsDeactivated() )
	{
		m_TransitionTable.OverrideDepthEnable( bEnable, bDepthEnable );
	}
}

void CShaderAPIDx8::OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable )
{
	LOCK_SHADERAPI();
	if ( !g_pShaderDeviceDx8->IsDeactivated() )
	{
		m_TransitionTable.OverrideAlphaWriteEnable( bOverrideEnable, bAlphaWriteEnable );
	}
}

void CShaderAPIDx8::OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable )
{
	LOCK_SHADERAPI();
	if ( !g_pShaderDeviceDx8->IsDeactivated() )
	{
		m_TransitionTable.OverrideColorWriteEnable( bOverrideEnable, bColorWriteEnable );
	}
}

void CShaderAPIDx8::UpdateFastClipUserClipPlane( void )
{
	float plane[4];
	switch( m_DynamicState.m_HeightClipMode )
	{
	case MATERIAL_HEIGHTCLIPMODE_DISABLE:
		EnableFastClip( false );
		break;
	case MATERIAL_HEIGHTCLIPMODE_RENDER_ABOVE_HEIGHT:
		plane[0] = 0.0f;
		plane[1] = 0.0f;
		plane[2] = 1.0f;
		plane[3] = m_DynamicState.m_HeightClipZ;
		EnableFastClip( true );
		SetFastClipPlane(plane);
		break;
	case MATERIAL_HEIGHTCLIPMODE_RENDER_BELOW_HEIGHT:
		plane[0] = 0.0f;
		plane[1] = 0.0f;
		plane[2] = -1.0f;
		plane[3] = -m_DynamicState.m_HeightClipZ;
		EnableFastClip( true );
		SetFastClipPlane(plane);
		break;
	}
}

void CShaderAPIDx8::SetHeightClipZ( float z )
{
	LOCK_SHADERAPI();
	if( z != m_DynamicState.m_HeightClipZ )
	{
		FlushBufferedPrimitives();
		m_DynamicState.m_HeightClipZ = z;
		UpdateVertexShaderFogParams();
		UpdateFastClipUserClipPlane();
		m_DynamicState.m_TransformChanged[MATERIAL_PROJECTION] |= 
			STATE_CHANGED_VERTEX_SHADER | STATE_CHANGED_FIXED_FUNCTION;
	}
}

void CShaderAPIDx8::SetHeightClipMode( MaterialHeightClipMode_t heightClipMode )
{
	LOCK_SHADERAPI();
	if( heightClipMode != m_DynamicState.m_HeightClipMode )
	{
		FlushBufferedPrimitives();
		m_DynamicState.m_HeightClipMode = heightClipMode;
		UpdateVertexShaderFogParams();
		UpdateFastClipUserClipPlane();
		m_DynamicState.m_TransformChanged[MATERIAL_PROJECTION] |= 
			STATE_CHANGED_VERTEX_SHADER | STATE_CHANGED_FIXED_FUNCTION;
	}
}

void CShaderAPIDx8::SetClipPlane( int index, const float *pPlane )
{
	LOCK_SHADERAPI();
	Assert( index < g_pHardwareConfig->MaxUserClipPlanes() && index >= 0 );

	// NOTE: The plane here is specified in *world space*
	// NOTE: This is done because they assume Ax+By+Cz+Dw = 0 (where w = 1 in real space)
	// while we use Ax+By+Cz=D
	DirectX::XMVECTOR plane{ DirectX::XMVectorSet(pPlane[0], pPlane[1], pPlane[2], -pPlane[3]) };

	if ( memcmp( &plane, &m_DynamicState.m_UserClipPlaneWorld[index], sizeof(plane) ) != 0 )
	{
		FlushBufferedPrimitives();

		m_DynamicState.m_UserClipPlaneChanged |= ( 1 << index );
		m_DynamicState.m_UserClipPlaneWorld[index] = plane;
	}
}


//-----------------------------------------------------------------------------
// Converts a DirectX::XMMATRIX to a VMatrix and back
//-----------------------------------------------------------------------------
void XM_CALLCONV CShaderAPIDx8::XMMatrixToVMatrix( DirectX::FXMMATRIX in, VMatrix& out )
{
	DirectX::XMMATRIX result = DirectX::XMMatrixTranspose( in );
	DirectX::XMStoreFloat4x4( out.XmMBase(), result );
}

DirectX::XMMATRIX XM_CALLCONV CShaderAPIDx8::VMatrixToXMMatrix( const VMatrix& in )
{
	DirectX::XMMATRIX result = DirectX::XMMatrixTranspose( DirectX::XMLoadFloat4x4( in.XmMBase() ) );
	return result;
}

	
//-----------------------------------------------------------------------------
// Mark all user clip planes as being dirty
//-----------------------------------------------------------------------------
void CShaderAPIDx8::MarkAllUserClipPlanesDirty()
{
	m_DynamicState.m_UserClipPlaneChanged |= ( 1 << g_pHardwareConfig->MaxUserClipPlanes() ) - 1;
	m_DynamicState.m_bFastClipPlaneChanged = true;
}


//-----------------------------------------------------------------------------
// User clip plane override
//-----------------------------------------------------------------------------
void CShaderAPIDx8::EnableUserClipTransformOverride( bool bEnable )
{
	LOCK_SHADERAPI();
	if ( m_DynamicState.m_bUserClipTransformOverride != bEnable )
	{
		FlushBufferedPrimitives();
		m_DynamicState.m_bUserClipTransformOverride = bEnable;
		MarkAllUserClipPlanesDirty();
	}
}


//-----------------------------------------------------------------------------
// Specify user clip transform
//-----------------------------------------------------------------------------
void CShaderAPIDx8::UserClipTransform( const VMatrix &worldToProjection )
{
	LOCK_SHADERAPI();
	DirectX::XMMATRIX dxWorldToProjection{ VMatrixToXMMatrix( worldToProjection ) };

	if ( memcmp( &m_DynamicState.m_UserClipTransform, &dxWorldToProjection, sizeof(dxWorldToProjection) ) != 0 )
	{
		m_DynamicState.m_UserClipTransform = dxWorldToProjection;
		if ( m_DynamicState.m_bUserClipTransformOverride )
		{
			FlushBufferedPrimitives();
			MarkAllUserClipPlanesDirty();
		}
	}
}


//-----------------------------------------------------------------------------
// Enables a user clip plane
//-----------------------------------------------------------------------------
void CShaderAPIDx8::EnableClipPlane( int index, bool bEnable )
{
	LOCK_SHADERAPI();
	Assert( index < g_pHardwareConfig->MaxUserClipPlanes() && index >= 0 );
	if( ( m_DynamicState.m_UserClipPlaneEnabled & ( 1 << index ) ? true : false ) != bEnable )
	{
		FlushBufferedPrimitives();
		if( bEnable )
		{
			m_DynamicState.m_UserClipPlaneEnabled |= ( 1 << index );
		}
		else
		{
			m_DynamicState.m_UserClipPlaneEnabled &= ~( 1 << index );
		}
		SetRenderStateConstMacro( this, D3DRS_CLIPPLANEENABLE, m_DynamicState.m_UserClipPlaneEnabled );
	}
}

//-----------------------------------------------------------------------------
// Recomputes the fast-clip plane matrices based on the current fast-clip plane
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitFastClipPlane( )
{
	// Don't bother recomputing if unchanged or disabled
	if ( !m_DynamicState.m_bFastClipPlaneChanged || !m_DynamicState.m_FastClipEnabled )
		return;

	m_DynamicState.m_bFastClipPlaneChanged = false;

	m_CachedFastClipProjectionMatrix = DirectX::XMMatrixIdentity();

	// Compute worldToProjection - need inv. transpose for transforming plane.
	DirectX::XMMATRIX viewToProj = GetTransform(MATERIAL_PROJECTION);
	// pull in zNear because the shear in effect 
	// moves it out: clipping artifacts when looking down at water 
	// could occur if this multiply is not done
	viewToProj.r[3] = DirectX::XMVectorSetZ( viewToProj.r[3], DirectX::XMVectorGetZ( DirectX::XMVectorScale( viewToProj.r[3], 0.5f ) ) );

	DirectX::XMMATRIX worldToView = GetUserClipTransform();
	DirectX::XMMATRIX worldToViewInv = DirectX::XMMatrixInverse( nullptr, worldToView );
	DirectX::XMMATRIX worldToViewInvTrans = DirectX::XMMatrixTranspose( worldToViewInv );

	DirectX::XMMATRIX viewToProjInv = DirectX::XMMatrixInverse( nullptr, viewToProj );
	DirectX::XMMATRIX viewToProjInvTrans = DirectX::XMMatrixTranspose( viewToProjInv );

	DirectX::XMVECTOR  plane = DirectX::XMPlaneNormalizeEst( m_DynamicState.m_FastClipPlane );

	// transform clip plane into view space
	plane = DirectX::XMVector4Transform( plane, worldToViewInvTrans );

	// transform clip plane into projection space
	plane = DirectX::XMVector4Transform( plane, viewToProjInvTrans );
	
#define ALLOW_FOR_FASTCLIPDUMPS 0

#if (ALLOW_FOR_FASTCLIPDUMPS == 1)
	static ConVar shader_dumpfastclipprojectioncoords( "shader_dumpfastclipprojectioncoords", "0", 0, "dump fast clip projected matrix" );
	if( shader_dumpfastclipprojectioncoords.GetBool() )
		DevMsg( "Fast clip plane projected coordinates: %f %f %f %f", clipPlane.x, clipPlane.y, clipPlane.z, clipPlane.w );
#endif
	
	if( DirectX::XMVectorGetZ( plane ) * DirectX::XMVectorGetW( plane ) <= -0.4f ) // a plane with (z*w) > -0.4 at this point is behind the camera and will cause graphical glitches. Toss it. (0.4 found through experimentation)
	{
#if (ALLOW_FOR_FASTCLIPDUMPS == 1)
		if( shader_dumpfastclipprojectioncoords.GetBool() )
			DevMsg( "    %f %f %f %f\n",
				DirectX::XMVectorGetX( plane ),
				DirectX::XMVectorGetY( plane ),
				DirectX::XMVectorGetZ( plane ),
				DirectX::XMVectorGetW( plane ) );
#endif

		plane = DirectX::XMVector4NormalizeEst( plane );

		//if ((fabs(clipPlane.z) > 0.01) && (fabs(clipPlane.w) > 0.01f))  
		{
			// put projection space clip plane in Z column
			DirectX::XMVectorSetZ( m_CachedFastClipProjectionMatrix.r[0], DirectX::XMVectorGetX( plane ) );
			DirectX::XMVectorSetZ( m_CachedFastClipProjectionMatrix.r[1], DirectX::XMVectorGetY( plane ) );
			DirectX::XMVectorSetZ( m_CachedFastClipProjectionMatrix.r[2], DirectX::XMVectorGetZ( plane ) );
			DirectX::XMVectorSetZ( m_CachedFastClipProjectionMatrix.r[3], DirectX::XMVectorGetW( plane ) );
		}
	}
#if (ALLOW_FOR_FASTCLIPDUMPS == 1)
	else
	{
		if( shader_dumpfastclipprojectioncoords.GetBool() )
			DevMsg( "\n" ); //finish off the line above
	}
#endif

	m_CachedFastClipProjectionMatrix = viewToProj * m_CachedFastClipProjectionMatrix;

	// Update the cached polyoffset matrix (with clip) too:
	ComputePolyOffsetMatrix( m_CachedFastClipProjectionMatrix, m_CachedFastClipPolyOffsetProjectionMatrix );
}

//-----------------------------------------------------------------------------
// Sets the fast-clip plane; but doesn't update the matrices
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetFastClipPlane( const float *pPlane )
{	
	LOCK_SHADERAPI();
	DirectX::XMVECTOR plane{ pPlane[0], pPlane[1], pPlane[2], -pPlane[3] };
	if ( memcmp( &plane, &m_DynamicState.m_FastClipPlane, sizeof(plane) ) != 0 )
	{
		FlushBufferedPrimitives();		
		UpdateVertexShaderFogParams();

		m_DynamicState.m_FastClipPlane = plane;

		// Mark a dirty bit so when it comes time to commit view + projection transforms,
		// we also update the fast clip matrices
		m_DynamicState.m_bFastClipPlaneChanged = true;

		m_DynamicState.m_TransformChanged[MATERIAL_PROJECTION] |= 
			STATE_CHANGED_VERTEX_SHADER | STATE_CHANGED_FIXED_FUNCTION;
	}
}


//-----------------------------------------------------------------------------
// Enables/disables fast-clip mode
//-----------------------------------------------------------------------------
void CShaderAPIDx8::EnableFastClip( bool bEnable )
{
	LOCK_SHADERAPI();
	if( m_DynamicState.m_FastClipEnabled != bEnable )
	{
		FlushBufferedPrimitives();
		UpdateVertexShaderFogParams();

		m_DynamicState.m_FastClipEnabled = bEnable;

		m_DynamicState.m_TransformChanged[MATERIAL_PROJECTION] |= 
			STATE_CHANGED_VERTEX_SHADER | STATE_CHANGED_FIXED_FUNCTION;
	}
}


//-----------------------------------------------------------------------------
// Vertex blending
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetVertexBlendState( int numBones )
{
	if (numBones < 0)
	{
		numBones = m_DynamicState.m_NumBones;
	}

	// For fixed-function, the number of weights is actually one less than
	// the number of bones
	if (numBones > 0)
		--numBones;

	bool normalizeNormals = true;
	D3DVERTEXBLENDFLAGS vertexBlend;
	switch(numBones)
	{
	case 0:
		vertexBlend = D3DVBF_DISABLE;
		normalizeNormals = false;
		break;

	case 1:
		vertexBlend = D3DVBF_1WEIGHTS;
		break;

	case 2:
		vertexBlend = D3DVBF_2WEIGHTS;
		break;

	case 3:
		vertexBlend = D3DVBF_3WEIGHTS;
		break;

	default:
		vertexBlend = D3DVBF_DISABLE;
		Assert(0);
		break;
	}

	if (m_DynamicState.m_VertexBlend != vertexBlend)
	{
		m_DynamicState.m_VertexBlend  = vertexBlend;
		SetSupportedRenderState( D3DRS_VERTEXBLEND, vertexBlend );
	}

	// Activate normalize normals when skinning is on
	if (m_DynamicState.m_NormalizeNormals != normalizeNormals)
	{
		m_DynamicState.m_NormalizeNormals  = normalizeNormals;
		SetSupportedRenderState( D3DRS_NORMALIZENORMALS, normalizeNormals );
	}
}


//-----------------------------------------------------------------------------
// Vertex blending
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetNumBoneWeights( int numBones )
{
	LOCK_SHADERAPI();
	if (m_DynamicState.m_NumBones != numBones)
	{
		FlushBufferedPrimitives();
		m_DynamicState.m_NumBones = numBones;

		if ( m_TransitionTable.CurrentShadowState() )
		{
			SetVertexBlendState( m_TransitionTable.CurrentShadowState()->m_VertexBlendEnable ? -1 : 0 );
		}
	}
}

void CShaderAPIDx8::EnableHWMorphing( bool bEnable )
{
	LOCK_SHADERAPI();
	if ( m_DynamicState.m_bHWMorphingEnabled != bEnable )
	{
		FlushBufferedPrimitives();
		m_DynamicState.m_bHWMorphingEnabled = bEnable;
	}
}

void CShaderAPIDx8::EnabledSRGBWrite( bool bEnabled )
{
	m_DynamicState.m_bSRGBWritesEnabled = bEnabled;

	if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
	{
		UpdatePixelFogColorConstant();

		if ( bEnabled && g_pHardwareConfig->NeedsShaderSRGBConversion() )
			BindTexture( SHADER_SAMPLER15, m_hLinearToGammaTableTexture );
		else
			BindTexture( SHADER_SAMPLER15, m_hLinearToGammaTableIdentityTexture );
	}
}

//-----------------------------------------------------------------------------
// Fog methods...
//-----------------------------------------------------------------------------
void CShaderAPIDx8::UpdatePixelFogColorConstant( void )
{
	Assert( HardwareConfig()->SupportsPixelShaders_2_b() );
	float fogColor[4];

	switch( GetPixelFogMode() )
	{
	case MATERIAL_FOG_NONE:
		{
			for( int i = 0; i != 3; ++i )
				fogColor[i] = 0.0f;
		}
		break;

	case MATERIAL_FOG_LINEAR:
		{
			//setup the fog for mixing linear fog in the pixel shader so that it emulates ff range fog
			for( int i = 0; i != 3; ++i )
				fogColor[i] = m_DynamicState.m_PixelFogColor[i];

			if( m_DynamicState.m_bSRGBWritesEnabled ) 
			{
				//since the fog color will assuredly get converted from linear to gamma, we should probably convert it from gamma to linear
				for( int i = 0; i != 3; ++i )
					fogColor[i] = GammaToLinear_HardwareSpecific( fogColor[i] );
			}

			if( (!m_DynamicState.m_bFogGammaCorrectionDisabled) && (g_pHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER) )
			{
				for( int i = 0; i != 3; ++i )
					fogColor[i] *= m_ToneMappingScale.x; // Linear
			}
		}
		break;

	case MATERIAL_FOG_LINEAR_BELOW_FOG_Z:
		{
			//water fog has been around a while and has never tonemap scaled, and has always been in linear space
			if( g_pHardwareConfig->NeedsShaderSRGBConversion() )
			{
				//srgb in ps2b uses the 2.2 curve
				for( int i = 0; i != 3; ++i )
					fogColor[i] = powf( m_DynamicState.m_PixelFogColor[i], 2.2f );
			}
			else
			{
				for( int i = 0; i != 3; ++i )
					fogColor[i] = GammaToLinear_HardwareSpecific( m_DynamicState.m_PixelFogColor[i] ); //this is how water fog color has always been setup in the past
			}

			if( (!m_DynamicState.m_bFogGammaCorrectionDisabled) && (g_pHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER) )
			{
				for( int i = 0; i != 3; ++i )
					fogColor[i] *= m_ToneMappingScale.x; // Linear
			}
		}
		break;

		NO_DEFAULT;
	};	

	fogColor[3] = 1.0f / m_DynamicState.m_DestAlphaDepthRange;

	SetPixelShaderConstant( LINEAR_FOG_COLOR, fogColor );
}


void CShaderAPIDx8::ApplyFogMode( ShaderFogMode_t fogMode, bool bSRGBWritesEnabled, bool bDisableFogGammaCorrection )
{
	HDRType_t hdrType = g_pHardwareConfig->GetHDRType();

	if ( fogMode == SHADER_FOGMODE_DISABLED )
	{
		if( hdrType != HDR_TYPE_FLOAT )
		{
			FogMode( MATERIAL_FOG_NONE );
		}

		if( m_DelayedShaderConstants.iPixelShaderFogParams != -1 )
			SetPixelShaderFogParams( m_DelayedShaderConstants.iPixelShaderFogParams, fogMode );

		return;
	}
	
	bool bShouldGammaCorrect = true;			// By default, we'll gamma correct.
	unsigned char r = 0, g = 0, b = 0;			// Black fog

	
	if( hdrType != HDR_TYPE_FLOAT )
	{
		FogMode( m_SceneFogMode );
	}

	if( m_DelayedShaderConstants.iPixelShaderFogParams != -1 )
		SetPixelShaderFogParams( m_DelayedShaderConstants.iPixelShaderFogParams, fogMode );

	switch( fogMode )
	{
		case SHADER_FOGMODE_BLACK:				// Additive decals
			bShouldGammaCorrect = false;
			break;
		case SHADER_FOGMODE_OO_OVERBRIGHT:
		case SHADER_FOGMODE_GREY:				// Mod2x decals
			r = g = b = 128;
			break;
		case SHADER_FOGMODE_WHITE:				// Multiplicative decals
			r = g = b = 255;
			bShouldGammaCorrect = false;
			break;
		case SHADER_FOGMODE_FOGCOLOR:
			GetSceneFogColor( &r, &g, &b );		// Scene fog color
			break;
		NO_DEFAULT
	}

	bShouldGammaCorrect &= !bDisableFogGammaCorrection;
	m_DynamicState.m_bFogGammaCorrectionDisabled = !bShouldGammaCorrect;

	D3DCOLOR color;
	if ( bShouldGammaCorrect )
	{
		color = ComputeGammaCorrectedFogColor( r, g, b, bSRGBWritesEnabled );
	}
	else
	{
		color = D3DCOLOR_ARGB( 255, r, g, b );
	}


	const float fColorScale = 1.0f / 255.0f;
	m_DynamicState.m_PixelFogColor[0] = (float)(r) * fColorScale;
	m_DynamicState.m_PixelFogColor[1] = (float)(g) * fColorScale;
	m_DynamicState.m_PixelFogColor[2] = (float)(b) * fColorScale;

	if( g_pMaterialSystemHardwareConfig->SupportsPixelShaders_2_b() )
	{
		UpdatePixelFogColorConstant();
	}

	if ( color != m_DynamicState.m_FogColor )
	{
		if( hdrType != HDR_TYPE_FLOAT )
		{
			m_DynamicState.m_FogColor = color;
			SetRenderStateConstMacro( this, D3DRS_FOGCOLOR, m_DynamicState.m_FogColor );
		}
	}
}

void CShaderAPIDx8::SceneFogMode( MaterialFogMode_t fogMode )
{
	LOCK_SHADERAPI();
	if( m_SceneFogMode != fogMode )
	{
		FlushBufferedPrimitives();
		m_SceneFogMode = fogMode;

		if ( m_TransitionTable.CurrentShadowState() )
		{
			// Get the shadow state in sync since it depends on SceneFogMode.
			ApplyFogMode( m_TransitionTable.CurrentShadowState()->m_FogMode, m_TransitionTable.CurrentShadowState()->m_SRGBWriteEnable, m_TransitionTable.CurrentShadowState()->m_bDisableFogGammaCorrection );
		}
	}
}

MaterialFogMode_t CShaderAPIDx8::GetSceneFogMode()
{
	return m_SceneFogMode;
}

MaterialFogMode_t CShaderAPIDx8::GetPixelFogMode()
{
	if( ShouldUsePixelFogForMode( m_SceneFogMode ) )
		return m_SceneFogMode;
	else
		return MATERIAL_FOG_NONE;
}

int CShaderAPIDx8::GetPixelFogCombo( void )
{
	if( (m_SceneFogMode != MATERIAL_FOG_NONE) && ShouldUsePixelFogForMode( m_SceneFogMode ) )
		return m_SceneFogMode - 1; //PIXELFOGTYPE dynamic combos are shifted down one. MATERIAL_FOG_NONE is simulated by range fog with rigged parameters. Gets rid of a dynamic combo across the ps2x set
	else	
		return MATERIAL_FOG_NONE;
}


static ConVar r_pixelfog( "r_pixelfog", "1" );

bool CShaderAPIDx8::ShouldUsePixelFogForMode( MaterialFogMode_t fogMode )
{
	if( fogMode == MATERIAL_FOG_NONE )
		return false;

	if( IsPosix() ) // Always use pixel fog on Posix
		return true;

	if( g_pHardwareConfig->Caps().m_nDXSupportLevel < 90 ) //pixel fog not available until at least ps2.0
		return false;

	switch( m_SceneFogMode )
	{
		case MATERIAL_FOG_LINEAR:
			return (g_pHardwareConfig->SupportsPixelShaders_2_b() && //lightmappedgeneric_ps20.fxc can't handle the instruction count
						r_pixelfog.GetBool()); //use pixel fog if preferred

		case MATERIAL_FOG_LINEAR_BELOW_FOG_Z:
			return true;
	};

	return false;
}

//-----------------------------------------------------------------------------
// Fog methods...
//-----------------------------------------------------------------------------
void CShaderAPIDx8::FogMode( MaterialFogMode_t fogMode )
{
	bool bFogEnable;

	m_DynamicState.m_SceneFog = fogMode;
	switch( fogMode )
	{
	default:
		Assert( 0 );
		// fall through
		[[fallthrough]];

	case MATERIAL_FOG_NONE:
		bFogEnable = false;
		break;

	case MATERIAL_FOG_LINEAR:
		// use vertex fog to achieve linear range fog
		bFogEnable = true;
		break;

	case MATERIAL_FOG_LINEAR_BELOW_FOG_Z:
		// use pixel fog on 9.0 and greater for height fog
		bFogEnable = g_pHardwareConfig->Caps().m_nDXSupportLevel < 90;
		break;
	}

	if( ShouldUsePixelFogForMode( fogMode ) )
	{
		bFogEnable = false; //disable FF fog when doing fog in the pixel shader
	}

	// These two are always set to this, so don't bother setting them.
	// We are always using vertex fog.
//	SetRenderState( D3DRS_FOGTABLEMODE, D3DFOG_NONE );
//	SetRenderState( D3DRS_RANGEFOGENABLE, false );

	// Set fog enable if it's different than before.
	if ( bFogEnable != m_DynamicState.m_FogEnable )
	{
		SetSupportedRenderState( D3DRS_FOGENABLE, bFogEnable );
		m_DynamicState.m_FogEnable = bFogEnable;
	}
}


void CShaderAPIDx8::FogStart( float fStart )
{
	LOCK_SHADERAPI();
	if (fStart != m_DynamicState.m_FogStart)
	{
		// need to flush the dynamic buffer
		FlushBufferedPrimitives();

		SetRenderStateConstMacro( this, D3DRS_FOGSTART, *((DWORD*)(&fStart)));
		m_VertexShaderFogParams[0] = fStart;
		UpdateVertexShaderFogParams();
		m_DynamicState.m_FogStart = fStart;
	}
}

void CShaderAPIDx8::FogEnd( float fEnd )
{
	LOCK_SHADERAPI();
	if (fEnd != m_DynamicState.m_FogEnd)
	{
		// need to flush the dynamic buffer
		FlushBufferedPrimitives();

		SetRenderStateConstMacro( this, D3DRS_FOGEND, *((DWORD*)(&fEnd)));
		m_VertexShaderFogParams[1] = fEnd;
		UpdateVertexShaderFogParams();
		m_DynamicState.m_FogEnd = fEnd;
	}
}

void CShaderAPIDx8::SetFogZ( float fogZ )
{
	LOCK_SHADERAPI();
	if (fogZ != m_DynamicState.m_FogZ)
	{
		// need to flush the dynamic buffer
		FlushBufferedPrimitives();
		m_DynamicState.m_FogZ = fogZ;
		UpdateVertexShaderFogParams();
	}
}

void CShaderAPIDx8::FogMaxDensity( float flMaxDensity )
{
	LOCK_SHADERAPI();
	if (flMaxDensity != m_DynamicState.m_FogMaxDensity)
	{
		// need to flush the dynamic buffer
		FlushBufferedPrimitives();

//		SetRenderState(D3DRS_FOGDENSITY, *((DWORD*)(&flMaxDensity)));  // ??? do I need to to this ???
		m_flFogMaxDensity = flMaxDensity;
		UpdateVertexShaderFogParams();
		m_DynamicState.m_FogMaxDensity = flMaxDensity;
	}
}

void CShaderAPIDx8::GetFogDistances( float *fStart, float *fEnd, float *fFogZ )
{
	LOCK_SHADERAPI();
	if( fStart )
		*fStart = m_DynamicState.m_FogStart;

	if( fEnd )
		*fEnd = m_DynamicState.m_FogEnd;

	if( fFogZ )
		*fFogZ = m_DynamicState.m_FogZ;
}

void CShaderAPIDx8::SceneFogColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
	LOCK_SHADERAPI();
	if( m_SceneFogColor[0] != r || m_SceneFogColor[1] != g || m_SceneFogColor[2] != b )
	{
		FlushBufferedPrimitives();
		m_SceneFogColor[0] = r;
		m_SceneFogColor[1] = g;
		m_SceneFogColor[2] = b;

		if ( m_TransitionTable.CurrentShadowState() )
		{
			ApplyFogMode( m_TransitionTable.CurrentShadowState()->m_FogMode, m_TransitionTable.CurrentShadowState()->m_SRGBWriteEnable, m_TransitionTable.CurrentShadowState()->m_bDisableFogGammaCorrection );
		}
	}
}

void CShaderAPIDx8::GetSceneFogColor( unsigned char *rgb )
{
	LOCK_SHADERAPI();
	rgb[0] = m_SceneFogColor[0];
	rgb[1] = m_SceneFogColor[1];
	rgb[2] = m_SceneFogColor[2];
}

void CShaderAPIDx8::GetSceneFogColor( unsigned char *r, unsigned char *g, unsigned char *b )
{
	*r = m_SceneFogColor[0];
	*g = m_SceneFogColor[1];
	*b = m_SceneFogColor[2];
}


//-----------------------------------------------------------------------------
// Gamma correction of fog color, or not...
//-----------------------------------------------------------------------------
D3DCOLOR CShaderAPIDx8::ComputeGammaCorrectedFogColor( unsigned char r, unsigned char g, unsigned char b, bool bSRGBWritesEnabled )
{
#ifdef _DEBUG
	if( g_pHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT && !bSRGBWritesEnabled )
	{
//		Assert( 0 );
	}
#endif
	bool bLinearSpace =   g_pHardwareConfig->Caps().m_bFogColorAlwaysLinearSpace ||
						( bSRGBWritesEnabled && ( g_pHardwareConfig->Caps().m_bFogColorSpecifiedInLinearSpace || g_pHardwareConfig->GetHDRType() == HDR_TYPE_FLOAT ) );

	bool bScaleFogByToneMappingScale = g_pHardwareConfig->GetHDRType() == HDR_TYPE_INTEGER;

	float fr = ( r / 255.0f );
	float fg = ( g / 255.0f );
	float fb = ( b / 255.0f );
	if ( bLinearSpace )
	{
		fr = GammaToLinear( fr ); 
		fg = GammaToLinear( fg ); 
		fb = GammaToLinear( fb ); 
		if ( bScaleFogByToneMappingScale )
		{
			fr *= m_ToneMappingScale.x;		//
			fg *= m_ToneMappingScale.x;		// Linear
			fb *= m_ToneMappingScale.x;		//
		}
	}
	else if ( bScaleFogByToneMappingScale )
	{
		fr *= m_ToneMappingScale.w;			//
		fg *= m_ToneMappingScale.w;			// Gamma
		fb *= m_ToneMappingScale.w;			//
	}

	fr = min( fr, 1.0f );
	fg = min( fg, 1.0f );
	fb = min( fb, 1.0f );
	r = (unsigned char)( fr * 255 );
	g = (unsigned char)( fg * 255 );
	b = (unsigned char)( fb * 255 );
	return D3DCOLOR_ARGB( 255, r, g, b ); 
}


//-----------------------------------------------------------------------------
// Some methods chaining vertex + pixel shaders through to the shader manager
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetVertexShaderIndex( int vshIndex )
{
	ShaderManager()->SetVertexShaderIndex( vshIndex );
}

void CShaderAPIDx8::SetPixelShaderIndex( int pshIndex )
{
	ShaderManager()->SetPixelShaderIndex( pshIndex );
}

void CShaderAPIDx8::SyncToken( const char *pToken )
{
	LOCK_SHADERAPI();
	RECORD_COMMAND( DX8_SYNC_TOKEN, 1 );
	RECORD_STRING( pToken );
}


//-----------------------------------------------------------------------------
// Deals with vertex buffers
//-----------------------------------------------------------------------------
void CShaderAPIDx8::DestroyVertexBuffers( bool bExitingLevel )
{
	LOCK_SHADERAPI();
	MeshMgr()->DestroyVertexBuffers( );
	// After a map is shut down, we switch to using smaller dynamic VBs
	// (VGUI shouldn't need much), so that we have more memory free during map loading
	m_nDynamicVBSize = bExitingLevel ? DYNAMIC_VERTEX_BUFFER_MEMORY_SMALL : DYNAMIC_VERTEX_BUFFER_MEMORY;
}

int CShaderAPIDx8::GetCurrentDynamicVBSize( void )
{
	return m_nDynamicVBSize;
}

FORCEINLINE void CShaderAPIDx8::SetVertexShaderConstantInternal( int var, float const* pVec, int numVecs, bool bForce )
{
	Assert( pVec );

	// DX8 asm shaders use a constant mapping which has transforms and vertex shader
	// specific constants shifted down by 10 constants (two 5-constant light structures)
	if ( (g_pHardwareConfig->Caps().m_nDXSupportLevel < 90) && (var >= VERTEX_SHADER_MODULATION_COLOR) )
	{
		var -= 10;
	}
	Assert( var + numVecs <= g_pHardwareConfig->NumVertexShaderConstants() );

	if ( !bForce )
	{
		int skip = 0;
		numVecs = AdjustUpdateRange( pVec, &m_DesiredState.m_pVectorVertexShaderConstant[var], numVecs, &skip );
		if ( !numVecs )
			return;
		var += skip;
		pVec += skip * 4;
	}
	Dx9Device()->SetVertexShaderConstantF( var, pVec, numVecs );
	memcpy( &m_DynamicState.m_pVectorVertexShaderConstant[var], pVec, numVecs * 4 * sizeof(float) );

	memcpy( &m_DesiredState.m_pVectorVertexShaderConstant[var], pVec, numVecs * 4 * sizeof(float) );	
}


//-----------------------------------------------------------------------------
// Sets the constant register for vertex and pixel shaders
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetVertexShaderConstant( int var, float const* pVec, int numVecs, bool bForce )
{
	SetVertexShaderConstantInternal( var, pVec, numVecs, bForce );
}

//-----------------------------------------------------------------------------
// Sets the boolean registers for vertex shader control flow
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetBooleanVertexShaderConstant( int var, int const* pVec, int numBools, bool bForce )
{
	Assert( pVec );
	Assert( var + numBools <= g_pHardwareConfig->NumBooleanVertexShaderConstants() );

	if ( g_pHardwareConfig->GetDXSupportLevel() < 90 )
	{
		return;
	}

	if ( !bForce && memcmp( pVec, &m_DesiredState.m_pBooleanVertexShaderConstant[var], numBools * sizeof( BOOL ) ) == 0 )
	{
		return;
	}

	Dx9Device()->SetVertexShaderConstantB( var, pVec, numBools );
	memcpy( &m_DynamicState.m_pBooleanVertexShaderConstant[var], pVec, numBools * sizeof(BOOL) );
	memcpy( &m_DesiredState.m_pBooleanVertexShaderConstant[var], pVec, numBools * sizeof(BOOL) );
}


//-----------------------------------------------------------------------------
// Sets the integer registers for vertex shader control flow
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetIntegerVertexShaderConstant( int var, int const* pVec, int numIntVecs, bool bForce )
{
	Assert( pVec );
	Assert( var + numIntVecs <= g_pHardwareConfig->NumIntegerVertexShaderConstants() );

	if ( g_pHardwareConfig->GetDXSupportLevel() < 90 )
	{
		return;
	}

	if ( !bForce && memcmp( pVec, &m_DesiredState.m_pIntegerVertexShaderConstant[var], numIntVecs * sizeof( IntVector4D ) ) == 0 )
	{
		return;
	}

	Dx9Device()->SetVertexShaderConstantI( var, pVec, numIntVecs );
	memcpy( &m_DynamicState.m_pIntegerVertexShaderConstant[var], pVec, numIntVecs * sizeof(IntVector4D) );
	memcpy( &m_DesiredState.m_pIntegerVertexShaderConstant[var], pVec, numIntVecs * sizeof(IntVector4D) );
}

FORCEINLINE void CShaderAPIDx8::SetPixelShaderConstantInternal( int nStartConst, float const* pValues, int nNumConsts, bool bForce )
{
	Assert( nStartConst + nNumConsts <= g_pHardwareConfig->NumPixelShaderConstants() );

	if ( ! bForce )
	{
		int skip = 0;
		nNumConsts = AdjustUpdateRange( pValues, &m_DesiredState.m_pVectorPixelShaderConstant[nStartConst], nNumConsts, &skip );
		if ( !nNumConsts )
			return;
		nStartConst += skip;
		pValues += skip * 4;
	}
					
	Dx9Device()->SetPixelShaderConstantF( nStartConst, pValues, nNumConsts );
	memcpy( &m_DynamicState.m_pVectorPixelShaderConstant[nStartConst], pValues, nNumConsts * 4 * sizeof(float) );
	memcpy( &m_DesiredState.m_pVectorPixelShaderConstant[nStartConst], pValues, nNumConsts * 4 * sizeof(float) );
}

void CShaderAPIDx8::SetPixelShaderConstant( int var, float const* pVec, int numVecs, bool bForce )
{
	SetPixelShaderConstantInternal( var, pVec, numVecs, bForce );
}

template<class T> FORCEINLINE T GetData( uint8 const *pData )
{
	return * ( reinterpret_cast< T const *>( pData ) );
}



void CShaderAPIDx8::SetStandardTextureHandle( StandardTextureId_t nId, ShaderAPITextureHandle_t nHandle )
{
	Assert( nId < ssize( m_StdTextureHandles ) );
	m_StdTextureHandles[nId] = nHandle;
}

void CShaderAPIDx8::ExecuteCommandBuffer( uint8 *pCmdBuf )
{
	uint8 *pReturnStack[20];
	uint8 **pSP = &pReturnStack[std::size(pReturnStack)];
	uint8 *pLastCmd;
	for(;;)
	{
		uint8 *pCmd=pCmdBuf;
		int nCmd = GetData<int>( pCmdBuf );
		switch( nCmd )
		{
			case CBCMD_END:
			{
				if ( pSP == &pReturnStack[std::size(pReturnStack)] )
					return;
				else
				{
					// pop pc
					pCmdBuf = *( pSP ++ );
					break;
				}
			}

			case CBCMD_JUMP:
				pCmdBuf = GetData<uint8 *>(  pCmdBuf + sizeof( int ) );
				break;
				
			case CBCMD_JSR:
			{
				Assert( pSP > &(pReturnStack[0] ) );
// 				*(--pSP ) = pCmdBuf + sizeof( int ) + sizeof( uint8 *);
// 				pCmdBuf = GetData<uint8 *>(  pCmdBuf + sizeof( int ) );
				ExecuteCommandBuffer( GetData<uint8 *>(  pCmdBuf + sizeof( int ) ) );
				pCmdBuf = pCmdBuf + sizeof( int ) + sizeof( uint8 *);
				break;
			}

			case CBCMD_SET_PIXEL_SHADER_FLOAT_CONST:
			{
				int nStartConst = GetData<int>( pCmdBuf + sizeof( int ) );
				int nNumConsts = GetData<int>( pCmdBuf + 2 * sizeof( int ) );
				float const *pValues = reinterpret_cast< float const *> ( pCmdBuf + 3 * sizeof( int ) );
				pCmdBuf += nNumConsts * 4 * sizeof( float ) + 3 * sizeof( int );
				SetPixelShaderConstantInternal( nStartConst, pValues, nNumConsts, false );
				break;
			}

			case CBCMD_SETPIXELSHADERFOGPARAMS:
			{
				int nReg = GetData<int>( pCmdBuf + sizeof( int ) );
				pCmdBuf += 2 * sizeof( int );
				SetPixelShaderFogParams( nReg );			// !! speed fixme
				break;
			}
			case CBCMD_STORE_EYE_POS_IN_PSCONST:
			{
				int nReg = GetData<int>( pCmdBuf + sizeof( int ) );
				pCmdBuf += 2 * sizeof( int );
				SetPixelShaderConstantInternal( nReg, m_WorldSpaceCameraPositon.Base(), 1, false  );
				break;
			}

			case CBCMD_COMMITPIXELSHADERLIGHTING:
			{
				int nReg = GetData<int>( pCmdBuf + sizeof( int ) );
				pCmdBuf += 2 * sizeof( int );
				CommitPixelShaderLighting( nReg );
				break;
			}

			case CBCMD_SETPIXELSHADERSTATEAMBIENTLIGHTCUBE:
			{
				int nReg = GetData<int>( pCmdBuf + sizeof( int ) );
				pCmdBuf += 2 * sizeof( int );
				float *pCubeBase = m_DynamicState.m_AmbientLightCube[0].Base();
				SetPixelShaderConstantInternal( nReg, pCubeBase, 6, false );
				break;
			}

			case CBCMD_SETAMBIENTCUBEDYNAMICSTATEVERTEXSHADER:
			{
				pCmdBuf +=sizeof( int );
				SetVertexShaderStateAmbientLightCube();
				break;
			}

			case CBCMD_SET_DEPTH_FEATHERING_CONST:
			{
				int nConst = GetData<int>( pCmdBuf + sizeof( int ) );
				float fDepthBlendScale = GetData<float>( pCmdBuf + 2 * sizeof( int ) );
				pCmdBuf += 2 * sizeof( int ) + sizeof( float );
				SetDepthFeatheringPixelShaderConstant( nConst, fDepthBlendScale );
				break;
			}

			case CBCMD_SET_VERTEX_SHADER_FLOAT_CONST:
			{
				int nStartConst = GetData<int>( pCmdBuf + sizeof( int ) );
				int nNumConsts = GetData<int>( pCmdBuf + 2 * sizeof( int ) );
				float const *pValues = reinterpret_cast< float const *> ( pCmdBuf + 3 * sizeof( int ) );
				pCmdBuf += nNumConsts * 4 * sizeof( float ) + 3 * sizeof( int );
				SetVertexShaderConstantInternal( nStartConst, pValues, nNumConsts, false );
				break;
			}

			case CBCMD_BIND_STANDARD_TEXTURE:
			{
				int nSampler = GetData<int>( pCmdBuf + sizeof( int ) );
				int nTextureID = GetData<int>( pCmdBuf + 2 * sizeof( int ) );
				pCmdBuf += 3 * sizeof( int );
				if ( m_StdTextureHandles[nTextureID] != INVALID_SHADERAPI_TEXTURE_HANDLE )
				{
					BindTexture( (Sampler_t) nSampler, m_StdTextureHandles[nTextureID] );
				}
				else
				{
					ShaderUtil()->BindStandardTexture( (Sampler_t) nSampler, (StandardTextureId_t ) nTextureID );
				}
				break;
			}

			case CBCMD_BIND_SHADERAPI_TEXTURE_HANDLE:
			{
				int nSampler = GetData<int>( pCmdBuf + sizeof( int ) );
				ShaderAPITextureHandle_t hTexture = GetData<ShaderAPITextureHandle_t>( pCmdBuf + 2 * sizeof( int ) );
				Assert( hTexture != INVALID_SHADERAPI_TEXTURE_HANDLE );
				pCmdBuf += 2 * sizeof( int ) + sizeof( ShaderAPITextureHandle_t );
				BindTexture( (Sampler_t) nSampler, hTexture );
				break;
			}

			case CBCMD_SET_PSHINDEX:
			{
				int nIdx = GetData<int>( pCmdBuf + sizeof( int ) );
				ShaderManager()->SetPixelShaderIndex( nIdx );
				pCmdBuf += 2 * sizeof( int );
				break;
			}

			case CBCMD_SET_VSHINDEX:
			{
				int nIdx = GetData<int>( pCmdBuf + sizeof( int ) );
				ShaderManager()->SetVertexShaderIndex( nIdx );
				pCmdBuf += 2 * sizeof( int );
				break;
			}



#ifndef NDEBUG
			default:
			{
				Assert(0);
			}
#endif


		}
		pLastCmd = pCmd;
	}
}
	

//-----------------------------------------------------------------------------
// Sets the boolean registers for pixel shader control flow
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetBooleanPixelShaderConstant( int var, int const* pVec, int numBools, bool bForce )
{
	Assert( pVec );
	Assert( var + numBools <= g_pHardwareConfig->NumBooleanPixelShaderConstants() );

	if ( IsPC() && !bForce && memcmp( pVec, &m_DesiredState.m_pBooleanPixelShaderConstant[var], numBools * sizeof( BOOL ) ) == 0 )
	{
		return;
	}

	if ( IsPC() )
	{
		Dx9Device()->SetPixelShaderConstantB( var, pVec, numBools );
		memcpy( &m_DynamicState.m_pBooleanPixelShaderConstant[var], pVec, numBools * sizeof(BOOL) );
	}

	memcpy( &m_DesiredState.m_pBooleanPixelShaderConstant[var], pVec, numBools * sizeof(BOOL) );
}


//-----------------------------------------------------------------------------
// Sets the integer registers for pixel shader control flow
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetIntegerPixelShaderConstant( int var, int const* pVec, int numIntVecs, bool bForce )
{
	Assert( pVec );
	Assert( var + numIntVecs <= g_pHardwareConfig->NumIntegerPixelShaderConstants() );

	if ( IsPC() && !bForce && memcmp( pVec, &m_DesiredState.m_pIntegerPixelShaderConstant[var], numIntVecs * sizeof( IntVector4D ) ) == 0 )
	{
		return;
	}

	if ( IsPC() )
	{
		Dx9Device()->SetPixelShaderConstantI( var, pVec, numIntVecs );
		memcpy( &m_DynamicState.m_pIntegerPixelShaderConstant[var], pVec, numIntVecs * sizeof(IntVector4D) );
	}

	memcpy( &m_DesiredState.m_pIntegerPixelShaderConstant[var], pVec, numIntVecs * sizeof(IntVector4D) );
}


void CShaderAPIDx8::InvalidateDelayedShaderConstants( void )
{
	m_DelayedShaderConstants.Invalidate();
}

//-----------------------------------------------------------------------------
//
// Methods dealing with texture stage state
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Gets the texture associated with a texture state...
//-----------------------------------------------------------------------------

inline IDirect3DBaseTexture* CShaderAPIDx8::GetD3DTexture( ShaderAPITextureHandle_t hTexture )
{
	if ( hTexture == INVALID_SHADERAPI_TEXTURE_HANDLE )
	{
		return NULL;
	}

	AssertValidTextureHandle( hTexture );

	Texture_t& tex = GetTexture( hTexture );
	if ( tex.m_NumCopies == 1 )
	{
		return tex.GetTexture();
	}
	else
	{
		return tex.GetTexture( tex.m_CurrentCopy );
	}
}

//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline ShaderAPITextureHandle_t CShaderAPIDx8::GetModifyTextureHandle() const
{
	return m_ModifyTextureHandle;
}

inline IDirect3DBaseTexture* CShaderAPIDx8::GetModifyTexture()
{
	return CShaderAPIDx8::GetD3DTexture( m_ModifyTextureHandle );
}

void CShaderAPIDx8::SetModifyTexture( IDirect3DBaseTexture* pTex )
{
	if ( m_ModifyTextureHandle == INVALID_SHADERAPI_TEXTURE_HANDLE )
		return;
	
	Texture_t& tex = GetTexture( m_ModifyTextureHandle );
	if ( tex.m_NumCopies == 1 )
	{
		tex.SetTexture( pTex );
	}
	else
	{
		tex.SetTexture( tex.m_CurrentCopy, pTex );
	}
}

inline ShaderAPITextureHandle_t CShaderAPIDx8::GetBoundTextureBindId( Sampler_t sampler ) const
{
	return SamplerState( sampler ).m_BoundTexture;
}

inline bool CShaderAPIDx8::WouldBeOverTextureLimit( ShaderAPITextureHandle_t hTexture )
{
	if ( IsPC() )
	{
		if ( mat_texture_limit.GetInt() < 0 )
			return false;

		Texture_t &tex = GetTexture( hTexture );
		if ( tex.m_LastBoundFrame == m_CurrentFrame )
			return false;

		return m_nTextureMemoryUsedLastFrame + tex.GetMemUsage() > (mat_texture_limit.GetInt() * static_cast<intp>(1024));
	}
	return false;
}


#define SETSAMPLESTATEANDMIRROR( sampler, samplerState, state_type, mirror_field, value )	\
	if ( samplerState.mirror_field != value )												\
	{																						\
		samplerState.mirror_field = value;													\
		::SetSamplerState( Dx9Device(), sampler, state_type, value );	\
	}

#define SETSAMPLESTATEANDMIRROR_FLOAT( sampler, samplerState, state_type, mirror_field, value )	\
	if ( samplerState.mirror_field != value )												\
		{																					\
		samplerState.mirror_field = value;													\
		::SetSamplerState( Dx9Device(), sampler, state_type, *(DWORD*)&value );	\
		}


//-----------------------------------------------------------------------------
// Sets state on the board related to the texture state
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetTextureState( Sampler_t sampler, ShaderAPITextureHandle_t hTexture, bool force )
{
	// Get the dynamic texture info
	SamplerState_t &samplerState = SamplerState( sampler );

	// Set the texture state, but only if it changes
	if ( ( samplerState.m_BoundTexture == hTexture ) && !force )
		return;

	// Disabling texturing
	if ( hTexture == INVALID_SHADERAPI_TEXTURE_HANDLE || WouldBeOverTextureLimit( hTexture ) )
	{
		Dx9Device()->SetTexture( sampler, 0 );
		return;
	}

	samplerState.m_BoundTexture = hTexture;

	// Don't set this if we're disabled
	if ( !samplerState.m_TextureEnable )
		return;

	RECORD_COMMAND( DX8_SET_TEXTURE, 3 );
	RECORD_INT( stage );
	RECORD_INT( hTexture );
	RECORD_INT( GetTexture( hTexture).m_CurrentCopy );

	IDirect3DBaseTexture *pTexture = CShaderAPIDx8::GetD3DTexture( hTexture );

	Dx9Device()->SetTexture( sampler, pTexture );

	Texture_t &tex = GetTexture( hTexture );
	if ( tex.m_LastBoundFrame != m_CurrentFrame )
	{
		tex.m_LastBoundFrame = m_CurrentFrame;
		tex.m_nTimesBoundThisFrame = 0;

		if ( tex.m_pTextureGroupCounterFrame )
		{
			constexpr auto maxCounterValue = std::numeric_limits<
				std::remove_pointer_t<
					decltype(tex.m_pTextureGroupCounterFrame)>
			>::max();

			AssertMsg( maxCounterValue - static_cast<uintp>(tex.GetMemUsage()) >=
				*tex.m_pTextureGroupCounterGlobal,
				"TexGroup_global_%s counter overflow. %s (total) + %s (texture) > %s (max).",
				tex.m_TextureGroupName.String(),
				V_pretifymem( *tex.m_pTextureGroupCounterGlobal, 2, true ),
				V_pretifymem( tex.GetMemUsage(), 2, true ),
				V_pretifymem( maxCounterValue, 2, true ) );

			// Update the per-frame texture group counter.
			*tex.m_pTextureGroupCounterFrame += tex.GetMemUsage();
		}

		// Track memory usage.
		m_nTextureMemoryUsedLastFrame += tex.GetMemUsage();
	}

	if ( !m_bDebugTexturesRendering )
		++tex.m_nTimesBoundThisFrame;

	tex.m_nTimesBoundMax = MAX( tex.m_nTimesBoundMax, tex.m_nTimesBoundThisFrame );

	static MaterialSystem_Config_t &materialSystemConfig = ShaderUtil()->GetConfig();

	D3DTEXTUREFILTERTYPE minFilter = tex.m_MinFilter;
	D3DTEXTUREFILTERTYPE magFilter = tex.m_MagFilter;
	D3DTEXTUREFILTERTYPE mipFilter = tex.m_MipFilter;
	int finestMipmapLevel		   = tex.m_FinestMipmapLevel;
	float lodBias				   = tex.m_LodBias;

	if ( materialSystemConfig.bMipMapTextures == 0 )
	{
		mipFilter = D3DTEXF_NONE;
	} 

	if ( materialSystemConfig.bFilterTextures == 0 && tex.m_NumLevels > 1 ) 
	{
		minFilter = D3DTEXF_NONE;
		magFilter = D3DTEXF_NONE;
		mipFilter = D3DTEXF_POINT;
	}

	D3DTEXTUREADDRESS uTexWrap = tex.m_UTexWrap;
	D3DTEXTUREADDRESS vTexWrap = tex.m_VTexWrap;
	D3DTEXTUREADDRESS wTexWrap = tex.m_WTexWrap;

	// For now do this the old way on OSX since the dxabstract layer doesn't support SetSamplerStates
	// ###OSX### punting on OSX for now
#if DX_TO_GL_ABSTRACTION && !OSX
	if ( ( samplerState.m_MinFilter != minFilter ) || ( samplerState.m_MagFilter != magFilter ) || ( samplerState.m_MipFilter != mipFilter ) ||
		( samplerState.m_UTexWrap != uTexWrap ) || ( samplerState.m_VTexWrap != vTexWrap ) || ( samplerState.m_WTexWrap != wTexWrap ) || 
		( samplerState.m_FinestMipmapLevel != finestMipmapLevel ) || ( samplerState.m_LodBias != lodBias ) )
	{
		samplerState.m_UTexWrap = uTexWrap;
		samplerState.m_VTexWrap = vTexWrap;
		samplerState.m_WTexWrap = wTexWrap;
		samplerState.m_MinFilter = minFilter;
		samplerState.m_MagFilter = magFilter;
		samplerState.m_MipFilter = mipFilter;
		samplerState.m_FinestMipmapLevel = finestMipmapLevel;
		samplerState.m_LodBias = lodBias;
		Dx9Device()->SetSamplerStates( sampler, uTexWrap, vTexWrap, wTexWrap, minFilter, magFilter, mipFilter, finestMipmapLevel, lodBias );
	}
#else
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_ADDRESSU, m_UTexWrap, uTexWrap );
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_ADDRESSV, m_VTexWrap, vTexWrap );
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_ADDRESSW, m_WTexWrap, wTexWrap );
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_MINFILTER, m_MinFilter, minFilter );
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_MAGFILTER, m_MagFilter, magFilter );
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_MIPFILTER, m_MipFilter, mipFilter );
	SETSAMPLESTATEANDMIRROR( sampler, samplerState, D3DSAMP_MAXMIPLEVEL, m_FinestMipmapLevel, finestMipmapLevel );
	SETSAMPLESTATEANDMIRROR_FLOAT( sampler, samplerState, D3DSAMP_MIPMAPLODBIAS, m_LodBias, lodBias );
	
#endif
}

void CShaderAPIDx8::BindTexture( Sampler_t sampler, ShaderAPITextureHandle_t textureHandle )
{
	LOCK_SHADERAPI();
	SetTextureState( sampler, textureHandle );
}

void CShaderAPIDx8::BindVertexTexture( VertexTextureSampler_t nStage, ShaderAPITextureHandle_t textureHandle )
{
	Assert( g_pMaterialSystemHardwareConfig->GetVertexTextureCount() != 0 );
	LOCK_SHADERAPI();
	ADD_VERTEX_TEXTURE_FUNC( COMMIT_PER_PASS, COMMIT_VERTEX_SHADER, 
		CommitVertexTextures, nStage, m_BoundTexture, textureHandle );
}


//-----------------------------------------------------------------------------
// Texture allocation/deallocation
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Computes stats info for a texture
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ComputeStatsInfo( ShaderAPITextureHandle_t hTexture, bool isCubeMap, bool isVolumeTexture )
{
	Texture_t &textureData = GetTexture( hTexture );

	textureData.m_SizeBytes = 0;
	textureData.m_SizeTexels = 0;
	textureData.m_LastBoundFrame = -1;

	IDirect3DBaseTexture* pD3DTex = CShaderAPIDx8::GetD3DTexture( hTexture );

	{
		if ( isCubeMap )
		{
			IDirect3DCubeTexture* pTex = static_cast<IDirect3DCubeTexture*>(pD3DTex);
			if ( !pTex )
			{
				Assert( 0 );
				return;
			}

			int numLevels = pTex->GetLevelCount();
			for (int i = 0; i < numLevels; ++i)
			{
				D3DSURFACE_DESC desc;
				HRESULT hr = pTex->GetLevelDesc( i, &desc );
				Assert( !FAILED(hr) );
				textureData.m_SizeBytes += 6 * ImageLoader::GetMemRequired( desc.Width, desc.Height, 1, textureData.GetImageFormat(), false );
				textureData.m_SizeTexels += 6 * desc.Width * desc.Height;
			}
		}
		else if ( isVolumeTexture )
		{
			IDirect3DVolumeTexture9* pTex = static_cast<IDirect3DVolumeTexture9*>(pD3DTex);
			if ( !pTex )
			{
				Assert( 0 );
				return;
			}
			int numLevels = pTex->GetLevelCount();
			for (int i = 0; i < numLevels; ++i)
			{
				D3DVOLUME_DESC desc;
				HRESULT hr = pTex->GetLevelDesc( i, &desc );
				Assert( !FAILED( hr ) );
				textureData.m_SizeBytes += ImageLoader::GetMemRequired( desc.Width, desc.Height, desc.Depth, textureData.GetImageFormat(), false );
				textureData.m_SizeTexels += desc.Width * desc.Height;
			}
		}
		else
		{
			IDirect3DTexture* pTex = static_cast<IDirect3DTexture*>(pD3DTex);
			if ( !pTex )
			{
				Assert( 0 );
				return;
			}

			int numLevels = pTex->GetLevelCount();
			for (int i = 0; i < numLevels; ++i)
			{
				D3DSURFACE_DESC desc;
				HRESULT hr = pTex->GetLevelDesc( i, &desc );
				Assert( !FAILED( hr ) );
				textureData.m_SizeBytes += ImageLoader::GetMemRequired( desc.Width, desc.Height, 1, textureData.GetImageFormat(), false );
				textureData.m_SizeTexels += desc.Width * desc.Height;
			}
		}
	}
}

ShaderAPITextureHandle_t CShaderAPIDx8::CreateDepthTexture( 
	ImageFormat renderTargetFormat, 
	int width, 
	int height, 
	const char *pDebugName,
	bool bTexture )
{
	LOCK_SHADERAPI();

	ShaderAPITextureHandle_t i = CreateTextureHandle();
	Texture_t *pTexture = &GetTexture( i );

	pTexture->m_Flags = Texture_t::IS_ALLOCATED;
	if( bTexture )
	{
		pTexture->m_Flags |= Texture_t::IS_DEPTH_STENCIL_TEXTURE;
	}
	else
	{
		pTexture->m_Flags |= Texture_t::IS_DEPTH_STENCIL;
	}

	pTexture->m_DebugName = pDebugName;
	pTexture->m_Width = width;
	pTexture->m_Height = height;
	pTexture->m_Depth = 1;			// fake
	pTexture->m_Count = 1;			// created single texture
	pTexture->m_CountIndex = 0;		// created single texture
	pTexture->m_CreationFlags = 0;	// fake
	pTexture->m_NumCopies = 1;
	pTexture->m_CurrentCopy = 0;

	ImageFormat renderFormat = FindNearestSupportedFormat( renderTargetFormat, false, true, false );
	D3DFORMAT nDepthFormat = m_bUsingStencil ? D3DFMT_D24S8 : D3DFMT_D24X8;
	D3DFORMAT format = FindNearestSupportedDepthFormat( m_nAdapter, m_AdapterFormat, renderFormat, nDepthFormat );
	D3DMULTISAMPLE_TYPE multisampleType = D3DMULTISAMPLE_NONE;

	pTexture->m_NumLevels = 1;
	pTexture->m_SizeTexels = width * height;
	pTexture->m_SizeBytes = ImageLoader::GetMemRequired( width, height, 1, renderFormat, false );

	RECORD_COMMAND( DX8_CREATE_DEPTH_TEXTURE, 5 );
	RECORD_INT( i );
	RECORD_INT( width );
	RECORD_INT( height );
	RECORD_INT( format );
	RECORD_INT( multisampleType );

	HRESULT hr;
	if ( !bTexture )
	{
		hr = Dx9Device()->CreateDepthStencilSurface(
			width, height, format, multisampleType, 0, TRUE, &pTexture->GetDepthStencilSurface(), NULL );
		if ( FAILED(hr) )
		{
			Assert(false);
			Warning( __FUNCTION__ ": IDirect3DDevice9Ex::CreateDepthStencilSurface(width = %d, height = %d, format = 0x%x, multisample type = 0x%x, multisample quality = 0, discard = true) failed w/e %s.\n",
				width,
				height,
				format,
				multisampleType,
				se::win::com::com_error_category().message(hr).c_str() );
		}
	}
	else
	{
		IDirect3DTexture9 *pTex;
		hr = Dx9Device()->CreateTexture( width, height, 1, D3DUSAGE_DEPTHSTENCIL, format, D3DPOOL_DEFAULT, &pTex, NULL );
		if ( FAILED(hr) )
		{
			Assert(false);
			Warning( __FUNCTION__ ": IDirect3DDevice9Ex::CreateTexture(width = %d, height = %d, levels = 1, usage = D3DUSAGE_DEPTHSTENCIL, format = 0x%x, pool = D3DPOOL_DEFAULT) failed w/e %s.\n",
				width,
				height,
				format,
				multisampleType,
				se::win::com::com_error_category().message(hr).c_str() );
		}

		pTexture->SetTexture( pTex );
	}

	return i;
}


// FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Could keep a free-list for this instead of linearly searching.  We
// don't create textures all the time, so this is probably fine for now.
// FIXME!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
ShaderAPITextureHandle_t CShaderAPIDx8::CreateTextureHandle( void )
{
	ShaderAPITextureHandle_t handle;
	CreateTextureHandles( &handle, 1 );
	return handle;
}

void CShaderAPIDx8::CreateTextureHandles( ShaderAPITextureHandle_t *handles, int count )
{
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL0 );

	if ( count <= 0 )
		return;

	MEM_ALLOC_CREDIT();

	int idxCreating = 0;
	ShaderAPITextureHandle_t hTexture;

	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - Search", __FUNCTION__ );

		for ( hTexture = m_Textures.Head(); hTexture != m_Textures.InvalidIndex(); hTexture = m_Textures.Next( hTexture ) )
		{
			if ( !( m_Textures[hTexture].m_Flags & Texture_t::IS_ALLOCATED ) )
			{
				handles[ idxCreating ++ ] = hTexture;
				if ( idxCreating >= count )
					return;
			}
		}
	}

	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - Add", __FUNCTION__ );
	while ( idxCreating < count )
		handles[ idxCreating ++ ] = m_Textures.AddToTail();
}

//-----------------------------------------------------------------------------
// Creates a lovely texture
//-----------------------------------------------------------------------------

ShaderAPITextureHandle_t CShaderAPIDx8::CreateTexture(
	int			width, 
	int			height, 
	int			depth,
	ImageFormat dstImageFormat, 
	int			numMipLevels, 
	int			numCopies, 
	int			creationFlags,
	const char *pDebugName,
	const char *pTextureGroupName )
{
	ShaderAPITextureHandle_t handle = 0;
	CreateTextures( &handle, 1, width, height, depth, dstImageFormat, numMipLevels, numCopies, creationFlags, pDebugName, pTextureGroupName );
	return handle;
}

void CShaderAPIDx8::CreateTextures(
	ShaderAPITextureHandle_t *pHandles,
	int			count,
	int			width, 
	int			height, 
	int			depth,
	ImageFormat dstImageFormat, 
	int			numMipLevels, 
	int			numCopies, 
	int			creationFlags,
	const char *pDebugName,
	const char *pTextureGroupName )
{
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL0 );

	LOCK_SHADERAPI();

	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - PostLock", __FUNCTION__ );

	Assert( this == g_pShaderAPI );

	if ( depth == 0 )
	{
		depth = 1;
	}

	bool isCubeMap = (creationFlags & TEXTURE_CREATE_CUBEMAP) != 0;
	bool isRenderTarget = (creationFlags & TEXTURE_CREATE_RENDERTARGET) != 0;
	bool managed = (creationFlags & TEXTURE_CREATE_MANAGED) != 0;
	[[maybe_unused]] bool isDepthBuffer = (creationFlags & TEXTURE_CREATE_DEPTHBUFFER) != 0;
	[[maybe_unused]] bool isDynamic = (creationFlags & TEXTURE_CREATE_DYNAMIC) != 0;
	[[maybe_unused]] bool isSRGB = (creationFlags & TEXTURE_CREATE_SRGB) != 0;			// for Posix/GL only... not used here ?

#if defined(IS_WINDOWS_PC) && defined(SHADERAPIDX9)
	if ( managed )
	{
		// Managed textures aren't available under D3D9Ex, but we never lose
		// texture data, so it's ok to use the default pool. Really. We can't
		// lock default-pool textures like we normally would to upload, but we
		// have special logic to blit full updates via XM* helper functions
		// in D3D9Ex mode (see texturedx8.cpp)
		managed = false;
		creationFlags &= ~TEXTURE_CREATE_MANAGED;
	}
#endif

	// Can't be both managed + dynamic. Dynamic is an optimization, but 
	// if it's not managed, then we gotta do special client-specific stuff
	// So, managed wins out!
	if ( managed )
	{
		creationFlags &= ~TEXTURE_CREATE_DYNAMIC;
		isDynamic = false;
	}

	

	// Create a set of texture handles
	CreateTextureHandles( pHandles, count );
	Texture_t **arrTxp = ( Texture_t ** ) stackalloc( count * sizeof( Texture_t * ) );

	unsigned short usSetFlags = 0;
	usSetFlags |= ( IsPosix() || ( creationFlags & (TEXTURE_CREATE_DYNAMIC | TEXTURE_CREATE_MANAGED) ) ) ? Texture_t::IS_LOCKABLE : 0;
	usSetFlags |= ( creationFlags & TEXTURE_CREATE_VERTEXTEXTURE) ? Texture_t::IS_VERTEX_TEXTURE : 0;

	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - CreateFrames", __FUNCTION__ );

	for ( int idxFrame = 0; idxFrame < count; ++ idxFrame )
	{
		arrTxp[ idxFrame ] = &GetTexture( pHandles[ idxFrame ] );
		Texture_t *pTexture = arrTxp[ idxFrame ];
		pTexture->m_Flags = Texture_t::IS_ALLOCATED;
		pTexture->m_DebugName = pDebugName;
		pTexture->m_Width = width;
		pTexture->m_Height = height;
		pTexture->m_Depth = depth;
		pTexture->m_Count = count;
		pTexture->m_CountIndex = idxFrame;

		pTexture->m_CreationFlags = creationFlags;
		pTexture->m_Flags |= usSetFlags;

		RECORD_COMMAND( DX8_CREATE_TEXTURE, 12 );
		RECORD_INT( textureHandle );
		RECORD_INT( width );
		RECORD_INT( height );
		RECORD_INT( depth );		// depth for volume textures
		RECORD_INT( ImageLoader::ImageFormatToD3DFormat( FindNearestSupportedFormat(dstImageFormat)) );
		RECORD_INT( numMipLevels );
		RECORD_INT( isCubeMap );
		RECORD_INT( numCopies <= 1 ? 1 : numCopies );
		RECORD_INT( isRenderTarget ? 1 : 0 );
		RECORD_INT( managed );
		RECORD_INT( isDepthBuffer ? 1 : 0 );
		RECORD_INT( isDynamic ? 1 : 0 );

		IDirect3DBaseTexture* pD3DTex;

		// Set the initial texture state
		if ( numCopies <= 1 )
		{
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - CreateD3DTexture", __FUNCTION__ );

			pTexture->m_NumCopies = 1;
			pD3DTex = CreateD3DTexture( width, height, depth, dstImageFormat, numMipLevels, creationFlags, (char*)pDebugName );
			pTexture->SetTexture( pD3DTex );
		}
		else
		{
			tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s - CreateD3DTexture", __FUNCTION__ );

			pTexture->m_NumCopies = numCopies;
			{
				// dimhotepus: Add MEM_ALLOC_D3D_CREDIT
				MEM_ALLOC_D3D_CREDIT();

				pTexture->GetTextureArray() = new IDirect3DBaseTexture* [numCopies];
			}

			for (int k = 0; k < numCopies; ++k)
			{
				pD3DTex = CreateD3DTexture( width, height, depth, dstImageFormat, numMipLevels, creationFlags, (char*)pDebugName );
				pTexture->SetTexture( k, pD3DTex );
			}
		}
		pTexture->m_CurrentCopy = 0;

		pD3DTex = CShaderAPIDx8::GetD3DTexture( pHandles[ idxFrame ] );

		pTexture->SetImageFormat( dstImageFormat );
		pTexture->m_UTexWrap = D3DTADDRESS_CLAMP;
		pTexture->m_VTexWrap = D3DTADDRESS_CLAMP;
		pTexture->m_WTexWrap = D3DTADDRESS_CLAMP;

		if ( isRenderTarget )
		{
			if ( ( dstImageFormat == IMAGE_FORMAT_NV_INTZ   ) || ( dstImageFormat == IMAGE_FORMAT_NV_RAWZ   ) || 
				 ( dstImageFormat == IMAGE_FORMAT_ATI_DST16 ) || ( dstImageFormat == IMAGE_FORMAT_ATI_DST24 ) )
			{
				pTexture->m_MinFilter = pTexture->m_MagFilter = D3DTEXF_POINT;
			}
			else
			{
				pTexture->m_MinFilter = pTexture->m_MagFilter = D3DTEXF_LINEAR;
			}

			pTexture->m_NumLevels = 1;
			pTexture->m_MipFilter = D3DTEXF_NONE;
		}
		else
		{
			pTexture->m_NumLevels = pD3DTex ? pD3DTex->GetLevelCount() : 1;
			pTexture->m_MipFilter = (pTexture->m_NumLevels != 1) ? D3DTEXF_LINEAR : D3DTEXF_NONE;
			pTexture->m_MinFilter = pTexture->m_MagFilter = D3DTEXF_LINEAR;
		}
		pTexture->m_SwitchNeeded = false;
		
		ComputeStatsInfo( pHandles[idxFrame], isCubeMap, (depth > 1) );
		SetupTextureGroup( pHandles[idxFrame], pTextureGroupName );
	}
}

void CShaderAPIDx8::SetupTextureGroup( ShaderAPITextureHandle_t hTexture, const char *pTextureGroupName )
{
	Texture_t *pTexture = &GetTexture( hTexture );

	Assert( !pTexture->m_pTextureGroupCounterGlobal );
	
	// Setup the texture group stuff.
	if ( pTextureGroupName && pTextureGroupName[0] != 0 )
	{
		pTexture->m_TextureGroupName = pTextureGroupName;
	}
	else
	{
		pTexture->m_TextureGroupName = TEXTURE_GROUP_UNACCOUNTED;
	}

	// 360 cannot vprof due to multicore loading until vprof is reentrant and these counters are real.
#if defined( VPROF_ENABLED )
	char counterName[256];
	V_sprintf_safe( counterName, "TexGroup_global_%s", pTexture->m_TextureGroupName.String() );
	pTexture->m_pTextureGroupCounterGlobal = g_VProfCurrentProfile.FindOrCreateCounter( counterName, COUNTER_GROUP_TEXTURE_GLOBAL );

	V_sprintf_safe( counterName, "TexGroup_frame_%s", pTexture->m_TextureGroupName.String() );
	pTexture->m_pTextureGroupCounterFrame = g_VProfCurrentProfile.FindOrCreateCounter( counterName, COUNTER_GROUP_TEXTURE_PER_FRAME );
#else
	pTexture->m_pTextureGroupCounterGlobal = NULL;
	pTexture->m_pTextureGroupCounterFrame  = NULL;
#endif

	if ( pTexture->m_pTextureGroupCounterGlobal )
	{
		constexpr auto maxCounterValue = std::numeric_limits<
			std::remove_pointer_t<
				decltype(pTexture->m_pTextureGroupCounterGlobal)>
		>::max();

		AssertMsg( maxCounterValue - static_cast<uintp>(pTexture->GetMemUsage()) >=
			*pTexture->m_pTextureGroupCounterGlobal,
			"TexGroup_global_%s counter overflow. %s (total) + %s (texture) > %s (max).",
			pTexture->m_TextureGroupName.String(),
			V_pretifymem( *pTexture->m_pTextureGroupCounterGlobal, 2, true ),
			V_pretifymem( pTexture->GetMemUsage(), 2, true ),
			V_pretifymem( maxCounterValue, 2, true ) );
		*pTexture->m_pTextureGroupCounterGlobal += pTexture->GetMemUsage();
	}
}

//-----------------------------------------------------------------------------
// Deletes a texture...
//-----------------------------------------------------------------------------
void CShaderAPIDx8::DeleteD3DTexture( ShaderAPITextureHandle_t hTexture )
{
	int numDeallocated = 0;
	Texture_t &texture = GetTexture( hTexture );

	if ( texture.m_Flags & Texture_t::IS_DEPTH_STENCIL )
	{
		// garymcthack - need to make sure that playback knows how to deal with these.
		RECORD_COMMAND( DX8_DESTROY_DEPTH_TEXTURE, 1 );
		RECORD_INT( hTexture );

		if ( texture.GetDepthStencilSurface() )
		{
			ULONG nRetVal = texture.GetDepthStencilSurface()->Release();
			Assert( nRetVal == 0 );
			texture.GetDepthStencilSurface() = 0;
			numDeallocated = 1;
		}
		else
		{
			// FIXME: we hit this on shutdown of HLMV on some machines
			Assert( 0 );
		}
	}
	else if ( texture.m_NumCopies == 1 )
	{
		if ( texture.GetTexture() )
		{
			RECORD_COMMAND( DX8_DESTROY_TEXTURE, 1 );
			RECORD_INT( hTexture );

			DestroyD3DTexture( texture.GetTexture() );
			texture.SetTexture( 0 );
			numDeallocated = 1;
		}
	}
	else
	{
		if ( texture.GetTextureArray() )
		{
			RECORD_COMMAND( DX8_DESTROY_TEXTURE, 1 );
			RECORD_INT( hTexture );

			// Multiple copy texture
			for (int j = 0; j < texture.m_NumCopies; ++j)
			{
				if (texture.GetTexture( j ))
				{
					DestroyD3DTexture( texture.GetTexture( j ) );
					texture.SetTexture( j, 0 );
					++numDeallocated;
				}
			}

			delete [] texture.GetTextureArray();
			texture.GetTextureArray() = 0;
		}
	}

	texture.m_NumCopies = 0;

	// Remove this texture from its global texture group counter.
	if ( texture.m_pTextureGroupCounterGlobal )
	{
		AssertMsg( *texture.m_pTextureGroupCounterGlobal >= static_cast<uintp>(texture.GetMemUsage()),
			"TexGroup_global_%s counter underflow. %s (total) < %s (texture).",
			texture.m_TextureGroupName.String(),
			V_pretifymem( *texture.m_pTextureGroupCounterGlobal, 2, true ),
			V_pretifymem( texture.GetMemUsage(), 2, true ) );

		*texture.m_pTextureGroupCounterGlobal -= texture.GetMemUsage();
		texture.m_pTextureGroupCounterGlobal = NULL;
	}

	// remove this texture from std textures
	for( auto &&h : m_StdTextureHandles )
	{
		if ( h == hTexture )
			h = INVALID_SHADERAPI_TEXTURE_HANDLE;
	}

}


//-----------------------------------------------------------------------------
// Unbinds a texture from all texture stages
//-----------------------------------------------------------------------------
void CShaderAPIDx8::UnbindTexture( ShaderAPITextureHandle_t hTexture )
{
	// Make sure no texture units are currently bound to it...
	for ( int unit = 0; unit < g_pHardwareConfig->GetSamplerCount(); ++unit )
	{
		if ( hTexture == SamplerState( unit ).m_BoundTexture )
		{
			// Gotta set this here because INVALID_SHADERAPI_TEXTURE_HANDLE means don't actually
			// set bound texture (it's used for disabling texturemapping)
			SamplerState( unit ).m_BoundTexture = INVALID_SHADERAPI_TEXTURE_HANDLE;
			SetTextureState( (Sampler_t)unit, INVALID_SHADERAPI_TEXTURE_HANDLE );
		}
	}

	int nVertexTextureCount = g_pHardwareConfig->GetVertexTextureCount();
	for ( int nSampler = 0; nSampler < nVertexTextureCount; ++nSampler )
	{
		if ( hTexture == m_DynamicState.m_VertexTextureState[ nSampler ].m_BoundTexture )
		{
			// Gotta set this here because INVALID_SHADERAPI_TEXTURE_HANDLE means don't actually
			// set bound texture (it's used for disabling texturemapping)
			BindVertexTexture( (VertexTextureSampler_t)nSampler, INVALID_SHADERAPI_TEXTURE_HANDLE );
		}
	}
}


//-----------------------------------------------------------------------------
// Deletes a texture...
//-----------------------------------------------------------------------------
void CShaderAPIDx8::DeleteTexture( ShaderAPITextureHandle_t textureHandle )
{
	LOCK_SHADERAPI();
	AssertValidTextureHandle( textureHandle );

	if ( !TextureIsAllocated( textureHandle ) )
	{
		// already deallocated
		return;
	}
	
	// Unbind it!
	UnbindTexture( textureHandle );

	// Delete it baby
	DeleteD3DTexture( textureHandle );

	// Now remove the texture from the list
	// Mark as deallocated so that it can be reused.
	GetTexture( textureHandle ).m_Flags = 0;
}


void CShaderAPIDx8::WriteTextureToFile( ShaderAPITextureHandle_t hTexture, const char *szFileName )
{
	Texture_t *pTexInt = &GetTexture( hTexture );
	//Assert( pTexInt->IsCubeMap() == false );
	//Assert( pTexInt->IsVolumeTexture() == false );
	IDirect3DTexture *pD3DTexture = (IDirect3DTexture *)pTexInt->GetTexture();

	// Get the level of the texture we want to read from
	se::win::com::com_ptr<IDirect3DSurface> pTextureLevel;
	HRESULT hr = pD3DTexture->GetSurfaceLevel( 0, &pTextureLevel );
	if ( FAILED( hr ) )
		return;

	D3DSURFACE_DESC surfaceDesc;
	pD3DTexture->GetLevelDesc( 0, &surfaceDesc );
	if ( FAILED( hr ) )
		return;

	D3DLOCKED_RECT	lockedRect;
	
	//if( pTexInt->m_Flags & Texture_t::IS_RENDER_TARGET )
	{
		//render targets can't be locked, luckily we can copy the surface to system memory and lock that.
		se::win::com::com_ptr<IDirect3DSurface> pSystemSurface;
		hr = Dx9Device()->CreateOffscreenPlainSurface( surfaceDesc.Width, surfaceDesc.Height, surfaceDesc.Format, D3DPOOL_SYSTEMMEM, &pSystemSurface, NULL );
		Assert( SUCCEEDED( hr ) );

		pSystemSurface->GetDesc( &surfaceDesc );

		hr = Dx9Device()->GetRenderTargetData( pTextureLevel, pSystemSurface );
		Assert( SUCCEEDED( hr ) );

		//pretend this is the texture level we originally grabbed with GetSurfaceLevel() and continue on
		pTextureLevel = std::move( pSystemSurface );
	}

	// lock the region
	if ( FAILED( pTextureLevel->LockRect( &lockedRect, NULL, D3DLOCK_READONLY ) ) )
	{
		Assert( 0 );
		return;
	}

	if ( !TGAWriter::WriteTGAFile( szFileName,
		surfaceDesc.Width, surfaceDesc.Height,
		pTexInt->GetImageFormat(),
		(const uint8 *)lockedRect.pBits,
		lockedRect.Pitch ) )
	{
		Warning( "Unable to write TGA '%s'.\n", szFileName );
		Assert( 0 );
	}

	if ( FAILED( pTextureLevel->UnlockRect() ) ) 
	{
		Assert( 0 );
		return;
	}
}


//-----------------------------------------------------------------------------
// Releases all textures
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ReleaseAllTextures()
{
	ClearStdTextureHandles();
	ShaderAPITextureHandle_t hTexture;
	for ( hTexture = m_Textures.Head(); hTexture != m_Textures.InvalidIndex(); hTexture = m_Textures.Next( hTexture ) )
	{
		if ( TextureIsAllocated( hTexture ) )
		{
			// Delete it baby
			DeleteD3DTexture( hTexture );
		}
	}

	// Make sure all texture units are pointing to nothing
	for (int unit = 0; unit < g_pHardwareConfig->GetSamplerCount(); ++unit )
	{
		SamplerState( unit ).m_BoundTexture = INVALID_SHADERAPI_TEXTURE_HANDLE;
		SetTextureState( (Sampler_t)unit, INVALID_SHADERAPI_TEXTURE_HANDLE );
	}
}

void CShaderAPIDx8::DeleteAllTextures()
{
	ReleaseAllTextures();
	m_Textures.Purge();
}

bool CShaderAPIDx8::IsTexture( ShaderAPITextureHandle_t textureHandle )
{
	LOCK_SHADERAPI();

	if ( !TextureIsAllocated( textureHandle ) )
	{
		return false;
	}

	if ( GetTexture( textureHandle ).m_Flags & Texture_t::IS_DEPTH_STENCIL )
	{
		return GetTexture( textureHandle ).GetDepthStencilSurface() != 0;
	}
	else if ( ( GetTexture( textureHandle ).m_NumCopies == 1 && GetTexture( textureHandle ).GetTexture() != 0 ) ||
				( GetTexture( textureHandle ).m_NumCopies > 1 && GetTexture( textureHandle ).GetTexture( 0 ) != 0 ) )
	{
		return true;
	}

	return false;
}



//-----------------------------------------------------------------------------
// Gets the surface associated with a texture (refcount of surface is increased)
//-----------------------------------------------------------------------------
se::win::com::com_ptr<IDirect3DSurface9> CShaderAPIDx8::GetTextureSurface( ShaderAPITextureHandle_t textureHandle )
{
	MEM_ALLOC_D3D_CREDIT();

	// We'll be modifying this sucka
	AssertValidTextureHandle( textureHandle );
	Texture_t &tex = GetTexture( textureHandle );
	if ( !( tex.m_Flags & Texture_t::IS_ALLOCATED ) )
	{
		return {};
	}

	IDirect3DBaseTexture* pD3DTex = CShaderAPIDx8::GetD3DTexture( textureHandle );
	IDirect3DTexture* pTex = static_cast<IDirect3DTexture*>( pD3DTex );
	Assert( pTex );
	if ( !pTex )
	{
		return {};
	}

	se::win::com::com_ptr<IDirect3DSurface9> pSurface;
	HRESULT hr = pTex->GetSurfaceLevel( 0, &pSurface );
	Assert( SUCCEEDED( hr ) );

	return pSurface;
}

//-----------------------------------------------------------------------------
// Gets the surface associated with a texture (refcount of surface is increased)
//-----------------------------------------------------------------------------
se::win::com::com_ptr<IDirect3DSurface9> CShaderAPIDx8::GetDepthTextureSurface( ShaderAPITextureHandle_t textureHandle )
{
	AssertValidTextureHandle( textureHandle );
	if ( !TextureIsAllocated( textureHandle ) )
	{
		return {};
	}

	return se::win::com::com_ptr<IDirect3DSurface9>{ GetTexture( textureHandle ).GetDepthStencilSurface() };
}

//-----------------------------------------------------------------------------
// Changes the render target
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t colorTextureHandle, ShaderAPITextureHandle_t depthTextureHandle )
{
	LOCK_SHADERAPI();
	if ( IsDeactivated( ) )
	{
		return;
	}

	// GR - need to flush batched geometry
	FlushBufferedPrimitives();

#if defined( PIX_INSTRUMENTATION )
	{
		const char *pRT = "Backbuffer";
		const char *pDS = "DefaultDepthStencil";

		if ( colorTextureHandle == SHADER_RENDERTARGET_NONE )
		{
			pRT = "None";
		}
		else if ( colorTextureHandle != SHADER_RENDERTARGET_BACKBUFFER )
		{
			Texture_t &tex = GetTexture( colorTextureHandle );
			pRT = tex.m_DebugName.String();
		}

		if ( depthTextureHandle == SHADER_RENDERTARGET_NONE )
		{
			pDS = "None";
		}
		else if ( depthTextureHandle != SHADER_RENDERTARGET_DEPTHBUFFER )
		{
			Texture_t &tex = GetTexture( depthTextureHandle );
			pDS = tex.m_DebugName.String();
		}

		char buf[256];
		V_sprintf_safe( buf, "SRT:%s %s", pRT ? pRT : "?", pDS ? pDS : "?" );
		BeginPIXEvent( 0xFFFFFFFF, buf );
		EndPIXEvent();
	}
#endif

	// dimhotepus: No need to check device state here as it 
	// RECORD_COMMAND( DX8_CHECK_DEVICE_STATE, 0 );
	// HRESULT hr = Dx9Device()->CheckDeviceState(static_cast<HWND>(m_hWnd));
	// Possible return values include:
	// D3D_OK,
	// D3DERR_DEVICELOST,
	// D3DERR_DEVICEHUNG,
	// D3DERR_DEVICEREMOVED,
	// D3DERR_OUTOFVIDEOMEMORY
	// S_PRESENT_MODE_CHANGED or S_PRESENT_OCCLUDED
	// if ( hr == D3DERR_DEVICELOST )
	// {
	// 	MarkDeviceLost();
	// 	return;
	// }
	// 
	// if ( hr == D3DERR_DEVICEHUNG )
	// {
	// 	MarkDeviceHung();
	// 	return;
	// }

	RECORD_COMMAND( DX8_SET_RENDER_TARGET, 3 );
	RECORD_INT( nRenderTargetID );
	RECORD_INT( colorTextureHandle );
	RECORD_INT( depthTextureHandle );

	// The 0th render target defines which depth buffer we are using, so 
	// don't bother if we are another render target
	if ( nRenderTargetID > 0 )
	{
		depthTextureHandle = SHADER_RENDERTARGET_NONE;
	}

	se::win::com::com_ptr<IDirect3DSurface9> pColorSurface;
	// NOTE!!!!  If this code changes, also change Dx8SetRenderTarget in playback.cpp
	bool usingTextureTarget = false;

	if ( colorTextureHandle == SHADER_RENDERTARGET_BACKBUFFER )
	{
		pColorSurface = m_pBackBufferSurface;
	}
	else
	{
		UnbindTexture( colorTextureHandle );

		// get the texture
		pColorSurface = std::move( GetTextureSurface( colorTextureHandle ) );
		if ( !pColorSurface )
		{
			return;
		}

		usingTextureTarget = true;
	}

	HRESULT hr;

	se::win::com::com_ptr<IDirect3DSurface9> pZSurface;
	if ( depthTextureHandle == SHADER_RENDERTARGET_DEPTHBUFFER )
	{
		// using the default depth buffer
		pZSurface = m_pZBufferSurface;
	}
	else if ( depthTextureHandle != SHADER_RENDERTARGET_NONE )
	{
		UnbindTexture( depthTextureHandle );

		Texture_t &tex = GetTexture( depthTextureHandle );
		if ( tex.m_Flags & Texture_t::IS_DEPTH_STENCIL )
		{
			pZSurface = std::move( GetDepthTextureSurface( depthTextureHandle ) );
		}
		else
		{
			hr = ((IDirect3DTexture9*)tex.GetTexture())->GetSurfaceLevel( 0, &pZSurface );
			// dimhotepus: Check z surface level present. 
			Assert( SUCCEEDED(hr) );
		}

		if ( !pZSurface )
		{
			return;
		}

		usingTextureTarget = true;
	}

#ifdef _DEBUG
	if ( pZSurface )
	{
		D3DSURFACE_DESC zSurfaceDesc;
		hr = pZSurface->GetDesc( &zSurfaceDesc );
		Assert( SUCCEEDED(hr) );

		D3DSURFACE_DESC colorSurfaceDesc;
		hr = pColorSurface->GetDesc( &colorSurfaceDesc );
		Assert( SUCCEEDED(hr) );

		if ( !HushAsserts() )
		{
			Assert( colorSurfaceDesc.Width <= zSurfaceDesc.Width );
			Assert( colorSurfaceDesc.Height <= zSurfaceDesc.Height );
		}
	}
#endif

	// we only set this flag for the 0th render target so that NULL
	// render targets 1-3 don't mess with the viewport on the main RT
	if ( nRenderTargetID == 0 )
		m_UsingTextureRenderTarget = usingTextureTarget;

	// NOTE: The documentation says that SetRenderTarget increases the refcount
	// but it doesn't appear to in practice. If this somehow changes (perhaps
	// in a device-specific manner, we're in trouble).
	if ( pColorSurface == m_pBackBufferSurface && nRenderTargetID > 0 )
	{
		// SetRenderTargetEx is overloaded so that if you pass NULL in for anything that
		// isn't the zeroth render target, you effectively disable that MRT index.
		// (Passing in NULL for the zeroth render target means that you want to use the backbuffer
		// as the render target.)
		// 
		// If nullptr, the color buffer for the corresponding RenderTargetIndex is disabled.
		// 
		// hack hack hack!!!!!  If the render target id > 0 and the user passed in NULL, disable the render target
		hr = Dx9Device()->SetRenderTarget( nRenderTargetID, nullptr );
		Assert( SUCCEEDED(hr) );
	}
	else
	{
		hr = Dx9Device()->SetRenderTarget( nRenderTargetID, pColorSurface );
		Assert( SUCCEEDED(hr) );
	}

	// The 0th render target defines which depth buffer we are using, so 
	// don't bother if we are another render target
	if ( nRenderTargetID == 0 )
	{
		hr = Dx9Device()->SetDepthStencilSurface( pZSurface );
		Assert( SUCCEEDED(hr) );
	}

	// The 0th render target defines the viewport, therefore it also defines
	// the viewport limits.
	if ( m_UsingTextureRenderTarget && nRenderTargetID == 0 )
	{
		D3DSURFACE_DESC  desc;
		hr = !pZSurface ? pColorSurface->GetDesc( &desc ) : pZSurface->GetDesc( &desc );
		Assert( !FAILED(hr) );

		m_ViewportMaxWidth = desc.Width;
		m_ViewportMaxHeight = desc.Height;
	}

	// Changing the render target sets a default viewport.  Force rewrite to preserve the current desired state.
	m_DynamicState.m_Viewport.X = 0;
	m_DynamicState.m_Viewport.Y = 0;
	m_DynamicState.m_Viewport.Width = (DWORD)-1;
	m_DynamicState.m_Viewport.Height = (DWORD)-1;

	ADD_COMMIT_FUNC( COMMIT_PER_DRAW, COMMIT_ALWAYS, CommitSetViewports );
}


//-----------------------------------------------------------------------------
// Changes the render target
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetRenderTarget( ShaderAPITextureHandle_t colorTextureHandle, ShaderAPITextureHandle_t depthTextureHandle )
{
	LOCK_SHADERAPI();
	SetRenderTargetEx( 0, colorTextureHandle, depthTextureHandle );
}

//-----------------------------------------------------------------------------
// Returns the nearest supported format
//-----------------------------------------------------------------------------
ImageFormat CShaderAPIDx8::GetNearestSupportedFormat( ImageFormat fmt, bool bFilteringRequired /* = true */ ) const
{
	return FindNearestSupportedFormat( fmt, false, false, bFilteringRequired );
}


ImageFormat CShaderAPIDx8::GetNearestRenderTargetFormat( ImageFormat fmt ) const
{
	return FindNearestSupportedFormat( fmt, false, true, false );
}


bool CShaderAPIDx8::DoRenderTargetsNeedSeparateDepthBuffer() const
{
	LOCK_SHADERAPI();
	return m_PresentParameters.MultiSampleType != D3DMULTISAMPLE_NONE;
}


//-----------------------------------------------------------------------------
// Indicates we're modifying a texture
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ModifyTexture( ShaderAPITextureHandle_t textureHandle )
{
	tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "%s", __FUNCTION__ );

	LOCK_SHADERAPI();
	// Can't do this if we're locked!
	Assert( m_ModifyTextureLockedLevel < 0 );

	AssertValidTextureHandle( textureHandle );
	m_ModifyTextureHandle = textureHandle;
	
	// If we're got a multi-copy texture, we need to up the current copy count
	Texture_t& tex = GetTexture( textureHandle );
	if (tex.m_NumCopies > 1)
	{
		// Each time we modify a texture, we'll want to switch texture
		// as soon as a TexImage2D call is made...
		tex.m_SwitchNeeded = true;
	}
}


//-----------------------------------------------------------------------------
// Advances the current copy of a texture...
//-----------------------------------------------------------------------------
void CShaderAPIDx8::AdvanceCurrentCopy( ShaderAPITextureHandle_t hTexture )
{
	// May need to switch textures....
	Texture_t& tex = GetTexture( hTexture );
	if (tex.m_NumCopies > 1)
	{
		if (++tex.m_CurrentCopy >= tex.m_NumCopies)
			tex.m_CurrentCopy = 0;

		// When the current copy changes, we need to make sure this texture
		// isn't bound to any stages any more; thereby guaranteeing the new
		// copy will be re-bound.
		UnbindTexture( hTexture );
	}
}


//-----------------------------------------------------------------------------
// Locks, unlocks the current texture
//-----------------------------------------------------------------------------

bool CShaderAPIDx8::TexLock( int level, int cubeFaceID, int xOffset, int yOffset, 
								int width, int height, CPixelWriter& writer )
{
	tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "%s", __FUNCTION__ );

	LOCK_SHADERAPI();

	Assert( m_ModifyTextureLockedLevel < 0 );

	ShaderAPITextureHandle_t hTexture = GetModifyTextureHandle();
	if ( !m_Textures.IsValidIndex( hTexture ) )
		return false;

	// Blow off mip levels if we don't support mipmapping
	if ( !g_pHardwareConfig->SupportsMipmapping() && ( level > 0 ) )
		return false;

	// This test here just makes sure we don't try to download mipmap levels
	// if we weren't able to create them in the first place
	Texture_t& tex = GetTexture( hTexture );
	if ( level >= tex.m_NumLevels  )
	{
		return false;
	}

	// May need to switch textures....
	if ( tex.m_SwitchNeeded )
	{
		AdvanceCurrentCopy( hTexture );
		tex.m_SwitchNeeded = false;
	}

	IDirect3DBaseTexture *pTexture = GetModifyTexture();

	bool bOK = LockTexture( hTexture, tex.m_CurrentCopy, pTexture,
		level, (D3DCUBEMAP_FACES)cubeFaceID, xOffset, yOffset, width, height, false, writer );
	if ( bOK )
	{
		m_ModifyTextureLockedLevel = level;
		m_ModifyTextureLockedFace = cubeFaceID;
	}
	return bOK;
}

void CShaderAPIDx8::TexUnlock( )
{
	tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "%s", __FUNCTION__ );

	LOCK_SHADERAPI();
	if ( m_ModifyTextureLockedLevel >= 0 )
	{
		Texture_t& tex = GetTexture( GetModifyTextureHandle() );
		UnlockTexture( GetModifyTextureHandle(), tex.m_CurrentCopy, GetModifyTexture(),
			m_ModifyTextureLockedLevel, (D3DCUBEMAP_FACES)m_ModifyTextureLockedFace );

		m_ModifyTextureLockedLevel = -1;
	}
}

//-----------------------------------------------------------------------------
// Texture image upload
//-----------------------------------------------------------------------------
void CShaderAPIDx8::TexImage2D( 
	int			level,
	int			cubeFaceID, 
	ImageFormat	dstFormat, 
	int			z, 
	int			width, 
	int			height, 
	ImageFormat srcFormat, 
	bool		bSrcIsTiled,
	void		*pSrcData )
{
	LOCK_SHADERAPI();

	Assert( pSrcData );
	AssertValidTextureHandle( GetModifyTextureHandle() );

	if ( !m_Textures.IsValidIndex( GetModifyTextureHandle() ) )
	{
		return;
	}

	Assert( (width <= g_pHardwareConfig->Caps().m_MaxTextureWidth) && (height <= g_pHardwareConfig->Caps().m_MaxTextureHeight) );

	// Blow off mip levels if we don't support mipmapping
	if ( !g_pHardwareConfig->SupportsMipmapping() && (level > 0))
	{
		return;
	}

	// This test here just makes sure we don't try to download mipmap levels
	// if we weren't able to create them in the first place
	Texture_t& tex = GetTexture( GetModifyTextureHandle() );
	if ( level >= tex.m_NumLevels )
	{
		return;
	}

	// May need to switch textures....
	if (tex.m_SwitchNeeded)
	{
		AdvanceCurrentCopy( GetModifyTextureHandle() );
		tex.m_SwitchNeeded = false;
	}

	TextureLoadInfo_t info;
	info.m_TextureHandle = GetModifyTextureHandle();
	info.m_pTexture = GetModifyTexture();
	info.m_nLevel = level;
	info.m_nCopy = tex.m_CurrentCopy;
	info.m_CubeFaceID = (D3DCUBEMAP_FACES)cubeFaceID;
	info.m_nWidth = width;
	info.m_nHeight = height;
	info.m_nZOffset = z;
	info.m_SrcFormat = srcFormat;
	info.m_pSrcData = (unsigned char *)pSrcData;
	info.m_bTextureIsLockable = ( tex.m_Flags & Texture_t::IS_LOCKABLE ) != 0;

	LoadTexture( info );
	SetModifyTexture( info.m_pTexture );
}

//-----------------------------------------------------------------------------
// Upload to a sub-piece of a texture
//-----------------------------------------------------------------------------
void CShaderAPIDx8::TexSubImage2D( 
	int			level, 
	int			cubeFaceID,
	int			xOffset,
	int			yOffset, 
	int			zOffset,
	int			width,
	int			height,
	ImageFormat srcFormat,
	int			srcStride,
	bool		bSrcIsTiled,
	void		*pSrcData )
{
	LOCK_SHADERAPI();

	Assert( pSrcData );
	AssertValidTextureHandle( GetModifyTextureHandle() );

	if ( !m_Textures.IsValidIndex( GetModifyTextureHandle() ) )
	{
		return;
	}

	// Blow off mip levels if we don't support mipmapping
	if ( !g_pHardwareConfig->SupportsMipmapping() && ( level > 0 ) )
	{
		return;
	}

	// NOTE: This can only be done with procedural textures if this method is
	// being used to download the entire texture, cause last frame's partial update
	// may be in a completely different texture! Sadly, I don't have all of the
	// information I need, but I can at least check a couple things....
#ifdef _DEBUG
	if ( GetTexture( GetModifyTextureHandle() ).m_NumCopies > 1 )
	{
		Assert( (xOffset == 0) && (yOffset == 0) );
	}
#endif

	// This test here just makes sure we don't try to download mipmap levels
	// if we weren't able to create them in the first place
	Texture_t& tex = GetTexture( GetModifyTextureHandle() );
	if ( level >= tex.m_NumLevels )
	{
		return;
	}

	// May need to switch textures....
	if ( tex.m_SwitchNeeded )
	{
		AdvanceCurrentCopy( GetModifyTextureHandle() );
		tex.m_SwitchNeeded = false;
	}

	TextureLoadInfo_t info;
	info.m_TextureHandle = GetModifyTextureHandle();
	info.m_pTexture = GetModifyTexture();
	info.m_nLevel = level;
	info.m_nCopy = tex.m_CurrentCopy;
	info.m_CubeFaceID = (D3DCUBEMAP_FACES)cubeFaceID;
	info.m_nWidth = width;
	info.m_nHeight = height;
	info.m_nZOffset = zOffset;
	info.m_SrcFormat = srcFormat;
	info.m_pSrcData = (unsigned char *)pSrcData;
	info.m_bTextureIsLockable = ( tex.m_Flags & Texture_t::IS_LOCKABLE ) != 0;

	LoadSubTexture( info, xOffset, yOffset, srcStride );
}

//-----------------------------------------------------------------------------
// Volume texture upload
//-----------------------------------------------------------------------------
void CShaderAPIDx8::TexImageFromVTF( IVTFTexture *pVTF, int iVTFFrame )
{
	LOCK_SHADERAPI();
	Assert( pVTF );
	AssertValidTextureHandle( GetModifyTextureHandle() );
	if ( !m_Textures.IsValidIndex( GetModifyTextureHandle() ) )
	{
		return;
	}
	Texture_t& tex = GetTexture( GetModifyTextureHandle() );

	// May need to switch textures....
	if (tex.m_SwitchNeeded)
	{
		AdvanceCurrentCopy( GetModifyTextureHandle() );
		tex.m_SwitchNeeded = false;
	}

	TextureLoadInfo_t info;
	info.m_TextureHandle = GetModifyTextureHandle();
	info.m_pTexture = GetModifyTexture();
	info.m_nLevel = 0;
	info.m_nCopy = tex.m_CurrentCopy;
	info.m_CubeFaceID = (D3DCUBEMAP_FACES)0;
	info.m_nWidth = 0;
	info.m_nHeight = 0;
	info.m_nZOffset = 0;
	info.m_SrcFormat = pVTF->Format();
	info.m_pSrcData = NULL;
	info.m_bTextureIsLockable = ( tex.m_Flags & Texture_t::IS_LOCKABLE ) != 0;
	if ( pVTF->Depth() > 1 )
	{
		LoadVolumeTextureFromVTF( info, pVTF, iVTFFrame );
	}
	else if ( pVTF->IsCubeMap() )
	{
		if ( HardwareConfig()->SupportsCubeMaps() )
		{
			LoadCubeTextureFromVTF( info, pVTF, iVTFFrame );
		}
		else
		{
			info.m_CubeFaceID = (D3DCUBEMAP_FACES)6;
			LoadTextureFromVTF( info, pVTF, iVTFFrame );
		}
	}
	else
	{
		LoadTextureFromVTF( info, pVTF, iVTFFrame );
	}
	SetModifyTexture( info.m_pTexture );
}


//-----------------------------------------------------------------------------
// Is the texture resident?
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::IsTextureResident( ShaderAPITextureHandle_t textureHandle )
{
	return true;
}


//-----------------------------------------------------------------------------
// Level of anisotropic filtering
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetAnisotropicLevel( int nAnisotropyLevel )
{
	LOCK_SHADERAPI();

	// NOTE: This must be called before the rest of the code in this function so
	//       anisotropic can be set per-texture to force it on! This will also avoid
	//       a possible infinite loop that existed before.
	g_pShaderUtil->NoteAnisotropicLevel( nAnisotropyLevel );

	// Never set this to 1. In the case we want it set to 1, we will use this to override
	//   aniso per-texture, so set it to something reasonable
	if ( nAnisotropyLevel > g_pHardwareConfig->Caps().m_nMaxAnisotropy || nAnisotropyLevel <= 1 )
	{
		// Set it to 1/4 the max but between 2-8
		nAnisotropyLevel = max( 2, min( 8, ( g_pHardwareConfig->Caps().m_nMaxAnisotropy / 4 ) ) );
	}

	// Set the D3D max insotropy state for all samplers
	for ( int i = 0; i < g_pHardwareConfig->Caps().m_NumSamplers; ++i)
	{
		SamplerState(i).m_nAnisotropicLevel = nAnisotropyLevel;
		SetSamplerState( i, D3DSAMP_MAXANISOTROPY, SamplerState(i).m_nAnisotropicLevel );
	}
}


//-----------------------------------------------------------------------------
// Sets the priority
//-----------------------------------------------------------------------------
void CShaderAPIDx8::TexSetPriority( int priority )
{
	LOCK_SHADERAPI();

	// A hint to the cacher...
	ShaderAPITextureHandle_t hModifyTexture = GetModifyTextureHandle();
	if ( hModifyTexture == INVALID_SHADERAPI_TEXTURE_HANDLE )
		return;

	Texture_t& tex = GetTexture( hModifyTexture );
	if ( tex.m_NumCopies > 1 )
	{
		for (int i = 0; i < tex.m_NumCopies; ++i)
			tex.GetTexture( i )->SetPriority( priority );
	}
	else
	{
		tex.GetTexture()->SetPriority( priority );
	}
}

void CShaderAPIDx8::TexLodClamp( int finest )
{
	LOCK_SHADERAPI();

	ShaderAPITextureHandle_t hModifyTexture = GetModifyTextureHandle();
	if ( hModifyTexture == INVALID_SHADERAPI_TEXTURE_HANDLE )
		return;

	Texture_t& tex = GetTexture( hModifyTexture );
	tex.m_FinestMipmapLevel = finest;
}

void CShaderAPIDx8::TexLodBias( float bias )
{
	LOCK_SHADERAPI();

	ShaderAPITextureHandle_t hModifyTexture = GetModifyTextureHandle();
	if ( hModifyTexture == INVALID_SHADERAPI_TEXTURE_HANDLE )
		return;

	Texture_t& tex = GetTexture( hModifyTexture );
	tex.m_LodBias = bias;
}



//-----------------------------------------------------------------------------
// Texturemapping state
//-----------------------------------------------------------------------------
void CShaderAPIDx8::TexWrap( ShaderTexCoordComponent_t coord, ShaderTexWrapMode_t wrapMode )
{
	LOCK_SHADERAPI();

	ShaderAPITextureHandle_t hModifyTexture = GetModifyTextureHandle();
	if ( hModifyTexture == INVALID_SHADERAPI_TEXTURE_HANDLE )
		return;

	D3DTEXTUREADDRESS address;
	switch( wrapMode )
	{
	case SHADER_TEXWRAPMODE_CLAMP:
		address = D3DTADDRESS_CLAMP;
		break;
	case SHADER_TEXWRAPMODE_REPEAT:
		address = D3DTADDRESS_WRAP;
		break; 
	case SHADER_TEXWRAPMODE_BORDER:
		address = D3DTADDRESS_BORDER;
		break; 
	default:
		address = D3DTADDRESS_CLAMP;
		Warning( "CShaderAPIDx8::TexWrap: unknown wrapMode\n" );
		break;
	}

	switch( coord )
	{
	case SHADER_TEXCOORD_S:
		GetTexture( hModifyTexture ).m_UTexWrap = address;
		break;
	case SHADER_TEXCOORD_T:
		GetTexture( hModifyTexture ).m_VTexWrap = address;
		break;
	case SHADER_TEXCOORD_U:
		GetTexture( hModifyTexture ).m_WTexWrap = address;
		break;
	default:
		Warning( "CShaderAPIDx8::TexWrap: unknown coord\n" );
		break;
	}
}

void CShaderAPIDx8::TexMinFilter( ShaderTexFilterMode_t texFilterMode )
{
	LOCK_SHADERAPI();

	ShaderAPITextureHandle_t hModifyTexture = GetModifyTextureHandle();
	if ( hModifyTexture == INVALID_SHADERAPI_TEXTURE_HANDLE )
		return;

	switch( texFilterMode )
	{
	case SHADER_TEXFILTERMODE_NEAREST:
		GetTexture( hModifyTexture ).m_MinFilter = D3DTEXF_POINT;
		GetTexture( hModifyTexture ).m_MipFilter = D3DTEXF_NONE;
		break;
	case SHADER_TEXFILTERMODE_LINEAR:
		GetTexture( hModifyTexture ).m_MinFilter = D3DTEXF_LINEAR;
		GetTexture( hModifyTexture ).m_MipFilter = D3DTEXF_NONE;
		break;
	case SHADER_TEXFILTERMODE_NEAREST_MIPMAP_NEAREST:
		GetTexture( hModifyTexture ).m_MinFilter = D3DTEXF_POINT;
		GetTexture( hModifyTexture ).m_MipFilter = 
			GetTexture( hModifyTexture ).m_NumLevels != 1 ? D3DTEXF_POINT : D3DTEXF_NONE;
		break;
	case SHADER_TEXFILTERMODE_LINEAR_MIPMAP_NEAREST:
		GetTexture( hModifyTexture ).m_MinFilter = D3DTEXF_LINEAR;
		GetTexture( hModifyTexture ).m_MipFilter = 
			GetTexture( hModifyTexture ).m_NumLevels != 1 ? D3DTEXF_POINT : D3DTEXF_NONE;
		break;
	case SHADER_TEXFILTERMODE_NEAREST_MIPMAP_LINEAR:
		GetTexture( hModifyTexture ).m_MinFilter = D3DTEXF_POINT;
		GetTexture( hModifyTexture ).m_MipFilter = 
			GetTexture( hModifyTexture ).m_NumLevels != 1 ? D3DTEXF_LINEAR : D3DTEXF_NONE;
		break;
	case SHADER_TEXFILTERMODE_LINEAR_MIPMAP_LINEAR:
		GetTexture( hModifyTexture ).m_MinFilter = D3DTEXF_LINEAR;
		GetTexture( hModifyTexture ).m_MipFilter = 
			GetTexture( hModifyTexture ).m_NumLevels != 1 ? D3DTEXF_LINEAR : D3DTEXF_NONE;
		break;
	case SHADER_TEXFILTERMODE_ANISOTROPIC:
		GetTexture( hModifyTexture ).m_MinFilter = D3DTEXF_ANISOTROPIC;
		GetTexture( hModifyTexture ).m_MipFilter = 
			GetTexture( hModifyTexture ).m_NumLevels != 1 ? D3DTEXF_LINEAR : D3DTEXF_NONE;
		break;
	default:
		Warning( "CShaderAPIDx8::TexMinFilter: Unknown texFilterMode\n" );
		break;
	}
}

void CShaderAPIDx8::TexMagFilter( ShaderTexFilterMode_t texFilterMode )
{
	LOCK_SHADERAPI();

	ShaderAPITextureHandle_t hModifyTexture = GetModifyTextureHandle();
	if ( hModifyTexture == INVALID_SHADERAPI_TEXTURE_HANDLE )
		return;

	switch( texFilterMode )
	{
	case SHADER_TEXFILTERMODE_NEAREST:
		GetTexture( hModifyTexture ).m_MagFilter = D3DTEXF_POINT;
		break;
	case SHADER_TEXFILTERMODE_LINEAR:
		GetTexture( hModifyTexture ).m_MagFilter = D3DTEXF_LINEAR;
		break;
	case SHADER_TEXFILTERMODE_NEAREST_MIPMAP_NEAREST:
		Warning( "CShaderAPIDx8::TexMagFilter: SHADER_TEXFILTERMODE_NEAREST_MIPMAP_NEAREST is invalid\n" );
		break;
	case SHADER_TEXFILTERMODE_LINEAR_MIPMAP_NEAREST:
		Warning( "CShaderAPIDx8::TexMagFilter: SHADER_TEXFILTERMODE_LINEAR_MIPMAP_NEAREST is invalid\n" );
		break;
	case SHADER_TEXFILTERMODE_NEAREST_MIPMAP_LINEAR:
		Warning( "CShaderAPIDx8::TexMagFilter: SHADER_TEXFILTERMODE_NEAREST_MIPMAP_LINEAR is invalid\n" );
		break;
	case SHADER_TEXFILTERMODE_LINEAR_MIPMAP_LINEAR:
		Warning( "CShaderAPIDx8::TexMagFilter: SHADER_TEXFILTERMODE_LINEAR_MIPMAP_LINEAR is invalid\n" );
		break;
	case SHADER_TEXFILTERMODE_ANISOTROPIC:
		GetTexture( hModifyTexture ).m_MagFilter = g_pHardwareConfig->Caps().m_bSupportsMagAnisotropicFiltering ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR;
		break;
	default:
		Warning( "CShaderAPIDx8::TexMAGFilter: Unknown texFilterMode\n" );
		break;
	}
}


//-----------------------------------------------------------------------------
// Gets the matrix stack from the matrix mode
//-----------------------------------------------------------------------------

int CShaderAPIDx8::GetMatrixStack( MaterialMatrixMode_t mode ) const
{
	Assert( mode >= 0 && mode < NUM_MATRIX_MODES );
	return mode;
}

//-----------------------------------------------------------------------------
// Returns true if we're modulating constant color into the vertex color
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::IsModulatingVertexColor() const
{
	return m_TransitionTable.CurrentShadowShaderState()->m_ModulateConstantColor;
}


//-----------------------------------------------------------------------------
// Material property (used to deal with overbright for lights)
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetDefaultMaterial()
{
	D3DMATERIAL mat;
	mat.Diffuse.r = mat.Diffuse.g = mat.Diffuse.b = mat.Diffuse.a = 1.0f;
	mat.Ambient.r = mat.Ambient.g = mat.Ambient.b = mat.Ambient.a = 0.0f;
	mat.Specular.r = mat.Specular.g = mat.Specular.b = mat.Specular.a = 0.0f;
	mat.Emissive.r = mat.Emissive.g = mat.Emissive.b = mat.Emissive.a = 0.0f;
	mat.Power = 1.0f;
	Dx9Device()->SetMaterial( &mat );
}

//-----------------------------------------------------------------------------
// lighting related methods
//-----------------------------------------------------------------------------

void CShaderAPIDx8::SetAmbientLight( float r, float g, float b )
{
	LOCK_SHADERAPI();
	unsigned int ambient = D3DCOLOR_ARGB( 255, (int)(r * 255), 
		(int)(g * 255), (int)(b * 255) );
	if (ambient != m_DynamicState.m_Ambient)
	{
		m_DynamicState.m_Ambient = ambient;
		SetSupportedRenderState( D3DRS_AMBIENT, ambient );
	}
}

void CShaderAPIDx8::SetLightingOrigin( Vector vLightingOrigin )
{
	if ( vLightingOrigin != m_DynamicState.m_vLightingOrigin )
	{
		FlushBufferedPrimitives();
		m_DynamicState.m_vLightingOrigin = vLightingOrigin;
	}
}

//#define NO_LOCAL_LIGHTS
void CShaderAPIDx8::SetLight( int lightNum, const LightDesc_t& desc_ )
{
	LOCK_SHADERAPI();
#ifdef NO_LOCAL_LIGHTS
	LightDesc_t desc = desc_;
	desc.m_Type = MATERIAL_LIGHT_DISABLE;
#else
	LightDesc_t &desc = const_cast<LightDesc_t &>(desc_); // to permit '&'
#endif
	Assert( lightNum < g_pHardwareConfig->Caps().m_MaxNumLights && lightNum >= 0 );

	if( lightNum >= g_pHardwareConfig->Caps().m_MaxNumLights || lightNum < 0 )
		return;
	
	m_DynamicState.m_LightDescs[lightNum] = desc;
	
	FlushBufferedPrimitives();

	if (desc.m_Type == MATERIAL_LIGHT_DISABLE)
	{
		if (m_DynamicState.m_LightEnable[lightNum])
		{
			m_DynamicState.m_LightEnableChanged[lightNum] = STATE_CHANGED;
			m_DynamicState.m_LightEnable[lightNum] = false;
		}
		return;
	}

	if (!m_DynamicState.m_LightEnable[lightNum])
	{
		m_DynamicState.m_LightEnableChanged[lightNum] = STATE_CHANGED;
		m_DynamicState.m_LightEnable[lightNum] = true;
	}

	D3DLIGHT light = {};
	switch( desc.m_Type )
	{
	case MATERIAL_LIGHT_POINT:
		light.Type = D3DLIGHT_POINT;
		light.Range = desc.m_Range;
		break;

	case MATERIAL_LIGHT_DIRECTIONAL:
		light.Type = D3DLIGHT_DIRECTIONAL;
		light.Range = 1e12;	// This is supposed to be ignored
		break;

	case MATERIAL_LIGHT_SPOT:
		light.Type = D3DLIGHT_SPOT;
		light.Range = desc.m_Range;
		break;

	default:
		m_DynamicState.m_LightEnable[lightNum] = false;
		return;
	}

	// This is a D3D limitation
	Assert( (light.Range >= 0) && (light.Range <= sqrt(FLT_MAX)) );

	memcpy( &light.Diffuse, &desc.m_Color[0], 3*sizeof(float) ); //-V1086
	memcpy( &light.Specular, &desc.m_Color[0], 3*sizeof(float) ); //-V1086
	light.Diffuse.a = 1.0f;
	light.Specular.a = 1.0f;
	light.Ambient.a = light.Ambient.b = light.Ambient.g = light.Ambient.r = 0;
	memcpy( &light.Position, &desc.m_Position[0], 3 * sizeof(float) );
	memcpy( &light.Direction, &desc.m_Direction[0], 3 * sizeof(float) );
	light.Falloff = desc.m_Falloff;
	light.Attenuation0 = desc.m_Attenuation0;
	light.Attenuation1 = desc.m_Attenuation1;
	light.Attenuation2 = desc.m_Attenuation2;

	// normalize light color...
	light.Theta = desc.m_Theta;
	light.Phi = desc.m_Phi;
	if (light.Phi > M_PI_F)
		light.Phi = M_PI_F;

	// This piece of crap line of code is because if theta gets too close to phi,
	// we get no light at all.
	if (light.Theta - light.Phi > -1e-3f)
		light.Theta = light.Phi - 1e-3f;

	m_DynamicState.m_LightChanged[lightNum] = STATE_CHANGED;
	memcpy( &m_DynamicState.m_Lights[lightNum], &light, sizeof(light) );
}

void CShaderAPIDx8::DisableAllLocalLights()
{
	LOCK_SHADERAPI();
	bool bFlushed = false;
	for ( int lightNum = 0; lightNum < MAX_NUM_LIGHTS; lightNum++ )
	{
		if (m_DynamicState.m_LightEnable[lightNum])
		{
			if ( !bFlushed )
			{
				FlushBufferedPrimitives();
				bFlushed = true;
			}
			m_DynamicState.m_LightDescs[lightNum].m_Type = MATERIAL_LIGHT_DISABLE;
			m_DynamicState.m_LightEnableChanged[lightNum] = STATE_CHANGED;
			m_DynamicState.m_LightEnable[lightNum] = false;
		}
	}
}

int CShaderAPIDx8::GetMaxLights( void ) const
{
	return g_pHardwareConfig->Caps().m_MaxNumLights;
}

const LightDesc_t& CShaderAPIDx8::GetLight( int lightNum ) const
{
	Assert( lightNum < g_pHardwareConfig->Caps().m_MaxNumLights && lightNum >= 0 );
	return m_DynamicState.m_LightDescs[lightNum];
}

//-----------------------------------------------------------------------------
// Ambient cube 
//-----------------------------------------------------------------------------

//#define NO_AMBIENT_CUBE 1
void CShaderAPIDx8::SetAmbientLightCube( Vector4D cube[6] )
{
	LOCK_SHADERAPI();
/*
	int i;
	for( i = 0; i < 6; i++ )
	{
		ColorClamp( cube[i].AsVector3D() );
//		if( i == 0 )
//		{
//			Warning( "%d: %f %f %f\n", i, cube[i][0], cube[i][1], cube[i][2] );
//		}
	}
*/
	if (memcmp(&m_DynamicState.m_AmbientLightCube[0][0], cube, 6 * sizeof(Vector4D)))
	{
		memcpy( &m_DynamicState.m_AmbientLightCube[0][0], cube, 6 * sizeof(Vector4D) );

#ifdef NO_AMBIENT_CUBE
		memset( &m_DynamicState.m_AmbientLightCube[0][0], 0, 6 * sizeof(Vector4D) );
#endif

//#define DEBUG_AMBIENT_CUBE

#ifdef DEBUG_AMBIENT_CUBE
		m_DynamicState.m_AmbientLightCube[0][0] = 1.0f;
		m_DynamicState.m_AmbientLightCube[0][1] = 0.0f;
		m_DynamicState.m_AmbientLightCube[0][2] = 0.0f;

		m_DynamicState.m_AmbientLightCube[1][0] = 0.0f;
		m_DynamicState.m_AmbientLightCube[1][1] = 1.0f;
		m_DynamicState.m_AmbientLightCube[1][2] = 0.0f;

		m_DynamicState.m_AmbientLightCube[2][0] = 0.0f;
		m_DynamicState.m_AmbientLightCube[2][1] = 0.0f;
		m_DynamicState.m_AmbientLightCube[2][2] = 1.0f;

		m_DynamicState.m_AmbientLightCube[3][0] = 1.0f;
		m_DynamicState.m_AmbientLightCube[3][1] = 0.0f;
		m_DynamicState.m_AmbientLightCube[3][2] = 1.0f;

		m_DynamicState.m_AmbientLightCube[4][0] = 1.0f;
		m_DynamicState.m_AmbientLightCube[4][1] = 1.0f;
		m_DynamicState.m_AmbientLightCube[4][2] = 0.0f;

		m_DynamicState.m_AmbientLightCube[5][0] = 0.0f;
		m_DynamicState.m_AmbientLightCube[5][1] = 1.0f;
		m_DynamicState.m_AmbientLightCube[5][2] = 1.0f;
#endif

		m_CachedAmbientLightCube = STATE_CHANGED;
	}
}

void CShaderAPIDx8::SetVertexShaderStateAmbientLightCube()
{
	if (m_CachedAmbientLightCube & STATE_CHANGED_VERTEX_SHADER)
	{
		SetVertexShaderConstant( VERTEX_SHADER_AMBIENT_LIGHT, m_DynamicState.m_AmbientLightCube[0].Base(), 6 );
		m_CachedAmbientLightCube &= ~STATE_CHANGED_VERTEX_SHADER;
	}
}


void CShaderAPIDx8::SetPixelShaderStateAmbientLightCube( int pshReg, bool bForceToBlack )
{
	float *pCubeBase;
	Vector4D tempCube[6];

	if( bForceToBlack )
	{
		for ( int i=0; i<6 ; i++ )
			tempCube[i].Init();
		
		pCubeBase = tempCube[0].Base();
	}
	else
	{
		pCubeBase = m_DynamicState.m_AmbientLightCube[0].Base();
	}

	SetPixelShaderConstant( pshReg, pCubeBase, 6 );
}

float CShaderAPIDx8::GetAmbientLightCubeLuminance( void )
{
	Vector4D vLuminance( 0.3f, 0.59f, 0.11f, 0.0f );
	float fLuminance = 0.0f;

	for (int i=0; i<6; i++)
	{
		fLuminance += vLuminance.Dot( m_DynamicState.m_AmbientLightCube[i] );
	}

	return fLuminance / 6.0f;
}

static inline RECT* RectToRECT( Rect_t *pSrcRect, RECT &dstRect )
{
	if ( !pSrcRect )
		return NULL;

	dstRect.left = pSrcRect->x;
	dstRect.top = pSrcRect->y;
	dstRect.right = pSrcRect->x + pSrcRect->width;
	dstRect.bottom = pSrcRect->y + pSrcRect->height;
	return &dstRect;
}

void CShaderAPIDx8::CopyRenderTargetToTextureEx( ShaderAPITextureHandle_t textureHandle, int nRenderTargetID, Rect_t *pSrcRect, Rect_t *pDstRect )
{
	LOCK_SHADERAPI();
	VPROF_BUDGET( "CShaderAPIDx8::CopyRenderTargetToTexture", "Refraction overhead" );

	if ( !TextureIsAllocated( textureHandle ) )
		return;

#if defined( PIX_INSTRUMENTATION )
	{
		const char *pRT = ( nRenderTargetID < 0 ) ? "DS" : "RT";

		if ( textureHandle == SHADER_RENDERTARGET_NONE )
		{
			pRT = "None";
		}
		else if ( textureHandle != SHADER_RENDERTARGET_BACKBUFFER )
		{
			Texture_t &tex = GetTexture( textureHandle );
			pRT = tex.m_DebugName.String();
		}

		char buf[256];
		V_sprintf_safe( buf, "CopyRTToTexture:%s", pRT ? pRT : "?" );
		BeginPIXEvent( 0xFFFFFFFF, buf );
		EndPIXEvent();
	}
#endif

	// Don't flush here!!  If you have to flush here, then there is a driver bug.
	// FlushHardware( );

	AssertValidTextureHandle( textureHandle );
	Texture_t *pTexture = &GetTexture( textureHandle );
	Assert( pTexture );
	IDirect3DTexture *pD3DTexture = (IDirect3DTexture *)pTexture->GetTexture();
	Assert( pD3DTexture );

	se::win::com::com_ptr<IDirect3DSurface> pRenderTargetSurface;
	HRESULT hr = Dx9Device()->GetRenderTarget( nRenderTargetID, &pRenderTargetSurface );
	if ( FAILED( hr ) )
	{
		Assert( 0 );
		return;
	}

	se::win::com::com_ptr<IDirect3DSurface> pDstSurf;
	hr = pD3DTexture->GetSurfaceLevel( 0, &pDstSurf );
	Assert( !FAILED( hr ) );
	if ( FAILED( hr ) )
	{
		return;
	}

	constexpr bool tryblit = true;
	if ( tryblit )
	{
		RECORD_COMMAND( DX8_COPY_FRAMEBUFFER_TO_TEXTURE, 1 );
		RECORD_INT( textureHandle );
		
		RECT srcRect, dstRect;
		hr = Dx9Device()->StretchRect( pRenderTargetSurface, RectToRECT( pSrcRect, srcRect ),
			pDstSurf, RectToRECT( pDstRect, dstRect ), D3DTEXF_LINEAR );
		Assert( !FAILED( hr ) );
	}
}

void CShaderAPIDx8::CopyRenderTargetToScratchTexture( ShaderAPITextureHandle_t srcRt, ShaderAPITextureHandle_t dstTex, Rect_t *pSrcRect, Rect_t *pDstRect )
{
	LOCK_SHADERAPI();

	if  ( !TextureIsAllocated( srcRt ) || !TextureIsAllocated( dstTex ) )
	{
		AssertMsg( false, "Fix that render target or dest texture aren't allocated." );
		return;
	}

	AssertValidTextureHandle( srcRt );
	Texture_t *pSrcRt = &GetTexture( srcRt );
	Assert( pSrcRt );
	IDirect3DTexture *pD3DSrcRt = ( IDirect3DTexture * ) pSrcRt->GetTexture();
	Assert( pD3DSrcRt );

	se::win::com::com_ptr<IDirect3DSurface9> srcSurf;
	HRESULT hr = pD3DSrcRt->GetSurfaceLevel( 0, &srcSurf );
	Assert( SUCCEEDED( hr ) && srcSurf );

	AssertValidTextureHandle( dstTex );
	Texture_t *pDstTex = &GetTexture( dstTex );
	Assert( pDstTex );
	IDirect3DTexture *pD3DDstTex = ( IDirect3DTexture * ) pDstTex->GetTexture();
	Assert( pD3DDstTex );

	se::win::com::com_ptr<IDirect3DSurface9> dstSurf;
	hr = pD3DDstTex->GetSurfaceLevel( 0, &dstSurf );
	Assert( SUCCEEDED( hr ) && dstSurf );

	// This does it.
	hr = Dx9Device()->GetRenderTargetData( srcSurf, dstSurf );
	Assert( SUCCEEDED( hr ) );
}

//-------------------------------------------------------------------------
// Allows locking and unlocking of very specific surface types. pOutBits and pOutPitch will not be touched if 
// the lock fails.
//-------------------------------------------------------------------------
void CShaderAPIDx8::LockRect( void** pOutBits, int* pOutPitch, ShaderAPITextureHandle_t texHandle, int mipmap, int x, int y, int w, int h, bool bWrite, bool bRead )
{
	LOCK_SHADERAPI();

	Assert( pOutBits );
	Assert( pOutPitch );

	if ( !TextureIsAllocated( texHandle ) )
	{
		AssertMsg( false, "Fix that texture isn't allocated." );
		return;
	}

	if ( !bRead && !bWrite )
	{
		AssertMsg( false, "Asking to neither read nor write? Probably a caller bug." );
		return;
	}

	AssertValidTextureHandle( texHandle );
	Texture_t *pTex = &GetTexture( texHandle );
	Assert( pTex );
	IDirect3DTexture *pD3DTex = ( IDirect3DTexture * ) pTex->GetTexture();
	Assert( pD3DTex );
	
	se::win::com::com_ptr<IDirect3DSurface9> surf;
	HRESULT hr = pD3DTex->GetSurfaceLevel( mipmap, &surf );
	Assert( SUCCEEDED( hr ) && surf );

	D3DLOCKED_RECT lockRect = {};
	RECT srcRect = { x, y, w, h };
	DWORD flags = 0;

	if ( bRead && !bWrite )
		flags = D3DLOCK_READONLY;

	hr = surf->LockRect( &lockRect, &srcRect, flags );
	if ( SUCCEEDED( hr ) )
	{
		(*pOutBits)  = lockRect.pBits;
		(*pOutPitch) = lockRect.Pitch;
	}
	
	AssertMsg( false, "Lock failed, look into why." );
}

void CShaderAPIDx8::UnlockRect( ShaderAPITextureHandle_t texHandle, int mipmap )
{
	LOCK_SHADERAPI();

	if ( !TextureIsAllocated( texHandle ) )
	{
		AssertMsg( false, "Fix that texture isn't allocated." );
		return;
	}

	AssertValidTextureHandle( texHandle );
	Texture_t *pTex = &GetTexture( texHandle );
	Assert( pTex );
	IDirect3DTexture *pD3DTex = ( IDirect3DTexture * ) pTex->GetTexture();
	Assert( pD3DTex );
	
	se::win::com::com_ptr<IDirect3DSurface9> surf;
	HRESULT hr = pD3DTex->GetSurfaceLevel( mipmap, &surf );
	Assert( SUCCEEDED( hr ) && surf );

	hr = surf->UnlockRect();
	Assert( SUCCEEDED( hr ) );
}

static float GetAspectRatio( const Texture_t* pTex )
{
	Assert( pTex );
	if ( pTex->m_Height != 0 )
		return float( pTex->m_Width ) / float( pTex->m_Height );

	AssertMsg( false, "Height of texture is 0, that seems like a bug." );
	return 0.0f;
}

struct TextureExtents_t
{
	int width;
	int height;
	int depth;
	int mipmaps;

	int fine;
	int coarse;

	TextureExtents_t() : width( 0 ), height( 0 ), depth( 0 ), mipmaps( 0 ), fine( 0 ), coarse( 0 ) { }
};

// Returns positive integer on success, 0 or <0 on failure.
static int FindCommonMipmapRange( int *pOutSrcFine, int *pOutDstFine, Texture_t *pSrcTex, Texture_t *pDstTex )
{
	Assert( pOutSrcFine && pOutDstFine );
	Assert( pSrcTex && pDstTex );

	if ( GetAspectRatio( pSrcTex ) != GetAspectRatio( pDstTex ) )
		return 0;

	TextureExtents_t src, 
		             dst;

	// LOD Clamp indicates that there's no actual data in the finer mipmap levels yet, so respect it when determining
	// the source and destination levels that could have data.
	const int srcLodClamp = pSrcTex->GetLodClamp();
	src.width =  Max( 1, pSrcTex->GetWidth()  >> srcLodClamp );
	src.height = Max( 1, pSrcTex->GetHeight() >> srcLodClamp );
	src.depth =  Max( 1, pSrcTex->GetDepth()  >> srcLodClamp );
	src.mipmaps = pSrcTex->m_NumLevels - srcLodClamp;
	Assert( src.mipmaps >= 1 );
	

	const int dstLodClamp = pDstTex->GetLodClamp();
	dst.width =  Max( 1, pDstTex->GetWidth()  >> dstLodClamp );
	dst.height = Max( 1, pDstTex->GetHeight() >> dstLodClamp );
	dst.depth =  Max( 1, pDstTex->GetDepth()  >> dstLodClamp );
	dst.mipmaps = pDstTex->m_NumLevels - dstLodClamp;
	Assert( dst.mipmaps >= 1 );

	TextureExtents_t *pLarger = NULL, 
		             *pSmaller = NULL;

	if ( src.width >= dst.width && src.height >= dst.height && src.depth >= dst.depth )
	{
		pLarger = &src;
		pSmaller = &dst;
	}
	else
	{
		pLarger = &dst;
		pSmaller = &src;
	}

	// Since we are same aspect ratio, only need to test one dimension
	while ( ( pLarger->width >> pLarger->fine ) > pSmaller->width )
	{
		++pLarger->fine;
		--pLarger->mipmaps;
	}

	( *pOutSrcFine ) = src.fine;
	( *pOutDstFine ) = dst.fine;

	return Min( src.mipmaps, dst.mipmaps );
}

void CShaderAPIDx8::CopyTextureToTexture( ShaderAPITextureHandle_t srcTex, ShaderAPITextureHandle_t dstTex )
{
	LOCK_SHADERAPI();

	AssertValidTextureHandle( srcTex );
	AssertValidTextureHandle( dstTex );

	Assert( TextureIsAllocated( srcTex ) && TextureIsAllocated( dstTex ) );

	Texture_t *pSrcTex = &GetTexture( srcTex );
	Texture_t *pDstTex = &GetTexture( dstTex );

	Assert( pSrcTex && pDstTex );

	// Must have same image format
	Assert( pSrcTex->GetImageFormat() == pDstTex->GetImageFormat() );

	int srcFine = 0,
		dstFine = 0;
	
	int mipmapCount = FindCommonMipmapRange( &srcFine, &dstFine, pSrcTex, pDstTex );
	
	if ( mipmapCount <= 0 )
	{
		// This is legit for things that are streamed in that are very small (near the 32x32 cutoff we do at the 
		// tip of the mipmap pyramid). But leaving it here because it's useful if you're tracking a specific bug.
		// Warning( "Attempted to copy textures that had non-overlapping mipmap pyramids. This has failed and no copy has taken place.\n" );
		return;
	}

	IDirect3DTexture* pSrcD3DTex = ( IDirect3DTexture * ) pSrcTex->GetTexture();
	IDirect3DTexture* pDstD3DTex = ( IDirect3DTexture * ) pDstTex->GetTexture();

	HRESULT hr = S_OK;
	for ( int i = 0; i < mipmapCount; ++i )
	{
		int srcMipmap = srcFine + i;
		int dstMipmap = dstFine + i;

		se::win::com::com_ptr<IDirect3DSurface9> pSrcSurf;
		hr = pSrcD3DTex->GetSurfaceLevel( srcMipmap, &pSrcSurf );
		Assert( SUCCEEDED( hr ) );
		
		se::win::com::com_ptr<IDirect3DSurface9> pDstSurf;
		hr = pDstD3DTex->GetSurfaceLevel( dstMipmap, &pDstSurf );
		Assert( SUCCEEDED( hr ) );

		hr = Dx9Device()->StretchRect( pSrcSurf, NULL, pDstSurf, NULL, D3DTEXF_NONE );
		Assert( SUCCEEDED( hr ) );
	}
}



void CShaderAPIDx8::CopyRenderTargetToTexture( ShaderAPITextureHandle_t textureHandle )
{
	LOCK_SHADERAPI();
	CopyRenderTargetToTextureEx( textureHandle, 0 );
}


void CShaderAPIDx8::CopyTextureToRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t textureHandle, Rect_t *pSrcRect, Rect_t *pDstRect )
{
	LOCK_SHADERAPI();
	VPROF( "CShaderAPIDx8::CopyRenderTargetToTexture" );

	if ( !TextureIsAllocated( textureHandle ) )
		return;

	// Don't flush here!!  If you have to flush here, then there is a driver bug.
	// FlushHardware( );

	AssertValidTextureHandle( textureHandle );
	Texture_t *pTexture = &GetTexture( textureHandle );
	Assert( pTexture );
	IDirect3DTexture *pD3DTexture = (IDirect3DTexture *)pTexture->GetTexture();
	Assert( pD3DTexture );

	se::win::com::com_ptr<IDirect3DSurface> pRenderTargetSurface;
	HRESULT hr = Dx9Device()->GetRenderTarget( nRenderTargetID, &pRenderTargetSurface );
	if ( FAILED( hr ) )
	{
		Assert( 0 );
		return;
	}

	se::win::com::com_ptr<IDirect3DSurface> pDstSurf;
	hr = pD3DTexture->GetSurfaceLevel( 0, &pDstSurf );
	Assert( !FAILED( hr ) );
	if ( FAILED( hr ) )
	{
		return;
	}

	RECORD_COMMAND( DX8_COPY_FRAMEBUFFER_TO_TEXTURE, 1 );
	RECORD_INT( textureHandle );

	RECT srcRect, dstRect;
	hr = Dx9Device()->StretchRect( pDstSurf, RectToRECT( pSrcRect, srcRect ),
		pRenderTargetSurface, RectToRECT( pDstRect, dstRect ), D3DTEXF_LINEAR );
	Assert( !FAILED( hr ) );
}


//-----------------------------------------------------------------------------
// modifies the vertex data when necessary
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ModifyVertexData( )
{
	// this should be a dead code path
	Assert( 0 );
#if 0
	// We have to modulate the vertex color by the constant color sometimes
	if (IsModulatingVertexColor())
	{
		m_ModifyBuilder.Reset();

		float factor[4];
		unsigned char* pColor = (unsigned char*)&m_DynamicState.m_ConstantColor;
		factor[0] = pColor[0] / 255.0f;
		factor[1] = pColor[1] / 255.0f;
		factor[2] = pColor[2] / 255.0f;
		factor[3] = pColor[3] / 255.0f;

		for ( int i = 0; i < m_ModifyBuilder.VertexCount(); ++i )
		{
			unsigned int color = m_ModifyBuilder.Color();
			unsigned char* pVertexColor = (unsigned char*)&color;

			pVertexColor[0] = (unsigned char)((float)pVertexColor[0] * factor[0]);
			pVertexColor[1] = (unsigned char)((float)pVertexColor[1] * factor[1]);
			pVertexColor[2] = (unsigned char)((float)pVertexColor[2] * factor[2]);
			pVertexColor[3] = (unsigned char)((float)pVertexColor[3] * factor[3]);
			m_ModifyBuilder.Color4ubv( pVertexColor );

			m_ModifyBuilder.AdvanceVertex();
		}
	}
#endif
}

static const char *TextureArgToString( int arg )
{
	static char buf[128];
	switch( arg & D3DTA_SELECTMASK )
	{
	case D3DTA_DIFFUSE:
		V_strcpy_safe( buf, "D3DTA_DIFFUSE" );
		break;
	case D3DTA_CURRENT:
		V_strcpy_safe( buf, "D3DTA_CURRENT" );
		break;
	case D3DTA_TEXTURE:
		V_strcpy_safe( buf, "D3DTA_TEXTURE" );
		break;
	case D3DTA_TFACTOR:
		V_strcpy_safe( buf, "D3DTA_TFACTOR" );
		break;
	case D3DTA_SPECULAR:
		V_strcpy_safe( buf, "D3DTA_SPECULAR" );
		break;
	case D3DTA_TEMP:
		V_strcpy_safe( buf, "D3DTA_TEMP" );
		break;
	default:
		V_strcpy_safe( buf, "<ERROR>" );
		break;
	}

	if( arg & D3DTA_COMPLEMENT )
	{
		V_strcat_safe( buf, "|D3DTA_COMPLEMENT" );
	}
	if( arg & D3DTA_ALPHAREPLICATE )
	{
		V_strcat_safe( buf, "|D3DTA_ALPHAREPLICATE" );
	}
	return buf;
}

static const char *TextureOpToString( D3DTEXTUREOP op )
{
	switch( op )
	{
	case D3DTOP_DISABLE:
		return "D3DTOP_DISABLE";
    case D3DTOP_SELECTARG1:
		return "D3DTOP_SELECTARG1";
    case D3DTOP_SELECTARG2:
		return "D3DTOP_SELECTARG2";
    case D3DTOP_MODULATE:
		return "D3DTOP_MODULATE";
    case D3DTOP_MODULATE2X:
		return "D3DTOP_MODULATE2X";
    case D3DTOP_MODULATE4X:
		return "D3DTOP_MODULATE4X";
    case D3DTOP_ADD:
		return "D3DTOP_ADD";
    case D3DTOP_ADDSIGNED:
		return "D3DTOP_ADDSIGNED";
    case D3DTOP_ADDSIGNED2X:
		return "D3DTOP_ADDSIGNED2X";
    case D3DTOP_SUBTRACT:
		return "D3DTOP_SUBTRACT";
    case D3DTOP_ADDSMOOTH:
		return "D3DTOP_ADDSMOOTH";
    case D3DTOP_BLENDDIFFUSEALPHA:
		return "D3DTOP_BLENDDIFFUSEALPHA";
    case D3DTOP_BLENDTEXTUREALPHA:
		return "D3DTOP_BLENDTEXTUREALPHA";
    case D3DTOP_BLENDFACTORALPHA:
		return "D3DTOP_BLENDFACTORALPHA";
    case D3DTOP_BLENDTEXTUREALPHAPM:
		return "D3DTOP_BLENDTEXTUREALPHAPM";
    case D3DTOP_BLENDCURRENTALPHA:
		return "D3DTOP_BLENDCURRENTALPHA";
    case D3DTOP_PREMODULATE:
		return "D3DTOP_PREMODULATE";
    case D3DTOP_MODULATEALPHA_ADDCOLOR:
		return "D3DTOP_MODULATEALPHA_ADDCOLOR";
    case D3DTOP_MODULATECOLOR_ADDALPHA:
		return "D3DTOP_MODULATECOLOR_ADDALPHA";
    case D3DTOP_MODULATEINVALPHA_ADDCOLOR:
		return "D3DTOP_MODULATEINVALPHA_ADDCOLOR";
    case D3DTOP_MODULATEINVCOLOR_ADDALPHA:
		return "D3DTOP_MODULATEINVCOLOR_ADDALPHA";
    case D3DTOP_BUMPENVMAP:
		return "D3DTOP_BUMPENVMAP";
    case D3DTOP_BUMPENVMAPLUMINANCE:
		return "D3DTOP_BUMPENVMAPLUMINANCE";
    case D3DTOP_DOTPRODUCT3:
		return "D3DTOP_DOTPRODUCT3";
    case D3DTOP_MULTIPLYADD:
		return "D3DTOP_MULTIPLYADD";
    case D3DTOP_LERP:
		return "D3DTOP_LERP";
	default:
		return "<ERROR>";
	}
}

static const char *BlendModeToString( int blendMode )
{
	switch( blendMode )
	{
	case D3DBLEND_ZERO:
		return "D3DBLEND_ZERO";
    case D3DBLEND_ONE:
		return "D3DBLEND_ONE";
    case D3DBLEND_SRCCOLOR:
		return "D3DBLEND_SRCCOLOR";
    case D3DBLEND_INVSRCCOLOR:
		return "D3DBLEND_INVSRCCOLOR";
    case D3DBLEND_SRCALPHA:
		return "D3DBLEND_SRCALPHA";
    case D3DBLEND_INVSRCALPHA:
		return "D3DBLEND_INVSRCALPHA";
    case D3DBLEND_DESTALPHA:
		return "D3DBLEND_DESTALPHA";
    case D3DBLEND_INVDESTALPHA:
		return "D3DBLEND_INVDESTALPHA";
    case D3DBLEND_DESTCOLOR:
		return "D3DBLEND_DESTCOLOR";
    case D3DBLEND_INVDESTCOLOR:
		return "D3DBLEND_INVDESTCOLOR";
    case D3DBLEND_SRCALPHASAT:
		return "D3DBLEND_SRCALPHASAT";
	case D3DBLEND_BOTHSRCALPHA:
		return "D3DBLEND_BOTHSRCALPHA";
    case D3DBLEND_BOTHINVSRCALPHA:
		return "D3DBLEND_BOTHINVSRCALPHA";
	default:
		return "<ERROR>";
	}
}

//-----------------------------------------------------------------------------
// Spew Board State
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SpewBoardState()
{
	// FIXME: This has regressed
	return;
#ifdef DEBUG_BOARD_STATE
	char buf[256];
	V_sprintf_safe(buf, "\nSnapshot id %zd : \n", m_TransitionTable.CurrentSnapshot() );
	Plat_DebugString(buf);

	ShadowState_t &boardState = m_TransitionTable.BoardState();
	ShadowShaderState_t &boardShaderState = m_TransitionTable.BoardShaderState();

	V_sprintf_safe(buf,"Depth States: ZFunc %d, ZWrite %d, ZEnable %d, ZBias %d\n",
		boardState.m_ZFunc, boardState.m_ZWriteEnable, 
		boardState.m_ZEnable, boardState.m_ZBias );
	Plat_DebugString(buf);
	V_sprintf_safe(buf,"Cull Enable %d Cull Mode %d Color Write %lu Fill %d Const Color Mod %d sRGBWriteEnable %d\n",
		boardState.m_CullEnable, m_DynamicState.m_CullMode, boardState.m_ColorWriteEnable, 
		boardState.m_FillMode, boardShaderState.m_ModulateConstantColor, boardState.m_SRGBWriteEnable );
	Plat_DebugString(buf);
	V_sprintf_safe(buf,"Blend States: Blend Enable %d Test Enable %d Func %d SrcBlend %d (%s) DstBlend %d (%s)\n",
		boardState.m_AlphaBlendEnable, boardState.m_AlphaTestEnable, 
		boardState.m_AlphaFunc, boardState.m_SrcBlend, BlendModeToString( boardState.m_SrcBlend ),
		boardState.m_DestBlend, BlendModeToString( boardState.m_DestBlend ) );
	Plat_DebugString(buf);
	int len = V_sprintf_safe(buf,"Alpha Ref %d, Lighting: %d, Ambient Color %lx, LightsEnabled ",
		boardState.m_AlphaRef, boardState.m_Lighting, m_DynamicState.m_Ambient);

	int i;
	for ( i = 0; i < g_pHardwareConfig->Caps().m_MaxNumLights; ++i)
	{
		len += sprintf(buf+len,"%d ", m_DynamicState.m_LightEnable[i] );
	}
	sprintf(buf+len,"\n");
	Plat_DebugString(buf);
	V_sprintf_safe(buf,"Fixed Function: %d, VertexBlend %d\n",
		boardState.m_UsingFixedFunction, m_DynamicState.m_VertexBlend );
	Plat_DebugString(buf);
	
	V_sprintf_safe(buf,"Pass Vertex Usage: %llx Pixel Shader %p Vertex Shader %p\n",
		boardShaderState.m_VertexUsage, ShaderManager()->GetCurrentPixelShader(), 
		ShaderManager()->GetCurrentVertexShader() );
	Plat_DebugString(buf);

	// REGRESSED!!!!
	/*
	auto* m = &GetTransform(MATERIAL_MODEL);
	V_sprintf_safe(buf,"WorldMat [%4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f]\n",
		m->m[0][0],	m->m[0][1],	m->m[0][2],	m->m[0][3],	
		m->m[1][0],	m->m[1][1],	m->m[1][2],	m->m[1][3],	
		m->m[2][0],	m->m[2][1],	m->m[2][2],	m->m[2][3],	
		m->m[3][0], m->m[3][1], m->m[3][2], m->m[3][3] );
	Plat_DebugString(buf);

	m = &GetTransform(MATERIAL_MODEL + 1);
	V_sprintf_safe(buf,"WorldMat2 [%4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f]\n",
		m->m[0][0],	m->m[0][1],	m->m[0][2],	m->m[0][3],	
		m->m[1][0],	m->m[1][1],	m->m[1][2],	m->m[1][3],	
		m->m[2][0],	m->m[2][1],	m->m[2][2],	m->m[2][3],	
		m->m[3][0], m->m[3][1], m->m[3][2], m->m[3][3] );
	Plat_DebugString(buf);

	m = &GetTransform(MATERIAL_VIEW);
	V_sprintf_safe(buf,"ViewMat [%4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f]\n",
		m->m[0][0],	m->m[0][1],	m->m[0][2],	
		m->m[1][0],	m->m[1][1],	m->m[1][2],
		m->m[2][0],	m->m[2][1],	m->m[2][2],
		m->m[3][0], m->m[3][1], m->m[3][2] );
	Plat_DebugString(buf);

	m = &GetTransform(MATERIAL_PROJECTION);
	V_sprintf_safe(buf,"ProjMat [%4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f]\n",
		m->m[0][0],	m->m[0][1],	m->m[0][2],	
		m->m[1][0],	m->m[1][1],	m->m[1][2],
		m->m[2][0],	m->m[2][1],	m->m[2][2],
		m->m[3][0], m->m[3][1], m->m[3][2] );
	Plat_DebugString(buf);

	for (i = 0; i < GetTextureStageCount(); ++i)
	{
		m = &GetTransform(MATERIAL_TEXTURE0 + i);
		V_sprintf_safe(buf,"TexMat%d [%4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f %4.3f]\n",
			i, m->m[0][0],	m->m[0][1],	m->m[0][2],	
			m->m[1][0],	m->m[1][1],	m->m[1][2],
			m->m[2][0],	m->m[2][1],	m->m[2][2],
			m->m[3][0], m->m[3][1], m->m[3][2] );
		Plat_DebugString(buf);
	}
	*/

	V_sprintf_safe(buf,"Viewport (%lu %lu) [%lu %lu] %4.3f %4.3f\n",
		m_DynamicState.m_Viewport.X, m_DynamicState.m_Viewport.Y, 
		m_DynamicState.m_Viewport.Width, m_DynamicState.m_Viewport.Height,
		m_DynamicState.m_Viewport.MinZ, m_DynamicState.m_Viewport.MaxZ);
	Plat_DebugString(buf);

	for (i = 0; i < MAX_TEXTURE_STAGES; ++i)
	{
		V_sprintf_safe(buf,"Stage %d :\n", i);
		Plat_DebugString(buf);
		V_sprintf_safe(buf,"   Color Op: %d (%s) Color Arg1: %d (%s)",
			boardState.m_TextureStage[i].m_ColorOp, 
			TextureOpToString( boardState.m_TextureStage[i].m_ColorOp ),
			boardState.m_TextureStage[i].m_ColorArg1, 
			TextureArgToString( boardState.m_TextureStage[i].m_ColorArg1 ) );
		Plat_DebugString(buf);
		V_sprintf_safe( buf, " Color Arg2: %d (%s)\n",
			boardState.m_TextureStage[i].m_ColorArg2,
			TextureArgToString( boardState.m_TextureStage[i].m_ColorArg2 ) );
		Plat_DebugString(buf);
		V_sprintf_safe(buf,"   Alpha Op: %d (%s) Alpha Arg1: %d (%s)",
			boardState.m_TextureStage[i].m_AlphaOp, 
			TextureOpToString( boardState.m_TextureStage[i].m_AlphaOp ),
			boardState.m_TextureStage[i].m_AlphaArg1, 
			TextureArgToString( boardState.m_TextureStage[i].m_AlphaArg1 ) );
		Plat_DebugString(buf);
		V_sprintf_safe(buf," Alpha Arg2: %d (%s)\n",
			boardState.m_TextureStage[i].m_AlphaArg2,
			TextureArgToString( boardState.m_TextureStage[i].m_AlphaArg2 ) );
		Plat_DebugString(buf);
	}

	for ( int ii = 0; ii < MAX_SAMPLERS; ++ii )
	{
		V_sprintf_safe(buf,"	Texture Enabled: %d Bound Texture: %zi UWrap: %d VWrap: %d\n",
			SamplerState(ii).m_TextureEnable, GetBoundTextureBindId( (Sampler_t)ii ),
			SamplerState(ii).m_UTexWrap, SamplerState(ii).m_VTexWrap );
		Plat_DebugString(buf);
		V_sprintf_safe(buf,"	Mag Filter: %d Min Filter: %d Mip Filter: %d\n",
			SamplerState(ii).m_MagFilter, SamplerState(ii).m_MinFilter,
			SamplerState(ii).m_MipFilter );
		V_sprintf_safe(buf,"   MaxMipLevel: %d\n", SamplerState(ii).m_FinestMipmapLevel );
		Plat_DebugString(buf);
	}
#else
	Plat_DebugString("::SpewBoardState() Not Implemented Yet");
#endif
}

//-----------------------------------------------------------------------------
// Begin a render pass
//-----------------------------------------------------------------------------
void CShaderAPIDx8::BeginPass( StateSnapshot_t snapshot )
{
	LOCK_SHADERAPI();
	VPROF("CShaderAPIDx8::BeginPass");
	if (IsDeactivated())
		return;

	m_nCurrentSnapshot = snapshot;
//	Assert( m_pRenderMesh );
	// FIXME: This only does anything with temp meshes, so don't bother yet for the new code.
	if( m_pRenderMesh )
	{
		m_pRenderMesh->BeginPass( );
	}
}


//-----------------------------------------------------------------------------
// Render da polygon!
//-----------------------------------------------------------------------------
void CShaderAPIDx8::RenderPass( int nPass, int nPassCount )
{
	if ( IsDeactivated() )
		return;

	Assert( m_nCurrentSnapshot != -1 );
//	Assert( m_pRenderMesh );  MESHFIXME

	m_TransitionTable.UseSnapshot( m_nCurrentSnapshot );
	CommitPerPassStateChanges( m_nCurrentSnapshot );

	// Make sure that we bound a texture for every stage that is enabled
	// NOTE: not enabled/finished yet... see comment in CShaderAPIDx8::ApplyTextureEnable
//	int nSampler;
//	for ( nSampler = 0; nSampler < g_pHardwareConfig->GetSamplerCount(); nSampler++ )
//	{
//		if ( SamplerState( nSampler ).m_TextureEnable )
//		{
//		}
//	}
	
#ifdef DEBUG_BOARD_STATE
	// Spew out render state...
	if ( m_pMaterial->PerformDebugTrace() )
	{
		SpewBoardState();
	}
#endif

#ifdef TEST_CACHE_LOCKS
	g_pDataCache->Flush();
#endif

//	Assert( m_pRenderMesh );  MESHFIXME
	if ( m_pRenderMesh )
	{
		m_pRenderMesh->RenderPass();
	}
	else
	{
		MeshMgr()->RenderPassWithVertexAndIndexBuffers();
	}
	m_nCurrentSnapshot = -1;
}


//-----------------------------------------------------------------------------
// Matrix mode
//-----------------------------------------------------------------------------
void CShaderAPIDx8::MatrixMode( MaterialMatrixMode_t matrixMode )
{
	// NOTE!!!!!!
	// The only time that m_MatrixMode is used is for texture matrices.  Do not use
	// it for anything else unless you change this code!
	if ( matrixMode >= MATERIAL_TEXTURE0 && matrixMode <= MATERIAL_TEXTURE7 )
	{
		m_MatrixMode = ( D3DTRANSFORMSTATETYPE )( matrixMode - MATERIAL_TEXTURE0 + D3DTS_TEXTURE0 );
	}
	else
	{
		m_MatrixMode = (D3DTRANSFORMSTATETYPE)-1;
	}

	m_CurrStack = GetMatrixStack( matrixMode );
}
				 	    
//-----------------------------------------------------------------------------
// the current camera position in world space.
//-----------------------------------------------------------------------------
void CShaderAPIDx8::GetWorldSpaceCameraPosition( float* pPos ) const
{
	memcpy( pPos, m_WorldSpaceCameraPositon.Base(), sizeof( float[3] ) );
}

void CShaderAPIDx8::CacheWorldSpaceCameraPosition()
{
	const DirectX::XMMATRIX view = GetTransform(MATERIAL_VIEW);

	DirectX::XMVECTOR worldSpaceCameraPosition0 =
		DirectX::XMVectorNegate( DirectX::XMVector3Dot( view.r[3], view.r[0] ) );
	DirectX::XMVECTOR worldSpaceCameraPosition1 =
		DirectX::XMVectorNegate( DirectX::XMVector3Dot( view.r[3], view.r[1] ) );
	DirectX::XMVECTOR worldSpaceCameraPosition2 =
		DirectX::XMVectorNegate( DirectX::XMVector3Dot( view.r[3], view.r[2] ) );

	DirectX::XMVECTOR worldSpaceCameraPosition =
		DirectX::XMVectorPermute
		<
			DirectX::XM_PERMUTE_0X,
			DirectX::XM_PERMUTE_0Y,
			DirectX::XM_PERMUTE_1X,
			DirectX::XM_PERMUTE_1Y
		>
		(
			DirectX::XMVectorPermute
			<
				DirectX::XM_PERMUTE_0X,
				DirectX::XM_PERMUTE_1X,
				DirectX::XM_PERMUTE_1Y,
				DirectX::XM_PERMUTE_1Z
			>(worldSpaceCameraPosition0, worldSpaceCameraPosition1),
			DirectX::XMVectorPermute
			<
				DirectX::XM_PERMUTE_0X,
				DirectX::XM_PERMUTE_1X,
				DirectX::XM_PERMUTE_1Y,
				DirectX::XM_PERMUTE_1Z
			>(worldSpaceCameraPosition2, DirectX::g_XMOne.v)
		);

	DirectX::XMStoreFloat4( m_WorldSpaceCameraPositon.XmBase(), worldSpaceCameraPosition );

	/*m_WorldSpaceCameraPositon[0] = 
		-DirectX::XMVectorGetX( DirectX::XMVector3Dot( view.r[3], view.r[0] ) );
	m_WorldSpaceCameraPositon[1] =
		-DirectX::XMVectorGetX( DirectX::XMVector3Dot( view.r[3], view.r[1] ) );
	m_WorldSpaceCameraPositon[2] = 
		-DirectX::XMVectorGetX( DirectX::XMVector3Dot( view.r[3], view.r[2] ) );

	m_WorldSpaceCameraPositon[3] = 1.0f;*/

	// Protect against zero, as some pixel shaders will divide by this in CalcWaterFogAlpha() in common_ps_fxc.h
	if ( fabs( m_WorldSpaceCameraPositon[2] ) <= 0.00001f )
	{
		m_WorldSpaceCameraPositon[2] = 0.01f;
	}
}


//-----------------------------------------------------------------------------
// Computes a matrix which includes the poly offset given an initial projection matrix
//-----------------------------------------------------------------------------
void XM_CALLCONV CShaderAPIDx8::ComputePolyOffsetMatrix( DirectX::FXMMATRIX matProjection, DirectX::XMMATRIX& matProjectionOffset )
{
	// We never need to do this on hardware that can handle zbias
	if ( g_pHardwareConfig->Caps().m_ZBiasAndSlopeScaledDepthBiasSupported )
		return;

	float offsetVal = 
		-1.0f * (m_DesiredState.m_Viewport.MaxZ - m_DesiredState.m_Viewport.MinZ) /
		16384.0f;

	matProjectionOffset = matProjection * DirectX::XMMatrixTranslation( 0.0f, 0.0f, offsetVal );
}


//-----------------------------------------------------------------------------
// Caches off the poly-offset projection matrix
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CachePolyOffsetProjectionMatrix()
{
	ComputePolyOffsetMatrix( GetTransform(MATERIAL_PROJECTION), m_CachedPolyOffsetProjectionMatrix );
}


//-----------------------------------------------------------------------------
// Performs a flush on the matrix state	if necessary
//-----------------------------------------------------------------------------
bool CShaderAPIDx8::MatrixIsChanging( TransformType_t type )
{
	if ( IsDeactivated() )	
	{
		return false;
	}

	// early out if the transform is already one of our standard types
	if ((type != TRANSFORM_IS_GENERAL) && (type == m_DynamicState.m_TransformType[m_CurrStack]))
		return false;

	// Only flush state if we're changing something other than a texture transform
	int textureMatrix = m_CurrStack - MATERIAL_TEXTURE0;
	if (( textureMatrix < 0 ) || (textureMatrix >= NUM_TEXTURE_TRANSFORMS))
		FlushBufferedPrimitivesInternal();

	return true;
}

void CShaderAPIDx8::SetTextureTransformDimension( TextureStage_t textureMatrix, int dimension, bool projected )
{
	D3DTEXTURETRANSFORMFLAGS textureTransformFlags = ( D3DTEXTURETRANSFORMFLAGS )dimension;
	if( projected )
	{
		Assert( sizeof( int ) == sizeof( D3DTEXTURETRANSFORMFLAGS ) );
		( *( int * )&textureTransformFlags ) |= D3DTTFF_PROJECTED;
	}

	if (TextureStage(textureMatrix).m_TextureTransformFlags != textureTransformFlags )
	{
		SetTextureStageState( textureMatrix, D3DTSS_TEXTURETRANSFORMFLAGS, textureTransformFlags );
		TextureStage(textureMatrix).m_TextureTransformFlags = textureTransformFlags;
	}
}

void CShaderAPIDx8::DisableTextureTransform( TextureStage_t textureMatrix )
{
	if (TextureStage(textureMatrix).m_TextureTransformFlags != D3DTTFF_DISABLE )
	{
		SetTextureStageState( textureMatrix, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
		TextureStage(textureMatrix).m_TextureTransformFlags = D3DTTFF_DISABLE;
	}
}

void CShaderAPIDx8::SetBumpEnvMatrix( TextureStage_t textureStage, float m00, float m01, float m10, float m11 )
{	
	TextureStageState_t &textureStageState = TextureStage( textureStage );
	
	if( textureStageState.m_BumpEnvMat00 != m00 ||
		textureStageState.m_BumpEnvMat01 != m01 ||
		textureStageState.m_BumpEnvMat10 != m10 ||
		textureStageState.m_BumpEnvMat11 != m11 )
	{
		SetTextureStageState( textureStage, D3DTSS_BUMPENVMAT00, *( ( LPDWORD ) (&m00) ) );
		SetTextureStageState( textureStage, D3DTSS_BUMPENVMAT01, *( ( LPDWORD ) (&m01) ) );
		SetTextureStageState( textureStage, D3DTSS_BUMPENVMAT10, *( ( LPDWORD ) (&m10) ) );
		SetTextureStageState( textureStage, D3DTSS_BUMPENVMAT11, *( ( LPDWORD ) (&m11) ) );
		textureStageState.m_BumpEnvMat00 = m00;
		textureStageState.m_BumpEnvMat01 = m01;
		textureStageState.m_BumpEnvMat10 = m10;
		textureStageState.m_BumpEnvMat11 = m11;
	}
}

//-----------------------------------------------------------------------------
// Sets the actual matrix state
//-----------------------------------------------------------------------------
void CShaderAPIDx8::UpdateMatrixTransform( TransformType_t type )
{
	int textureMatrix = m_CurrStack - MATERIAL_TEXTURE0;
	if (( textureMatrix >= 0 ) && (textureMatrix < NUM_TEXTURE_TRANSFORMS))
	{
		// NOTE: Flush shouldn't happen here because we
		// expect that texture transforms will be set within the shader

		// FIXME: We only want to use D3DTTFF_COUNT3 for cubemaps
		// D3DTFF_COUNT2 is used for non-cubemaps. Of course, if there's
		// no performance penalty for COUNT3, we should just use that.
		D3DTEXTURETRANSFORMFLAGS transformFlags;
		transformFlags = (type == TRANSFORM_IS_IDENTITY) ? D3DTTFF_DISABLE : D3DTTFF_COUNT3;

		if (TextureStage(textureMatrix).m_TextureTransformFlags != transformFlags )
		{
			SetTextureStageState( textureMatrix, D3DTSS_TEXTURETRANSFORMFLAGS, transformFlags );
			TextureStage(textureMatrix).m_TextureTransformFlags = transformFlags;
		}
	}

	m_DynamicState.m_TransformType[m_CurrStack] = type;
	m_DynamicState.m_TransformChanged[m_CurrStack] = STATE_CHANGED;

#ifdef _DEBUG
	// Store off the board state
	const DirectX::XMMATRIX *pSrc = &GetTransform(m_CurrStack);
	DirectX::XMMATRIX *pDst = &m_DynamicState.m_Transform[m_CurrStack];
//	Assert( *pSrc != *pDst );
	memcpy( pDst, pSrc, sizeof(DirectX::XMMATRIX) );
#endif

	if ( m_CurrStack == MATERIAL_VIEW )
	{
		CacheWorldSpaceCameraPosition();
	}

	if ( m_CurrStack == MATERIAL_PROJECTION )
	{
		CachePolyOffsetProjectionMatrix();
	}

	// Any time the view or projection matrix changes, the user clip planes need recomputing....
	// Assuming we're not overriding the user clip transform
	if ( ( m_CurrStack == MATERIAL_PROJECTION ) ||
		 ( ( m_CurrStack == MATERIAL_VIEW ) && ( !m_DynamicState.m_bUserClipTransformOverride ) ) )
	{
		MarkAllUserClipPlanesDirty();
	}

	// Set the state if it's a texture transform
	if ( (m_CurrStack >= MATERIAL_TEXTURE0) && (m_CurrStack <= MATERIAL_TEXTURE7) )
	{
		SetTransform( m_MatrixMode, &GetTransform(m_CurrStack) );
	}
}


//--------------------------------------------------------------------------------
// deformations
//--------------------------------------------------------------------------------
void CShaderAPIDx8::PushDeformation( DeformationBase_t const *pDef )
{
	Assert( m_pDeformationStackPtr > m_DeformationStack );
	--m_pDeformationStackPtr;
	m_pDeformationStackPtr->m_nDeformationType = pDef->m_eType;

	switch( pDef->m_eType )
	{
		case DEFORMATION_CLAMP_TO_BOX_IN_WORLDSPACE:
		{
			BoxDeformation_t const *pBox = reinterpret_cast< const BoxDeformation_t *>( pDef );
			m_pDeformationStackPtr->m_nNumParameters = 16;
			memcpy( m_pDeformationStackPtr->m_flDeformationParameters, pBox, 16 * sizeof( float ) );
		}
		break;
		
		default:
			Assert( 0 );
	}
}


void CShaderAPIDx8::PopDeformation( )
{
	Assert( m_pDeformationStackPtr != m_DeformationStack + DEFORMATION_STACK_DEPTH );
	++m_pDeformationStackPtr;
}

intp CShaderAPIDx8::GetNumActiveDeformations( void ) const
{
	return ( m_DeformationStack + DEFORMATION_STACK_DEPTH ) - m_pDeformationStackPtr;
}


// for shaders to set vertex shader constants. returns a packed state which can be used to set the dynamic combo
int CShaderAPIDx8::GetPackedDeformationInformation( int nMaskOfUnderstoodDeformations,
													 float *pConstantValuesOut,
													 int nBufferSize,
													 int nMaximumDeformations,
													 int *pDefCombosOut ) const
{
	int nCombosFound = 0;
	memset( pDefCombosOut, 0, sizeof( pDefCombosOut[0] ) * nMaximumDeformations );
	size_t nRemainingBufferSize = nBufferSize;

	for( const Deformation_t *i = m_DeformationStack + DEFORMATION_STACK_DEPTH -1; i >= m_pDeformationStackPtr; i-- )
	{
		int nFloatsOut = 4 * ( ( i->m_nNumParameters + 3 )>> 2 );
		if ( 
			( ( 1 << i->m_nDeformationType )  & nMaskOfUnderstoodDeformations ) &&
			( nRemainingBufferSize >= ( nFloatsOut * sizeof( float ) ) ) )
		{
			memcpy( pConstantValuesOut, i->m_flDeformationParameters, nFloatsOut * sizeof( float ) );
			pConstantValuesOut += nFloatsOut;
			nRemainingBufferSize -= nFloatsOut * sizeof( float );
			( *pDefCombosOut++ ) = i->m_nDeformationType;
			nCombosFound++;
		}
	}
	return nCombosFound;
}




//-----------------------------------------------------------------------------
// Matrix stack operations
//-----------------------------------------------------------------------------

void CShaderAPIDx8::PushMatrix()
{
	// NOTE: No matrix transform update needed here.
	m_pMatrixStack[m_CurrStack]->Push();
}

void CShaderAPIDx8::PopMatrix()
{
	if (MatrixIsChanging())
	{
		m_pMatrixStack[m_CurrStack]->Pop();
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::LoadIdentity( )
{
	if (MatrixIsChanging(TRANSFORM_IS_IDENTITY))
	{
		m_pMatrixStack[m_CurrStack]->LoadIdentity( );
		UpdateMatrixTransform( TRANSFORM_IS_IDENTITY );
	}
}

void CShaderAPIDx8::LoadCameraToWorld( )
{
	if (MatrixIsChanging(TRANSFORM_IS_CAMERA_TO_WORLD))
	{
		// could just use the transpose instead, if we know there's no scale
		DirectX::XMMATRIX inv{ DirectX::XMMatrixInverse( nullptr, GetTransform(MATERIAL_VIEW) ) };

		// Kill translation
		inv.r[3] = DirectX::XMVectorPermute
		<
			DirectX::XM_PERMUTE_1X,
			DirectX::XM_PERMUTE_1Y,
			DirectX::XM_PERMUTE_1Z,
			DirectX::XM_PERMUTE_0W
		>( inv.r[3], Four_Zeros );
		// inv.r[3].m128_f32[0] = inv.r[3].m128_f32[1] = inv.r[3].m128_f32[2] = 0.0F;

		m_pMatrixStack[m_CurrStack]->LoadMatrix( inv );
		UpdateMatrixTransform( TRANSFORM_IS_CAMERA_TO_WORLD );
	}
}

void CShaderAPIDx8::LoadMatrix( float *m )
{
	// Check for identity...
	if ( (fabs(m[0] - 1.0f) < 1e-3) && (fabs(m[5] - 1.0f) < 1e-3) && (fabs(m[10] - 1.0f) < 1e-3) && (fabs(m[15] - 1.0f) < 1e-3) &&
		 (fabs(m[1]) < 1e-3)  && (fabs(m[2]) < 1e-3)  && (fabs(m[3]) < 1e-3) &&
		 (fabs(m[4]) < 1e-3)  && (fabs(m[6]) < 1e-3)  && (fabs(m[7]) < 1e-3) &&
		 (fabs(m[8]) < 1e-3)  && (fabs(m[9]) < 1e-3)  && (fabs(m[11]) < 1e-3) &&
		 (fabs(m[12]) < 1e-3) && (fabs(m[13]) < 1e-3) && (fabs(m[14]) < 1e-3) )
	{
		LoadIdentity();
		return;
	}

	if (MatrixIsChanging())
	{
		m_pMatrixStack[m_CurrStack]->LoadMatrix( DirectX::XMMATRIX{ m } );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::LoadBoneMatrix( int boneIndex, const float *m )
{
	if ( IsDeactivated() )	
		return;

	memcpy( m_boneMatrix[boneIndex].Base(), m, sizeof(float)*12 );
	if ( boneIndex > m_maxBoneLoaded )
	{
		m_maxBoneLoaded = boneIndex;
	}
	if ( boneIndex == 0 )
	{
		MatrixMode( MATERIAL_MODEL );
		VMatrix transposeMatrix;
		transposeMatrix.Init( *(const matrix3x4_t *)m );
		MatrixTranspose( transposeMatrix, transposeMatrix );
		LoadMatrix( (float*)transposeMatrix.m );
	}
}

//-----------------------------------------------------------------------------
// Commits morph target factors
//-----------------------------------------------------------------------------
static void CommitFlexWeights( IDirect3DDevice9 *pDevice, const DynamicState_t &desiredState, 
									 DynamicState_t &currentState, bool bForce )
{
	CommitVertexShaderConstantRange( pDevice, desiredState, currentState, bForce,
		VERTEX_SHADER_FLEX_WEIGHTS, VERTEX_SHADER_MAX_FLEX_WEIGHT_COUNT );
}

void CShaderAPIDx8::SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights )
{
	LOCK_SHADERAPI();
	if ( g_pHardwareConfig->Caps().m_NumVertexShaderConstants < VERTEX_SHADER_FLEX_WEIGHTS + VERTEX_SHADER_MAX_FLEX_WEIGHT_COUNT )
		return;

	if ( nFirstWeight + nCount > VERTEX_SHADER_MAX_FLEX_WEIGHT_COUNT )	
	{
		Warning( "Attempted to set too many flex weights! Max is %d\n", VERTEX_SHADER_MAX_FLEX_WEIGHT_COUNT );
		nCount = VERTEX_SHADER_MAX_FLEX_WEIGHT_COUNT - nFirstWeight;
	}

	if ( nCount <= 0 )
		return;

	float *pDest = m_DesiredState.m_pVectorVertexShaderConstant[ VERTEX_SHADER_FLEX_WEIGHTS + nFirstWeight ].Base();
	memcpy( pDest, pWeights, nCount * sizeof(MorphWeight_t) );

	ADD_COMMIT_FUNC( COMMIT_PER_DRAW, COMMIT_VERTEX_SHADER, CommitFlexWeights );
}

void CShaderAPIDx8::MultMatrix( float *m )
{
	if (MatrixIsChanging())
	{
		m_pMatrixStack[m_CurrStack]->MultiplyMatrix( DirectX::XMMATRIX{ m } );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::MultMatrixLocal( float *m )
{
	if (MatrixIsChanging())
	{
		m_pMatrixStack[m_CurrStack]->MultiplyMatrixLocal( DirectX::XMMATRIX{ m } );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::Rotate( float angle, float x, float y, float z )
{
	if (MatrixIsChanging())
	{
		DirectX::XMVECTOR axis( DirectX::XMVectorSet( x, y, z, 0.0f ) );
		m_pMatrixStack[m_CurrStack]->RotateAxisLocal( axis, M_PI_F * angle / 180.0f );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::Translate( float x, float y, float z )
{
	if (MatrixIsChanging())
	{
		m_pMatrixStack[m_CurrStack]->TranslateLocal( x, y, z );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::Scale( float x, float y, float z )
{
	if (MatrixIsChanging())
	{
		m_pMatrixStack[m_CurrStack]->ScaleLocal( x, y, z );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::ScaleXY( float x, float y )
{
	if (MatrixIsChanging())
	{
		m_pMatrixStack[m_CurrStack]->ScaleLocal( x, y, 1.0f );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::Ortho( double left, double top, double right, double bottom, double zNear, double zFar )
{
	if (MatrixIsChanging())
	{
		// FIXME: This is being used incorrectly! Should read:
		// matrix = XMMatrixOrthographicOffCenterRH( left, right, bottom, top, zNear, zFar );
		// Which is certainly why we need these extra -1 scales in y. Bleah

		// NOTE: The camera can be imagined as the following diagram:
		//		/z
		//	   /
		//	  /____ x	Z is going into the screen
		//	  |
		//	  |
		//	  |y
		//
		// (0,0,z) represents the upper-left corner of the screen.
		// Our projection transform needs to transform from this space to a LH coordinate
		// system that looks thusly:
		// 
		//	y|  /z
		//	 | /
		//	 |/____ x	Z is going into the screen
		//
		// Where x,y lies between -1 and 1, and z lies from 0 to 1
		// This is because the viewport transformation from projection space to pixels
		// introduces a -1 scale in the y coordinates
//		matrix = XMMatrixOrthographicOffCenterLH( left, right, bottom, top, zNear, zFar );

		DirectX::XMMATRIX matrix = DirectX::XMMatrixOrthographicOffCenterRH( left, right, top, bottom, zNear, zFar );
		m_pMatrixStack[m_CurrStack]->MultiplyMatrixLocal( matrix );
		Assert( m_CurrStack == MATERIAL_PROJECTION );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::PerspectiveX( double fovx, double aspect, double zNear, double zFar )
{
	if (MatrixIsChanging())
	{
		float width = 2 * zNear * tanf( fovx * M_PI_F / 360.0F );
		float height = width / aspect;
		Assert( m_CurrStack == MATERIAL_PROJECTION );
		DirectX::XMMATRIX rh = DirectX::XMMatrixPerspectiveRH( width, height, zNear, zFar );
		m_pMatrixStack[m_CurrStack]->MultiplyMatrixLocal( rh );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right )
{
	if (MatrixIsChanging())
	{
		float width = 2 * zNear * tanf( fovx * M_PI_F / 360.0F );
		float height = width / aspect;

		// bottom, top, left, right are 0..1 so convert to -1..1
		float flFrontPlaneLeft   = -(width/2.0f)  * (1.0f - left)   + left   * (width/2.0f);
		float flFrontPlaneRight  = -(width/2.0f)  * (1.0f - right)  + right  * (width/2.0f);
		float flFrontPlaneBottom = -(height/2.0f) * (1.0f - bottom) + bottom * (height/2.0f);
		float flFrontPlaneTop    = -(height/2.0f) * (1.0f - top)    + top    * (height/2.0f);

		Assert( m_CurrStack == MATERIAL_PROJECTION );
		DirectX::XMMATRIX rh = DirectX::XMMatrixPerspectiveOffCenterRH( flFrontPlaneLeft, flFrontPlaneRight, flFrontPlaneBottom, flFrontPlaneTop, zNear, zFar );
		m_pMatrixStack[m_CurrStack]->MultiplyMatrixLocal( rh );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::PickMatrix( int x, int y, int width, int height )
{
	if (MatrixIsChanging())
	{
		Assert( m_CurrStack == MATERIAL_PROJECTION );

		// This is going to create a matrix to append to the standard projection.
		// Projection space goes from -1 to 1 in x and y. This matrix we append
		// will transform the pick region to -1 to 1 in projection space
		ShaderViewport_t viewport;
		GetViewports( &viewport, 1 );
		
		int vx = viewport.m_nTopLeftX;
		int vy = viewport.m_nTopLeftX;
		int vwidth = viewport.m_nWidth;
		int vheight = viewport.m_nHeight;

		// Compute the location of the pick region in projection space...
		float px = 2.0f * (float)(x - vx) / (float)vwidth - 1;
		float py = 2.0f * (float)(y - vy)/ (float)vheight - 1;
		float pw = 2.0f * (float)width / (float)vwidth;
		float ph = 2.0f * (float)height / (float)vheight;

		// we need to translate (px, py) to the origin
		// and scale so (pw,ph) -> (2, 2)
		DirectX::XMMATRIX matrix = DirectX::XMMatrixIdentity();
		DirectX::XMVectorSetX( matrix.r[0], 2.0f / pw );
		DirectX::XMVectorSetY( matrix.r[1], 2.0f / ph );
		DirectX::XMVectorSetX( matrix.r[3], -2.0f * px / pw );
		DirectX::XMVectorSetY( matrix.r[3], -2.0f * py / ph );

		m_pMatrixStack[m_CurrStack]->MultiplyMatrixLocal( matrix );
		UpdateMatrixTransform();
	}
}

void CShaderAPIDx8::GetMatrix( MaterialMatrixMode_t matrixMode, float *dst )
{
	memcpy( dst, &GetTransform(matrixMode), sizeof(DirectX::XMMATRIX) );
}


//-----------------------------------------------------------------------------
// Did a transform change?
//-----------------------------------------------------------------------------
inline bool CShaderAPIDx8::VertexShaderTransformChanged( int i )
{
	return (m_DynamicState.m_TransformChanged[i] & STATE_CHANGED_VERTEX_SHADER) != 0;
}

inline bool CShaderAPIDx8::FixedFunctionTransformChanged( int i )
{
	return (m_DynamicState.m_TransformChanged[i] & STATE_CHANGED_FIXED_FUNCTION) != 0;
}


const DirectX::XMMATRIX& CShaderAPIDx8::GetProjectionMatrix( void )
{
	bool bUsingZBiasProjectionMatrix = 
		!g_pHardwareConfig->Caps().m_ZBiasAndSlopeScaledDepthBiasSupported &&
		( m_TransitionTable.CurrentSnapshot() != -1 ) &&
		m_TransitionTable.CurrentShadowState() &&
		m_TransitionTable.CurrentShadowState()->m_ZBias;

	if ( !m_DynamicState.m_FastClipEnabled )
	{
		if ( bUsingZBiasProjectionMatrix )
			return m_CachedPolyOffsetProjectionMatrix;

		return GetTransform( MATERIAL_PROJECTION );
	}
	
	if ( bUsingZBiasProjectionMatrix )
		return m_CachedFastClipPolyOffsetProjectionMatrix;

	return m_CachedFastClipProjectionMatrix;
}


//-----------------------------------------------------------------------------
// Workaround hack for visualization of selection mode
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetupSelectionModeVisualizationState()
{
	Dx9Device()->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

	DirectX::XMMATRIX ident{ DirectX::XMMatrixIdentity() };
	SetTransform( D3DTS_WORLD, &ident );
	SetTransform( D3DTS_VIEW, &ident );
	SetTransform( D3DTS_PROJECTION, &ident );

	if ( g_pHardwareConfig->Caps().m_SupportsPixelShaders )
	{
		SetVertexShaderConstant( VERTEX_SHADER_VIEWPROJ, reinterpret_cast<float*>(&ident.r[0]), 4 );
		SetVertexShaderConstant( VERTEX_SHADER_MODELVIEWPROJ, reinterpret_cast<float*>(&ident.r[0]), 4 );
		SetVertexShaderConstant( VERTEX_SHADER_MODELVIEWPROJ_THIRD_ROW, reinterpret_cast<float*>(&ident.r[2]), 1 ); // Row two of an identity matrix
		SetVertexShaderConstant( VERTEX_SHADER_MODEL, reinterpret_cast<float*>(&ident.r[0]), 3 * NUM_MODEL_TRANSFORMS );
	}
}


//-----------------------------------------------------------------------------
// Set view transforms
//-----------------------------------------------------------------------------

//static void printmat4x4( char *label, float *m00 )
//{
//	// print label..
//	// fetch 4 from row, print as a row
//	// fetch 4 from column, print as a row
//	
//#ifdef DX_TO_GL_ABSTRACTION
//	float	row[4];
//	float	col[4];
//	
//	GLMPRINTF(("-M-    -- %s --", label ));
//	for( int n=0; n<4; n++ )
//	{
//		// extract row and column floats
//		for( int i=0; i<4;i++)
//		{
//			row[i] = m00[(n*4)+i];			
//			col[i] = m00[(i*4)+n];
//		}
//		GLMPRINTF((		"-M-    [ %7.4f %7.4f %7.4f %7.4f ] T=> [ %7.4f %7.4f %7.4f %7.4f ]",
//						row[0],row[1],row[2],row[3],
//						col[0],col[1],col[2],col[3]						
//						));
//	}
//	GLMPRINTF(("-M-"));
//#endif
//}

void CShaderAPIDx8::SetVertexShaderViewProj()
{
	//GLM_FUNC;
	//GLMPRINTF(( ">-M- SetVertexShaderViewProj" ));
	
	if (g_pHardwareConfig->Caps().m_SupportsPixelShaders)
	{
		// show work
		DirectX::XMMATRIX matView		= GetTransform(MATERIAL_VIEW);
		DirectX::XMMATRIX matProj		= GetProjectionMatrix();
		DirectX::XMMATRIX transpose	= matView * matProj;
			
		//printmat4x4( "matView", (float*)&matView );
		//printmat4x4( "matProj", (float*)&matProj );
		//printmat4x4( "result (view * proj) pre-transpose", (float*)&transpose );

		transpose = DirectX::XMMatrixTranspose( transpose );

		#if 0	// turned off while we try to do fixup-Y in shader translate
			if (IsPosix())			// flip all shader projection matrices for Y on GL since you can't have an upside-down viewport specification
			{
				// flip Y
				transpose._21 *= -1.0f;
				transpose._22 *= -1.0f;
				transpose._23 *= -1.0f;
				transpose._24 *= -1.0f;
			}
		#endif
			
		//printmat4x4( "result (view * proj) post-transpose", (float*)&transpose );
		
		SetVertexShaderConstant( VERTEX_SHADER_VIEWPROJ, reinterpret_cast<float*>(&transpose.r[0]), 4 );

		// If we're doing FastClip, the above viewproj matrix won't work well for
		// vertex shaders which compute projPos.z, hence we'll compute a more useful
		// viewproj and put the third row of it in another constant
		transpose = GetTransform( MATERIAL_VIEW ) * GetTransform( MATERIAL_PROJECTION ); // Get the non-FastClip projection matrix
		transpose = DirectX::XMMatrixTranspose( transpose );

		SetVertexShaderConstant( VERTEX_SHADER_VIEWPROJ_THIRD_ROW, reinterpret_cast<float*>(&transpose.r[2]), 1 );
	}
	//GLMPRINTF(( "<-M- SetVertexShaderViewProj" ));
}

void CShaderAPIDx8::SetVertexShaderModelViewProjAndModelView( void )
{
	//GLM_FUNC;
	//GLMPRINTF(( ">-M- SetVertexShaderModelViewProjAndModelView" ));
	
	if (g_pHardwareConfig->Caps().m_SupportsPixelShaders)
	{
		// show work
		DirectX::XMMATRIX matModel	= GetTransform(MATERIAL_MODEL);
		DirectX::XMMATRIX matView		= GetTransform(MATERIAL_VIEW);
		DirectX::XMMATRIX matProj		= GetProjectionMatrix();
			
		DirectX::XMMATRIX modelView = matModel * matView;
		DirectX::XMMATRIX transpose = modelView * matProj;
			
		//printmat4x4( "matModel", (float*)&matModel );
		//printmat4x4( "matView", (float*)&matView );
		//printmat4x4( "matProj", (float*)&matProj );
		//printmat4x4( "result (model * view * proj) pre-transpose", (float*)&transpose );

		transpose = DirectX::XMMatrixTranspose( transpose );
		
		#if 0	// turned off while we try to do fixup-Y in shader translate
			if (IsPosix())			// flip all shader projection matrices for Y on GL since you can't have an upside-down viewport specification
			{
				// flip Y
				transpose._21 *= -1.0f;
				transpose._22 *= -1.0f;
				transpose._23 *= -1.0f;
				transpose._24 *= -1.0f;
			}
		#endif
		
		SetVertexShaderConstant( VERTEX_SHADER_MODELVIEWPROJ, reinterpret_cast<float*>(&transpose.r[0]), 4 );

		// If we're doing FastClip, the above modelviewproj matrix won't work well for
		// vertex shaders which compute projPos.z, hence we'll compute a more useful
		// modelviewproj and put the third row of it in another constant
		transpose = modelView * GetTransform( MATERIAL_PROJECTION ); // Get the non-FastClip projection matrix
		transpose = DirectX::XMMatrixTranspose( transpose );

		SetVertexShaderConstant( VERTEX_SHADER_MODELVIEWPROJ_THIRD_ROW, reinterpret_cast<float*>(&transpose.r[2]), 1 );
	}
	
	//GLMPRINTF(( "<-M- SetVertexShaderModelViewProjAndModelView" ));
}

void CShaderAPIDx8::UpdateVertexShaderMatrix( int iMatrix )
{
	//GLM_FUNC;
	if ( iMatrix == 0 )
	{
		int matrix = MATERIAL_MODEL;
		if (VertexShaderTransformChanged(matrix))
		{
			int vertexShaderConstant = VERTEX_SHADER_MODEL + iMatrix * 3;

			// Put the transform into the vertex shader constants...
			DirectX::XMMATRIX transpose = DirectX::XMMatrixTranspose( GetTransform(matrix) );
			SetVertexShaderConstant( vertexShaderConstant, reinterpret_cast<float*>(&transpose.r[0]), 3 );

			// clear the change flag
			m_DynamicState.m_TransformChanged[matrix] &= ~STATE_CHANGED_VERTEX_SHADER;
		}
	}
	else
	{
		SetVertexShaderConstant( VERTEX_SHADER_MODEL + iMatrix, m_boneMatrix[iMatrix].Base(), 3 );
	}
}


void CShaderAPIDx8::SetVertexShaderStateSkinningMatrices()
{
	//GLM_FUNC;
	// casting from 4x3 matrices to a 4x4 DirectX::XMMATRIX, need 4 floats of overflow
	// get the first one from the MATERIAL_MODEL matrix stack
	DirectX::XMMATRIX result = DirectX::XMMatrixTranspose( GetTransform( MATERIAL_MODEL ) );
	memcpy( m_boneMatrix[0].Base(), result.r, 12 * sizeof(float) );

	m_maxBoneLoaded++;
	int matricesLoaded = max( 1, m_maxBoneLoaded );
	m_maxBoneLoaded = 0;

	m_DynamicState.m_TransformChanged[MATERIAL_MODEL] &= ~STATE_CHANGED_VERTEX_SHADER;
	SetVertexShaderConstant( VERTEX_SHADER_MODEL, m_boneMatrix[0].Base(), matricesLoaded * 3, true );

	// ###OSX### punting on OSX for now		
#if defined( DX_TO_GL_ABSTRACTION ) && !defined( OSX )
	Dx9Device()->SetMaxUsedVertexShaderConstantsHint( VERTEX_SHADER_MODEL + ( matricesLoaded * 3 ) );
#endif

}

//-----------------------------------------------------------------------------
// Commits vertex shader transforms that can change on a per pass basis
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitPerPassVertexShaderTransforms()
{
	//GLMPRINTF(( ">-M- CommitPerPassVertexShaderTransforms" ));
	Assert( g_pHardwareConfig->Caps().m_SupportsPixelShaders );

	bool projChanged = VertexShaderTransformChanged( MATERIAL_PROJECTION );
	//projChanged = true;	//only for debug
	if ( projChanged )
	{
		//GLMPRINTF(( "-M- projChanged=true in CommitPerPassVertexShaderTransforms" ));
		SetVertexShaderViewProj();
		SetVertexShaderModelViewProjAndModelView();

		// Clear change flags
		m_DynamicState.m_TransformChanged[MATERIAL_PROJECTION] &= ~STATE_CHANGED_VERTEX_SHADER;
	}
	else
	{		
		//GLMPRINTF(( "-M- projChanged=false in CommitPerPassVertexShaderTransforms" ));
	}

	//GLMPRINTF(( "<-M- CommitPerPassVertexShaderTransforms" ));
}

//-----------------------------------------------------------------------------
// Commits vertex shader transforms
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitVertexShaderTransforms()
{
	//GLMPRINTF(( ">-M- CommitVertexShaderTransforms" ));

	Assert( g_pHardwareConfig->Caps().m_SupportsPixelShaders );

	bool viewChanged = VertexShaderTransformChanged(MATERIAL_VIEW);
	bool projChanged = VertexShaderTransformChanged(MATERIAL_PROJECTION);
	bool modelChanged = VertexShaderTransformChanged(MATERIAL_MODEL) && (m_DynamicState.m_NumBones < 1);

	//GLMPRINTF(( "-M-     viewChanged=%s  projChanged=%s  modelChanged = %s in CommitVertexShaderTransforms", viewChanged?"Y":"N",projChanged?"Y":"N",modelChanged?"Y":"N" ));
	if (viewChanged)
	{
		//GLMPRINTF(( "-M-       viewChanged --> UpdateVertexShaderFogParams" ));
		UpdateVertexShaderFogParams();
	}

	if( viewChanged || projChanged )
	{
		// NOTE: We have to deal with fast-clip *before* 
		//GLMPRINTF(( "-M-       viewChanged||projChanged --> SetVertexShaderViewProj" ));
		SetVertexShaderViewProj();
	}

	if( viewChanged || modelChanged || projChanged )
	{
		//GLMPRINTF(( "-M-       viewChanged||projChanged||modelChanged --> SetVertexShaderModelViewProjAndModelView" ));
		SetVertexShaderModelViewProjAndModelView();
	}

	if( modelChanged && m_DynamicState.m_NumBones < 1 )
	{
		UpdateVertexShaderMatrix( 0 );
	}

	// Clear change flags
	m_DynamicState.m_TransformChanged[MATERIAL_MODEL]		&= ~STATE_CHANGED_VERTEX_SHADER;
	m_DynamicState.m_TransformChanged[MATERIAL_VIEW]		&= ~STATE_CHANGED_VERTEX_SHADER;
	m_DynamicState.m_TransformChanged[MATERIAL_PROJECTION]	&= ~STATE_CHANGED_VERTEX_SHADER;

	//GLMPRINTF(( "<-M- CommitVertexShaderTransforms" ));
}


void CShaderAPIDx8::UpdateFixedFunctionMatrix( int iMatrix )
{
	int matrix = MATERIAL_MODEL + iMatrix;
	if ( FixedFunctionTransformChanged( matrix ) )
	{
		SetTransform( D3DTS_WORLDMATRIX(iMatrix), &GetTransform(matrix) );

		// clear the change flag
		m_DynamicState.m_TransformChanged[matrix] &= ~STATE_CHANGED_FIXED_FUNCTION;
	}
}


void CShaderAPIDx8::SetFixedFunctionStateSkinningMatrices()
{
	for( int i=1; i < g_pHardwareConfig->MaxBlendMatrices(); i++ )
	{
		UpdateFixedFunctionMatrix( i );
	}
}

//-----------------------------------------------------------------------------
// Commits transforms for the fixed function pipeline that can happen on a per pass basis
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitPerPassFixedFunctionTransforms()
{
	// Update projection
	if ( FixedFunctionTransformChanged( MATERIAL_PROJECTION ) )
	{
		D3DTRANSFORMSTATETYPE matrix = D3DTS_PROJECTION;

		SetTransform( matrix, &GetProjectionMatrix() );
		// clear the change flag
		m_DynamicState.m_TransformChanged[MATERIAL_PROJECTION] &= ~STATE_CHANGED_FIXED_FUNCTION;
	}
}


//-----------------------------------------------------------------------------
// Commits transforms for the fixed function pipeline
//-----------------------------------------------------------------------------

void CShaderAPIDx8::CommitFixedFunctionTransforms()
{
	// Update view + projection
	int i;
	for ( i = MATERIAL_VIEW; i <= MATERIAL_PROJECTION; ++i)
	{
		if (FixedFunctionTransformChanged( i ))
		{
			D3DTRANSFORMSTATETYPE matrix = (i == MATERIAL_VIEW) ? D3DTS_VIEW : D3DTS_PROJECTION;
			if ( i == MATERIAL_PROJECTION )
			{
				SetTransform( matrix, &GetProjectionMatrix() );
			}
			else
			{
				SetTransform( matrix, &GetTransform(i) );
			}

			// clear the change flag
			m_DynamicState.m_TransformChanged[i] &= ~STATE_CHANGED_FIXED_FUNCTION;
		}
	}

	UpdateFixedFunctionMatrix( 0 );
}


void CShaderAPIDx8::SetSkinningMatrices()
{
	LOCK_SHADERAPI();
	Assert( m_pMaterial );

	if ( m_DynamicState.m_NumBones == 0 )
	{
		// ###OSX### punting on OSX for now
#if defined( DX_TO_GL_ABSTRACTION ) && !defined( OSX)
		Dx9Device()->SetMaxUsedVertexShaderConstantsHint( VERTEX_SHADER_BONE_TRANSFORM( 0 ) + 3 );
#endif
		return;
	}
	
	if ( UsesVertexShader(m_pMaterial->GetVertexFormat()) )
	{
		SetVertexShaderStateSkinningMatrices();
	}
	else
	{
#if defined( DX_TO_GL_ABSTRACTION ) && !defined( OSX)
		Assert( 0 );
#else
		SetFixedFunctionStateSkinningMatrices();
#endif
	}
}



//-----------------------------------------------------------------------------
// Commits vertex shader lighting
//-----------------------------------------------------------------------------

inline bool CShaderAPIDx8::VertexShaderLightingChanged( int i )
{
	return (m_DynamicState.m_LightChanged[i] & STATE_CHANGED_VERTEX_SHADER) != 0;
}

inline bool CShaderAPIDx8::VertexShaderLightingEnableChanged( int i )
{
	return (m_DynamicState.m_LightEnableChanged[i] & STATE_CHANGED_VERTEX_SHADER) != 0;
}

inline bool CShaderAPIDx8::FixedFunctionLightingChanged( int i )
{
	return (m_DynamicState.m_LightChanged[i] & STATE_CHANGED_FIXED_FUNCTION) != 0;
}

inline bool CShaderAPIDx8::FixedFunctionLightingEnableChanged( int i )
{
	return (m_DynamicState.m_LightEnableChanged[i] & STATE_CHANGED_FIXED_FUNCTION) != 0;
}


//-----------------------------------------------------------------------------
// Computes the light type
//-----------------------------------------------------------------------------

VertexShaderLightTypes_t CShaderAPIDx8::ComputeLightType( int i ) const
{
	if (!m_DynamicState.m_LightEnable[i])
		return LIGHT_NONE;

	switch( m_DynamicState.m_Lights[i].Type )
	{
	case D3DLIGHT_POINT:
		return LIGHT_POINT;

	case D3DLIGHT_DIRECTIONAL:
		return LIGHT_DIRECTIONAL;

	case D3DLIGHT_SPOT:
		return LIGHT_SPOT;

	default:
		break;
	}

	Assert(0);
	return LIGHT_NONE;
}


//-----------------------------------------------------------------------------
// Sort the lights by type
//-----------------------------------------------------------------------------

void CShaderAPIDx8::SortLights( int* index )
{
	m_DynamicState.m_NumLights = 0;

	for (int i = 0; i < MAX_NUM_LIGHTS; ++i)
	{
		VertexShaderLightTypes_t type = ComputeLightType(i);	// returns LIGHT_NONE if the light is disabled
		int j = m_DynamicState.m_NumLights;
		if (type != LIGHT_NONE)
		{
			while ( --j >= 0 )
			{
				if (m_DynamicState.m_LightType[j] <= type)
					break;

				// shift...
				m_DynamicState.m_LightType[j+1] = m_DynamicState.m_LightType[j];
				index[j+1] = index[j];
			}
			++j;

			m_DynamicState.m_LightType[j] = type;
			index[j] = i;

			++m_DynamicState.m_NumLights;
		}
	}
}

//-----------------------------------------------------------------------------
// Vertex Shader lighting
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitVertexShaderLighting()
{
	// If nothing changed, then don't bother. Otherwise, reload...
	int i;
	for ( i = 0; i < MAX_NUM_LIGHTS; ++i )
	{
		if (VertexShaderLightingChanged(i) || VertexShaderLightingEnableChanged(i))
			break;
	}

	// Yeah baby
	if ( i == MAX_NUM_LIGHTS )
		return;

	// First, gotta sort the lights by their type
	int lightIndex[MAX_NUM_LIGHTS];
	memset( lightIndex, 0, sizeof( lightIndex ) );
	SortLights( lightIndex );

	// Clear the lighting enable flags
	for ( i = 0; i < MAX_NUM_LIGHTS; ++i )
	{
		m_DynamicState.m_LightEnableChanged[i] &= ~STATE_CHANGED_VERTEX_SHADER;
		m_DynamicState.m_LightChanged[i] &= ~STATE_CHANGED_VERTEX_SHADER;
	}

	bool bAtLeastDX90 = g_pHardwareConfig->GetDXSupportLevel() >= 90; 

	// Set the lighting state
	for ( i = 0; i < m_DynamicState.m_NumLights; ++i )
	{
		D3DLIGHT& light = m_DynamicState.m_Lights[lightIndex[i]];

		Vector4D lightState[5];

		// The first one is the light color (and light type code on DX9)
		float w = (light.Type == D3DLIGHT_DIRECTIONAL) && bAtLeastDX90 ? 1.0f : 0.0f;
		lightState[0].Init( light.Diffuse.r, light.Diffuse.g, light.Diffuse.b, w);

		// The next constant holds the light direction (and light type code on DX9)
		w = (light.Type == D3DLIGHT_SPOT) && bAtLeastDX90 ? 1.0f : 0.0f;
		lightState[1].Init( light.Direction.x, light.Direction.y, light.Direction.z, w );

		// The next constant holds the light position
		lightState[2].Init( light.Position.x, light.Position.y, light.Position.z, 1.0f );

		// The next constant holds exponent, stopdot, stopdot2, 1 / (stopdot - stopdot2)
		if (light.Type == D3DLIGHT_SPOT)
		{
			float stopdot = cosf( light.Theta * 0.5f );
			float stopdot2 = cosf( light.Phi * 0.5f );
			float oodot = (stopdot > stopdot2) ? 1.0f / (stopdot - stopdot2) : 0.0f;
			lightState[3].Init( light.Falloff, stopdot, stopdot2, oodot );
		}
		else
		{
			lightState[3].Init( 0, 1, 1, 1 );
		}

		// The last constant holds attenuation0, attenuation1, attenuation2
		lightState[4].Init( light.Attenuation0, light.Attenuation1, light.Attenuation2, 0.0f );

		// Set the state
		SetVertexShaderConstant( VERTEX_SHADER_LIGHTS + i * 5, lightState[0].Base(), 5 );
	}

	if ( g_pHardwareConfig->NumIntegerVertexShaderConstants() > 0 && g_pHardwareConfig->NumBooleanVertexShaderConstants() > 0 )
	{
		// Vertex Shader loop counter for number of lights (Only the .x component is used by our shaders) 
		// .x is the iteration count, .y is the initial value and .z is the increment step 
		int nLoopControl[4] = {m_DynamicState.m_NumLights, 0, 1, 0};
		SetIntegerVertexShaderConstant( 0, nLoopControl, 1 );

		// Enable lights using vertex shader static flow control
		int nLightEnable[VERTEX_SHADER_LIGHT_ENABLE_BOOL_CONST_COUNT] = {0, 0, 0, 0};
		for ( i = 0; i < m_DynamicState.m_NumLights; ++i )
		{
			nLightEnable[i] = 1;
		}

		SetBooleanVertexShaderConstant( VERTEX_SHADER_LIGHT_ENABLE_BOOL_CONST, nLightEnable, VERTEX_SHADER_LIGHT_ENABLE_BOOL_CONST_COUNT );
	}
}

//-----------------------------------------------------------------------------
// Set the pixel shader constants for lights
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitPixelShaderLighting( int pshReg )
{
#ifndef NDEBUG
	[[maybe_unused]] char const *materialName = m_pMaterial->GetName();
#endif

	// First, gotta sort the lights by their type
	int lightIndex[MAX_NUM_LIGHTS];
	SortLights( lightIndex );

	// Offset to create a point light from directional
	constexpr float fFarAway = 10000.0f;

	// Total pixel shader lighting state for four lights
	Vector4D lightState[6];
	for ( int i = 0; i < 6; i++ )
		lightState[i].Init();

	int nNumLights = m_DynamicState.m_NumLights;
	if ( nNumLights > 0  )
	{
		D3DLIGHT *light = &m_DynamicState.m_Lights[lightIndex[0]];
		lightState[0].Init( light->Diffuse.r, light->Diffuse.g, light->Diffuse.b, 0.0f );

		if ( light->Type == D3DLIGHT_DIRECTIONAL )
		{
			Vector vDir(light->Direction.x, light->Direction.y, light->Direction.z );
			Vector vPos = m_DynamicState.m_vLightingOrigin - vDir * fFarAway;
			lightState[1].Init( vPos.x,  vPos.y,  vPos.z, 0.0f );
		}
		else
		{
			lightState[1].Init( light->Position.x,  light->Position.y,  light->Position.z, 0.0f );
		}

		if ( nNumLights > 1 )															// At least two lights
		{
			light = &m_DynamicState.m_Lights[lightIndex[1]];
			lightState[2].Init( light->Diffuse.r, light->Diffuse.g, light->Diffuse.b, 0.0f );

			if ( light->Type == D3DLIGHT_DIRECTIONAL )
			{
				Vector vDir(light->Direction.x, light->Direction.y, light->Direction.z );
				Vector vPos = m_DynamicState.m_vLightingOrigin - vDir * fFarAway;
				lightState[3].Init( vPos.x,  vPos.y,  vPos.z, 0.0f );
			}
			else
			{
				lightState[3].Init( light->Position.x,  light->Position.y,  light->Position.z, 0.0f );
			}

			if ( nNumLights > 2 )														// At least three lights
			{
				light = &m_DynamicState.m_Lights[lightIndex[2]];
				lightState[4].Init( light->Diffuse.r,   light->Diffuse.g,   light->Diffuse.b, 0.0f );

				if ( light->Type == D3DLIGHT_DIRECTIONAL )
				{
					Vector vDir(light->Direction.x, light->Direction.y, light->Direction.z );
					Vector vPos = m_DynamicState.m_vLightingOrigin - vDir * fFarAway;
					lightState[5].Init( vPos.x,  vPos.y,  vPos.z, 0.0f );
				}
				else
				{
					lightState[5].Init( light->Position.x,  light->Position.y,  light->Position.z, 0.0f );
				}

				if ( nNumLights > 3 )													// At least four lights (our current max)
				{
					light = &m_DynamicState.m_Lights[lightIndex[3]];		// Spread 4th light's constants across w components
					lightState[0].w = light->Diffuse.r;
					lightState[1].w = light->Diffuse.g;
					lightState[2].w = light->Diffuse.b;

					if ( light->Type == D3DLIGHT_DIRECTIONAL )
					{
						Vector vDir(light->Direction.x, light->Direction.y, light->Direction.z );
						Vector vPos = m_DynamicState.m_vLightingOrigin - vDir * fFarAway;
						lightState[3].w = vPos.x;
						lightState[4].w = vPos.y;
						lightState[5].w = vPos.z;
					}
					else
					{
						lightState[3].w = light->Position.x;
						lightState[4].w = light->Position.y;
						lightState[5].w = light->Position.z;
					}
				}
			}
		}
	}
	SetPixelShaderConstant( pshReg, lightState[0].Base(), 6 );
}

//-----------------------------------------------------------------------------
// Fixed function lighting
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitFixedFunctionLighting()
{
	// Commit each light
	for (int i = 0; i < g_pHardwareConfig->MaxNumLights(); ++i)
	{
		// Change light enable
		if ( FixedFunctionLightingEnableChanged( i ) )
		{
			LightEnable( i, m_DynamicState.m_LightEnable[i] );

			// Clear change flag
			m_DynamicState.m_LightEnableChanged[i] &= ~STATE_CHANGED_FIXED_FUNCTION;
		}

		// Change lighting state...
		if ( m_DynamicState.m_LightEnable[i] )
		{
			if ( FixedFunctionLightingChanged( i ) )
			{
				// Store off the "correct" falloff...
				D3DLIGHT& light = m_DynamicState.m_Lights[i];

				float falloff = light.Falloff;

				SetLight( i, &light );

				// Clear change flag
				m_DynamicState.m_LightChanged[i] &= ~STATE_CHANGED_FIXED_FUNCTION;

				// restore the correct falloff
				light.Falloff = falloff;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Commits user clip planes
//-----------------------------------------------------------------------------
DirectX::XMMATRIX XM_CALLCONV CShaderAPIDx8::GetUserClipTransform( )
{
	if ( !m_DynamicState.m_bUserClipTransformOverride )
		return GetTransform(MATERIAL_VIEW);

	return m_DynamicState.m_UserClipTransform;
}


//-----------------------------------------------------------------------------
// Commits user clip planes
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitUserClipPlanes( bool bUsingFixedFunction )
{
	// We need to transform the clip planes, specified in world space,
	// to be in projection space.. To transform the plane, we must transform
	// the intercept and then transform the normal.

	if( bUsingFixedFunction != m_DynamicState.m_UserClipLastUpdatedUsingFixedFunction )
	{
		//fixed function clip planes are in world space, vertex shader clip planes are in clip space, so we need to update every clip plane whenever there's a flip
		m_DynamicState.m_UserClipPlaneChanged = (1 << g_pHardwareConfig->MaxUserClipPlanes()) - 1;
		m_DynamicState.m_UserClipLastUpdatedUsingFixedFunction = bUsingFixedFunction;
	}

	// dimhotepus: Add default init as var can be accessed.
	DirectX::XMMATRIX worldToProjectionInvTrans = DirectX::XMMatrixIdentity();
#ifndef _DEBUG
	if( m_DynamicState.m_UserClipPlaneChanged & m_DynamicState.m_UserClipPlaneEnabled & ((1 << g_pHardwareConfig->MaxUserClipPlanes()) - 1) )
#endif
	{
		//we're going to need the transformation matrix at least once this call
		if( bUsingFixedFunction )
		{
			if( m_DynamicState.m_bUserClipTransformOverride )
			{
				//worldToProjectionInvTrans = DirectX::XMMatrixIdentity(); //TODO: Test user clip transforms with this
				//Since GetUserClipTransform() returns the view matrix if a user supplied transform doesn't exist, the general solution to this should be to transform the user transform by the inverse view matrix
				//Since we don't know if the user clip is invertable, we'll premultiply by inverse view and cross our fingers that it's right more often than wrong
				DirectX::XMMATRIX viewInverse = DirectX::XMMatrixInverse( nullptr, GetTransform(MATERIAL_VIEW) );
				worldToProjectionInvTrans = viewInverse * GetUserClipTransform(); //taking a cue from the multiplication below, multiplication goes left into right
				
				worldToProjectionInvTrans = DirectX::XMMatrixTranspose( DirectX::XMMatrixInverse( nullptr, worldToProjectionInvTrans ) );
			}
			else
			{
				worldToProjectionInvTrans = DirectX::XMMatrixIdentity();
			}
		}
		else
		{
			worldToProjectionInvTrans = GetUserClipTransform( ) * GetTransform( MATERIAL_PROJECTION );
			worldToProjectionInvTrans = DirectX::XMMatrixTranspose( DirectX::XMMatrixInverse( nullptr, worldToProjectionInvTrans ) );
		}
	}

	for (int i = 0; i < g_pHardwareConfig->MaxUserClipPlanes(); ++i)
	{
		// Don't bother with the plane if it's not enabled
		if ( (m_DynamicState.m_UserClipPlaneEnabled & (1 << i)) == 0 )
			continue;

		// Don't bother if it didn't change...
		if ( (m_DynamicState.m_UserClipPlaneChanged & (1 << i)) == 0 )
		{
#ifdef _DEBUG
			//verify that the plane has not actually changed
			DirectX::XMVECTOR clipPlaneProj{ DirectX::XMPlaneTransform( m_DynamicState.m_UserClipPlaneWorld[i], worldToProjectionInvTrans ) };
			Assert ( memcmp( &clipPlaneProj, &m_DynamicState.m_UserClipPlaneProj[i], sizeof(clipPlaneProj) ) == 0 );
#endif
			continue;
		}

		m_DynamicState.m_UserClipPlaneChanged &= ~(1 << i);

		DirectX::XMVECTOR clipPlaneProj{ DirectX::XMPlaneTransform( m_DynamicState.m_UserClipPlaneWorld[i], worldToProjectionInvTrans ) };

		if ( memcmp( &clipPlaneProj, &m_DynamicState.m_UserClipPlaneProj[i], sizeof(clipPlaneProj) ) != 0 )
		{
			Dx9Device()->SetClipPlane( i, reinterpret_cast<float*>(&clipPlaneProj) );
			m_DynamicState.m_UserClipPlaneProj[i] = clipPlaneProj;
		}
	}
}


//-----------------------------------------------------------------------------
// Need to handle fog mode on a per-pass basis
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitPerPassFogMode( bool bUsingVertexAndPixelShaders )
{
	D3DFOGMODE dxFogMode = D3DFOG_NONE;
	if ( m_DynamicState.m_FogEnable )
	{
		dxFogMode = bUsingVertexAndPixelShaders ? D3DFOG_NONE : D3DFOG_LINEAR;
	}

	// Set fog mode if it's different than before.
	if( m_DynamicState.m_FogMode != dxFogMode )
	{
		SetRenderStateConstMacro( this, D3DRS_FOGVERTEXMODE, dxFogMode );
		m_DynamicState.m_FogMode = dxFogMode;
	}
}

//-----------------------------------------------------------------------------
// These states can change between each pass
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitPerPassStateChanges( StateSnapshot_t id )
{
	if ( UsesVertexAndPixelShaders(id) )
	{
		CommitPerPassVertexShaderTransforms();
		CommitPerPassFogMode( true );
		CallCommitFuncs( COMMIT_PER_PASS, false );
	}
	else
	{
		CommitPerPassFixedFunctionTransforms();
		CommitPerPassFogMode( false );
		CallCommitFuncs( COMMIT_PER_PASS, true );
	}
}


//-----------------------------------------------------------------------------
// Commits transforms and lighting
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CommitStateChanges()
{
	VPROF("CShaderAPIDx8::CommitStateChanges");
	CommitFastClipPlane();

	bool bUsingFixedFunction = m_pMaterial && !UsesVertexShader( m_pMaterial->GetVertexFormat() );
	if ( !bUsingFixedFunction )
	{
		CommitVertexShaderTransforms();

		if ( m_pMaterial && m_pMaterial->IsVertexLit() )
		{
			CommitVertexShaderLighting();
		}
	}
	else
	{
		CommitFixedFunctionTransforms();

		if ( m_pMaterial && ( m_pMaterial->IsVertexLit() || m_pMaterial->NeedsFixedFunctionFlashlight() ) )
		{
			CommitFixedFunctionLighting();
		}
	}

	if ( m_DynamicState.m_UserClipPlaneEnabled )
	{
		CommitUserClipPlanes( bUsingFixedFunction );
	}

	CallCommitFuncs( COMMIT_PER_DRAW, bUsingFixedFunction );
}

//-----------------------------------------------------------------------------
// Commits viewports
//-----------------------------------------------------------------------------
static void CommitSetViewports( IDirect3DDevice9 *pDevice, const DynamicState_t &desiredState, DynamicState_t &currentState, bool bForce )
{
	bool bChanged = bForce || memcmp( &desiredState.m_Viewport, &currentState.m_Viewport, sizeof(D3DVIEWPORT9) );

	// The width + height can be zero at startup sometimes.
	if ( bChanged && ( desiredState.m_Viewport.Width != 0 ) && ( desiredState.m_Viewport.Height != 0 ) )
	{
		if( ReverseDepthOnX360() ) //reverse depth on 360 for better perf through hierarchical z
		{
			D3DVIEWPORT9 reverseDepthViewport;
			reverseDepthViewport = desiredState.m_Viewport;
			reverseDepthViewport.MinZ = 1.0f - desiredState.m_Viewport.MinZ;
			reverseDepthViewport.MaxZ = 1.0f - desiredState.m_Viewport.MaxZ;
			Dx9Device()->SetViewport( &reverseDepthViewport );
		}
		else
		{
			Dx9Device()->SetViewport( &desiredState.m_Viewport );
		}
		memcpy( &currentState.m_Viewport, &desiredState.m_Viewport, sizeof( D3DVIEWPORT9 ) );
	}
}


void CShaderAPIDx8::SetViewports( int nCount, const ShaderViewport_t* pViewports )
{
	Assert( nCount == 1 && pViewports[0].m_nVersion == SHADER_VIEWPORT_VERSION );
	if ( nCount != 1 )
		return;

	LOCK_SHADERAPI();

	D3DVIEWPORT9 viewport;
	viewport.X = pViewports[0].m_nTopLeftX;
	viewport.Y = pViewports[0].m_nTopLeftY;
	viewport.Width = pViewports[0].m_nWidth;
	viewport.Height = pViewports[0].m_nHeight;
	viewport.MinZ = pViewports[0].m_flMinZ; 
	viewport.MaxZ = pViewports[0].m_flMaxZ;

	// Clamp the viewport to the current render target...
	if ( !m_UsingTextureRenderTarget )
	{
		// Clamp to both the back buffer and the window, if it is resizing
		int nMaxWidth = 0, nMaxHeight = 0;
		GetBackBufferDimensions( nMaxWidth, nMaxHeight );
		if ( IsPC() && m_IsResizing )
		{
			RECT viewRect;
#if !defined( DX_TO_GL_ABSTRACTION )
			GetClientRect( ( HWND )m_ViewHWnd, &viewRect );
#else
			toglGetClientRect( (VD3DHWND)m_ViewHWnd, &viewRect );
#endif
			m_nWindowWidth = viewRect.right - viewRect.left;
			m_nWindowHeight = viewRect.bottom - viewRect.top;
			nMaxWidth = min( m_nWindowWidth, nMaxWidth );
			nMaxHeight = min( m_nWindowHeight, nMaxHeight );
		}

		// Dimensions can freak out on app exit, so at least make sure the viewport is positive
		if ( (viewport.Width > (unsigned int)nMaxWidth ) && (nMaxWidth > 0) )
		{
			viewport.Width = nMaxWidth;
		}

		// Dimensions can freak out on app exit, so at least make sure the viewport is positive
		if ( ( viewport.Height > (unsigned int)nMaxHeight ) && (nMaxHeight > 0) )
		{
			viewport.Height = nMaxHeight;
		}
	}
	else
	{
		if ( viewport.Width > (unsigned int)m_ViewportMaxWidth )
		{
			viewport.Width = m_ViewportMaxWidth;
		}
		if ( viewport.Height > (unsigned int)m_ViewportMaxHeight )
		{
			viewport.Height = m_ViewportMaxHeight;
		}
	}

	// FIXME: Once we extract buffered primitives out, we can directly fill in desired state
	// and avoid the memcmp and copy
	if ( memcmp( &m_DesiredState.m_Viewport, &viewport, sizeof(D3DVIEWPORT9) ) )
	{
		if ( !IsDeactivated() )
		{
			// State changed... need to flush the dynamic buffer
			FlushBufferedPrimitives();
		}

		memcpy( &m_DesiredState.m_Viewport, &viewport, sizeof(D3DVIEWPORT9) );
	}

	ADD_COMMIT_FUNC( COMMIT_PER_DRAW, COMMIT_ALWAYS, CommitSetViewports );
}


//-----------------------------------------------------------------------------
// Gets the current viewport size
//-----------------------------------------------------------------------------
int CShaderAPIDx8::GetViewports( ShaderViewport_t* pViewports, int nMax ) const
{
	if ( !pViewports || nMax == 0 )
		return 1;

	LOCK_SHADERAPI();

	pViewports[0].m_nTopLeftX = m_DesiredState.m_Viewport.X;
	pViewports[0].m_nTopLeftY = m_DesiredState.m_Viewport.Y;
	pViewports[0].m_nWidth = m_DesiredState.m_Viewport.Width;
	pViewports[0].m_nHeight = m_DesiredState.m_Viewport.Height;
	pViewports[0].m_flMinZ = m_DesiredState.m_Viewport.MinZ;
	pViewports[0].m_flMaxZ = m_DesiredState.m_Viewport.MaxZ;
	return 1;
}


//-----------------------------------------------------------------------------
// Flushes buffered primitives
//-----------------------------------------------------------------------------
void CShaderAPIDx8::FlushBufferedPrimitives( )
{
	if ( ShaderUtil() )
	{
		if ( !ShaderUtil()->OnFlushBufferedPrimitives() )
		{
			return;
		}
	}
	FlushBufferedPrimitivesInternal();
}

void CShaderAPIDx8::FlushBufferedPrimitivesInternal( )
{
	LOCK_SHADERAPI();
	// This shouldn't happen in the inner rendering loop!
	Assert( m_pRenderMesh == 0 );

	// NOTE: We've gotta store off the matrix mode because
	// it'll get reset by the default state application caused by the flush
	int tempStack = m_CurrStack;
	D3DTRANSFORMSTATETYPE tempMatrixMode = m_MatrixMode;

	MeshMgr()->Flush();

	m_CurrStack = tempStack;
	m_MatrixMode = tempMatrixMode;
}


//-----------------------------------------------------------------------------
// Flush the hardware
//-----------------------------------------------------------------------------
void CShaderAPIDx8::FlushHardware( )
{
	LOCK_SHADERAPI();
	FlushBufferedPrimitives();

	HRESULT hr = Dx9Device()->EndScene();
	if (FAILED(hr))
	{
		Assert(false);
		Warning( __FUNCTION__ ": IDirect3DDevice9Ex::EndScene failed w/e %s.\n",
			se::win::com::com_error_category().message(hr).c_str() );
	}

	DiscardVertexBuffers();

	hr = Dx9Device()->BeginScene();
	if (FAILED(hr))
	{
		Assert(false);
		Warning( __FUNCTION__ ": IDirect3DDevice9Ex::BeginScene failed w/e %s.\n",
			se::win::com::com_error_category().message(hr).c_str() );
	}
	
	ForceHardwareSync();
}


	
//-----------------------------------------------------------------------------
// Deal with device lost (alt-tab)
//-----------------------------------------------------------------------------
void CShaderAPIDx8::HandleDeviceLost()
{
	LOCK_SHADERAPI();

	if ( !IsActive() )
		return;

	// need to flush the dynamic buffer
	FlushBufferedPrimitives();

	if ( !IsDeactivated() )
	{
		const HRESULT hr = Dx9Device()->EndScene();
		if (FAILED(hr))
		{
			Assert(false);
			Warning( __FUNCTION__ ": IDirect3DDevice9Ex::EndScene failed w/e %s.\n",
				se::win::com::com_error_category().message(hr).c_str() );
		}
	}

	CheckDeviceState( m_bOtherAppInitializing );

	if ( !IsDeactivated() )
	{
		const HRESULT hr = Dx9Device()->BeginScene();
		if (FAILED(hr))
		{
			Assert(false);
			Warning( __FUNCTION__ ": IDirect3DDevice9Ex::BeginScene failed w/e %s.\n",
				se::win::com::com_error_category().message(hr).c_str() );
		}
	}
}

//-----------------------------------------------------------------------------
// Buffer clear	color
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ClearColor3ub( unsigned char r, unsigned char g, unsigned char b )
{
	LOCK_SHADERAPI();
	float a = 255;//(r * 0.30f + g * 0.59f + b * 0.11f) / MAX_HDR_OVERBRIGHT;

	// GR - need to force alpha to black for HDR
	m_DynamicState.m_ClearColor = D3DCOLOR_ARGB((unsigned char)a,r,g,b);
}

void CShaderAPIDx8::ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{
	LOCK_SHADERAPI();
	m_DynamicState.m_ClearColor = D3DCOLOR_ARGB(a,r,g,b);
}

// Converts the clear color to be appropriate for HDR
D3DCOLOR CShaderAPIDx8::GetActualClearColor( D3DCOLOR clearColor )
{
	bool bConvert = m_TransitionTable.CurrentState().m_bLinearColorSpaceFrameBufferEnable;
	if ( bConvert )
	{
		// HDRFIXME: need to make sure this works this way.
		// HDRFIXME: Is there a helper function that'll do this easier?
		// convert clearColor from gamma to linear since our frame buffer is linear.
		Vector vecGammaColor;
		vecGammaColor.x = ( 1.0f / 255.0f ) * ( ( clearColor >> 16 ) & 0xff );
		vecGammaColor.y = ( 1.0f / 255.0f ) * ( ( clearColor >> 8 ) & 0xff );
		vecGammaColor.z = ( 1.0f / 255.0f ) * ( clearColor & 0xff );
		Vector vecLinearColor;
		vecLinearColor.x = GammaToLinear( vecGammaColor.x );
		vecLinearColor.y = GammaToLinear( vecGammaColor.y );
		vecLinearColor.z = GammaToLinear( vecGammaColor.z );
		clearColor &= D3DCOLOR_RGBA( 0, 0, 0, 255 );
		clearColor |= D3DCOLOR_COLORVALUE( vecLinearColor.x, vecLinearColor.y, vecLinearColor.z, 0.0f );
	}

	return clearColor;
}


//-----------------------------------------------------------------------------
// Clear buffers while obeying stencil
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth )
{
	//copy the clear color bool into the clear alpha bool
	ClearBuffersObeyStencilEx( bClearColor, bClearColor, bClearDepth );
}

void CShaderAPIDx8::ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth )
{
	LOCK_SHADERAPI();

	if ( !bClearColor && !bClearAlpha && !bClearDepth )
		return;

	FlushBufferedPrimitives();

	// Before clearing can happen, user clip planes must be disabled
	SetRenderState( D3DRS_CLIPPLANEENABLE, 0 );

	D3DCOLOR clearColor = GetActualClearColor( m_DynamicState.m_ClearColor );

	unsigned char r, g, b, a;
	b = clearColor& 0xFF;
	g = ( clearColor >> 8 ) & 0xFF;
	r = ( clearColor >> 16 ) & 0xFF;
	a = ( clearColor >> 24 ) & 0xFF;

	ShaderUtil()->DrawClearBufferQuad( r, g, b, a, bClearColor, bClearAlpha, bClearDepth );

	// Reset user clip plane state
	FlushBufferedPrimitives();
	SetRenderState( D3DRS_CLIPPLANEENABLE, m_DynamicState.m_UserClipPlaneEnabled );
}

//-------------------------------------------------------------------------
//Perform stencil operations to every pixel on the screen
//-------------------------------------------------------------------------
void CShaderAPIDx8::PerformFullScreenStencilOperation( void )
{
	LOCK_SHADERAPI();

	FlushBufferedPrimitives();

	// We'll be drawing a large quad in altered worldspace, user clip planes must be disabled
	SetRenderStateConstMacro( this, D3DRS_CLIPPLANEENABLE, 0 );

	ShaderUtil()->DrawClearBufferQuad( 0, 0, 0, 0, false, false, false );

	// Reset user clip plane state
	FlushBufferedPrimitives();
	SetRenderStateConstMacro( this, D3DRS_CLIPPLANEENABLE, m_DynamicState.m_UserClipPlaneEnabled );
}


//-----------------------------------------------------------------------------
// Buffer clear
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil, int renderTargetWidth, int renderTargetHeight )
{
	LOCK_SHADERAPI();
	if ( ShaderUtil()->GetConfig().m_bSuppressRendering )
		return;

	if ( IsDeactivated() )
		return;

	// State changed... need to flush the dynamic buffer
	FlushBufferedPrimitives();
	CallCommitFuncs( COMMIT_PER_DRAW, true );

	float depth = (ShaderUtil()->GetConfig().bReverseDepth ^ ReverseDepthOnX360()) ? 0.0f : 1.0f;
	DWORD mask = 0;

	if ( bClearColor )
	{
		mask |= D3DCLEAR_TARGET;
	}

	if ( bClearDepth )
	{
		mask |= D3DCLEAR_ZBUFFER;
	}
	
	if ( bClearStencil && m_bUsingStencil )
	{
		mask |= D3DCLEAR_STENCIL;
	}

	// Only clear the current view... right!??!
	D3DRECT clear;
	clear.x1 = m_DesiredState.m_Viewport.X;
	clear.y1 = m_DesiredState.m_Viewport.Y;
	clear.x2 = clear.x1 + m_DesiredState.m_Viewport.Width;
	clear.y2 = clear.y1 + m_DesiredState.m_Viewport.Height;

	// SRGBWrite is disabled when clearing so that the clear color won't get gamma converted
	bool bSRGBWriteEnable = false;
	if ( bClearColor && m_TransitionTable.CurrentShadowState() )
	{
		bSRGBWriteEnable = m_TransitionTable.CurrentShadowState()->m_SRGBWriteEnable;
	}
	
	if ( bSRGBWriteEnable )
	{
		// This path used to be !IsPosix(), but this makes no sense and causes the clear color to differ in D3D9 vs. GL.
		Dx9Device()->SetRenderState( D3DRS_SRGBWRITEENABLE, 0 );
	}
	
	D3DCOLOR clearColor = GetActualClearColor( m_DynamicState.m_ClearColor );

	if ( mask != 0 )
	{
		bool bRenderTargetMatchesViewport = 
			( renderTargetWidth == -1 && renderTargetHeight == -1 ) ||
			( m_DesiredState.m_Viewport.Width == ULONG_MAX && m_DesiredState.m_Viewport.Height == ULONG_MAX ) ||
			( renderTargetWidth ==  ( int )m_DesiredState.m_Viewport.Width &&
			  renderTargetHeight == ( int )m_DesiredState.m_Viewport.Height );

		if ( bRenderTargetMatchesViewport )
		{
			RECORD_COMMAND( DX8_CLEAR, 6 );
			RECORD_INT( 0 );
			RECORD_STRUCT( &clear, sizeof(clear) );
			RECORD_INT( mask );
			RECORD_INT( clearColor );
			RECORD_FLOAT( depth );
			RECORD_INT( 0 );

			Dx9Device()->Clear( 0, NULL, mask, clearColor, depth, 0L );	
		}
		else
		{
			RECORD_COMMAND( DX8_CLEAR, 6 );
			RECORD_INT( 0 );
			RECORD_STRUCT( &clear, sizeof(clear) );
			RECORD_INT( mask );
			RECORD_INT( clearColor );
			RECORD_FLOAT( depth );
			RECORD_INT( 0 );

			Dx9Device()->Clear( 1, &clear, mask, clearColor, depth, 0L );	
		}
	}

	// Restore state
	if ( bSRGBWriteEnable )
	{
		// sRGBWriteEnable shouldn't be true if we have no shadow state. . . Assert just in case.
		Assert( m_TransitionTable.CurrentShadowState() );
		m_TransitionTable.ApplySRGBWriteEnable( *m_TransitionTable.CurrentShadowState() );
	}
}


//-----------------------------------------------------------------------------
// Bind
//-----------------------------------------------------------------------------
void CShaderAPIDx8::BindVertexShader( VertexShaderHandle_t hVertexShader )
{
	ShaderManager()->BindVertexShader( hVertexShader );
}

void CShaderAPIDx8::BindGeometryShader( GeometryShaderHandle_t hGeometryShader )
{
	Assert( hGeometryShader == GEOMETRY_SHADER_HANDLE_INVALID );
}

void CShaderAPIDx8::BindPixelShader( PixelShaderHandle_t hPixelShader )
{
	ShaderManager()->BindPixelShader( hPixelShader );
}


//-----------------------------------------------------------------------------
// Returns a copy of the front buffer
//-----------------------------------------------------------------------------
se::win::com::com_ptr<IDirect3DSurface> CShaderAPIDx8::GetFrontBufferImage( ImageFormat& format )
{
	// need to flush the dynamic buffer	and make sure the entire image is there
	FlushBufferedPrimitives();

	int w, h;
	GetBackBufferDimensions( w, h );

	se::win::com::com_ptr<IDirect3DSurface9> pFullScreenSurfaceBits;
	HRESULT hr = Dx9Device()->CreateOffscreenPlainSurface( w, h, 
		D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &pFullScreenSurfaceBits, NULL );
	if (FAILED(hr))
		return {};

	hr = Dx9Device()->GetFrontBufferData( 0, pFullScreenSurfaceBits );
	if (FAILED(hr))
		return {};

	int windowWidth, windowHeight;
	GetWindowSize( windowWidth, windowHeight );
	
	se::win::com::com_ptr<IDirect3DSurface9> pSurfaceBits;
	hr = Dx9Device()->CreateOffscreenPlainSurface( windowWidth, windowHeight,
		D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &pSurfaceBits, NULL );
	Assert( SUCCEEDED( hr ) );
	
	POINT pnt{ 0, 0 };
#ifdef _WIN32
	BOOL result = ClientToScreen( ( HWND )m_hWnd, &pnt );
#else
	BOOL result = ClientToScreen( (VD3DHWND)m_hWnd, &pnt );
#endif
	Assert( result );

	RECT srcRect;
	srcRect.left = pnt.x;
	srcRect.top = pnt.y;
	srcRect.right = pnt.x + windowWidth;
	srcRect.bottom = pnt.y + windowHeight;

	POINT dstPnt;
	dstPnt.x = dstPnt.y = 0;
	
	D3DLOCKED_RECT lockedSrcRect;
	hr = pFullScreenSurfaceBits->LockRect( &lockedSrcRect, &srcRect, D3DLOCK_READONLY );
	Assert( SUCCEEDED( hr ) );
	
	D3DLOCKED_RECT lockedDstRect;
	hr = pSurfaceBits->LockRect( &lockedDstRect, NULL, 0 );
	Assert( SUCCEEDED( hr ) );

	for( int i = 0; i < windowHeight; i++ )
	{
		memcpy( ( unsigned char * )lockedDstRect.pBits + ( i * lockedDstRect.Pitch ), 
			    ( unsigned char * )lockedSrcRect.pBits + ( i * lockedSrcRect.Pitch ),
				windowWidth * 4 ); // hack . .  what if this is a different format?
	}
	hr = pSurfaceBits->UnlockRect();
	Assert( SUCCEEDED( hr ) );
	hr = pFullScreenSurfaceBits->UnlockRect();
	Assert( SUCCEEDED( hr ) );

	format = ImageLoader::D3DFormatToImageFormat( D3DFMT_A8R8G8B8 );
	return pSurfaceBits;
}


//-----------------------------------------------------------------------------
// Lets the shader know about the full-screen texture so it can 
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetFullScreenTextureHandle( ShaderAPITextureHandle_t h )
{
	LOCK_SHADERAPI();
	m_hFullScreenTexture = h;
}


//-----------------------------------------------------------------------------
// Lets the shader know about the full-screen texture so it can 
//-----------------------------------------------------------------------------
void CShaderAPIDx8::SetLinearToGammaConversionTextures( ShaderAPITextureHandle_t hSRGBWriteEnabledTexture, ShaderAPITextureHandle_t hIdentityTexture )
{
	LOCK_SHADERAPI();

	m_hLinearToGammaTableTexture = hSRGBWriteEnabledTexture;
	m_hLinearToGammaTableIdentityTexture = hIdentityTexture;
}

//-----------------------------------------------------------------------------
// Returns a copy of the back buffer
//-----------------------------------------------------------------------------
se::win::com::com_ptr<IDirect3DSurface> CShaderAPIDx8::GetBackBufferImageHDR( Rect_t *pSrcRect, Rect_t *pDstRect, ImageFormat& format )
{
	// Get the back buffer
	se::win::com::com_ptr<IDirect3DSurface9> pBackBuffer;
	HRESULT hr = Dx9Device()->GetRenderTarget( 0, &pBackBuffer );
	if (FAILED(hr))
		return {};

	// Find about its size and format
	D3DSURFACE_DESC desc;
	hr = pBackBuffer->GetDesc( &desc );
	if (FAILED(hr))
		return {};
	
	D3DTEXTUREFILTERTYPE filter = ((pDstRect->width != pSrcRect->width) ||
		(pDstRect->height != pSrcRect->height)) ? D3DTEXF_LINEAR : D3DTEXF_NONE;
	
	se::win::com::com_ptr<IDirect3DSurface> pTmpSurface;
	if ( ( pDstRect->x + pDstRect->width <= SMALL_BACK_BUFFER_SURFACE_WIDTH ) && 
		 ( pDstRect->y + pDstRect->height <= SMALL_BACK_BUFFER_SURFACE_HEIGHT ) )
	{
		if (!m_pSmallBackBufferFP16TempSurface)
		{
			hr = Dx9Device()->CreateRenderTarget( 
				SMALL_BACK_BUFFER_SURFACE_WIDTH, SMALL_BACK_BUFFER_SURFACE_HEIGHT, 
				desc.Format, D3DMULTISAMPLE_NONE, 0, TRUE, &m_pSmallBackBufferFP16TempSurface,
				NULL );
		}
		pTmpSurface = m_pSmallBackBufferFP16TempSurface;

		desc.Width = SMALL_BACK_BUFFER_SURFACE_WIDTH;
		desc.Height = SMALL_BACK_BUFFER_SURFACE_HEIGHT;

		RECT srcRect, destRect;
		RectToRECT( pSrcRect, srcRect );
		RectToRECT( pDstRect, destRect );

		hr = Dx9Device()->StretchRect( pBackBuffer, &srcRect, pTmpSurface, &destRect, filter );
		if ( FAILED(hr) )
			return {};
	}
	else
	{
		// Normally we would only have to create a separate render target here and StretchBlt to it first
		// if AA was enabled, but certain machines/drivers get reboots if we do GetRenderTargetData 
		// straight off the backbuffer.
		hr = Dx9Device()->CreateRenderTarget( desc.Width, desc.Height, desc.Format,
			D3DMULTISAMPLE_NONE, 0, TRUE, &pTmpSurface, NULL );
		if ( FAILED(hr) )
			return {};

		hr = Dx9Device()->StretchRect( pBackBuffer, NULL, pTmpSurface, NULL, filter );
		if ( FAILED(hr) )
			return {};
	}
	
	se::win::com::com_ptr<IDirect3DSurface> pSurfaceBits;
	// Create a buffer the same size and format
	hr = Dx9Device()->CreateOffscreenPlainSurface( desc.Width, desc.Height, 
		desc.Format, D3DPOOL_SYSTEMMEM, &pSurfaceBits, NULL );
	if (FAILED(hr))
		return {};

	// Blit from the back buffer to our scratch buffer
	hr = Dx9Device()->GetRenderTargetData( pTmpSurface ? pTmpSurface : pBackBuffer, pSurfaceBits );
	if (FAILED(hr))
		return {};

	format = ImageLoader::D3DFormatToImageFormat(desc.Format);
	return pSurfaceBits;
}


//-----------------------------------------------------------------------------
// Returns a copy of the back buffer
//-----------------------------------------------------------------------------
se::win::com::com_ptr<IDirect3DSurface> CShaderAPIDx8::GetBackBufferImage( Rect_t *pSrcRect, Rect_t *pDstRect, ImageFormat& format )
{
	if ( !m_pBackBufferSurface || ( m_hFullScreenTexture == INVALID_SHADERAPI_TEXTURE_HANDLE ) )
		return {};

	FlushBufferedPrimitives();

	// Get the current render target
	se::win::com::com_ptr<IDirect3DSurface9> pRenderTarget;
	HRESULT hr = Dx9Device()->GetRenderTarget( 0, &pRenderTarget );
	if (FAILED(hr))
		return {};
	
	D3DSURFACE_DESC desc;
	// Find about its size and format
	hr = pRenderTarget->GetDesc( &desc );
	if (FAILED(hr))
		return {};

	if ( desc.Format == D3DFMT_A16B16G16R16F || desc.Format == D3DFMT_A32B32G32R32F )
		return GetBackBufferImageHDR( pSrcRect, pDstRect, format );

	se::win::com::com_ptr<IDirect3DSurface9> pTmpSurface;
	if ( (desc.MultiSampleType == D3DMULTISAMPLE_NONE) && (pRenderTarget != m_pBackBufferSurface) && 
		 (pSrcRect->width == pDstRect->width) && (pSrcRect->height == pDstRect->height) )
	{
		// Don't bother to blit through the full-screen texture if we don't
		// have to stretch, we're not coming from the backbuffer, and we don't have to do AA resolve
		pTmpSurface = pRenderTarget;
	}
	else
	{
		Texture_t &pTex = GetTexture( m_hFullScreenTexture );
		IDirect3DTexture* pFullScreenTexture = (IDirect3DTexture*)pTex.GetTexture();

		D3DTEXTUREFILTERTYPE filter = ((pDstRect->width != pSrcRect->width) || (pDstRect->height != pSrcRect->height)) ? D3DTEXF_LINEAR : D3DTEXF_NONE;

		hr = pFullScreenTexture->GetSurfaceLevel( 0, &pTmpSurface ); 
		if ( FAILED(hr) )
			return {};

		if ( pTmpSurface == pRenderTarget )
		{
			Warning( "Can't blit from full-sized offscreen buffer!\n" );
			return {};
		}

		RECT srcRect, destRect;
		srcRect.left = pSrcRect->x; srcRect.right = pSrcRect->x + pSrcRect->width;
		srcRect.top = pSrcRect->y; srcRect.bottom = pSrcRect->y + pSrcRect->height;
		srcRect.left = std::clamp( srcRect.left, 0L, (long)desc.Width );
		srcRect.right = std::clamp( srcRect.right, 0L, (long)desc.Width );
		srcRect.top = std::clamp( srcRect.top, 0L, (long)desc.Height );
		srcRect.bottom = std::clamp( srcRect.bottom, 0L, (long)desc.Height );

		destRect.left = pDstRect->x ; destRect.right = pDstRect->x + pDstRect->width;
		destRect.top = pDstRect->y; destRect.bottom = pDstRect->y + pDstRect->height;
		destRect.left = std::clamp( destRect.left, 0L, (long)desc.Width );
		destRect.right = std::clamp( destRect.right, 0L, (long)desc.Width );
		destRect.top = std::clamp( destRect.top, 0L, (long)desc.Height );
		destRect.bottom = std::clamp( destRect.bottom, 0L, (long)desc.Height );

		hr = Dx9Device()->StretchRect( pRenderTarget, &srcRect, pTmpSurface, &destRect, filter );
		if ( FAILED(hr) )
		{
			AssertOnce( "Error resizing pixels!\n" );
			return {};
		}
	}

	D3DSURFACE_DESC tmpDesc;
	hr = pTmpSurface->GetDesc( &tmpDesc );
	Assert( SUCCEEDED(hr) );
	
	se::win::com::com_ptr<IDirect3DSurface9> pSurfaceBits;
	// Create a buffer the same size and format
	hr = Dx9Device()->CreateOffscreenPlainSurface( tmpDesc.Width, tmpDesc.Height, 
		desc.Format, D3DPOOL_SYSTEMMEM, &pSurfaceBits, NULL );
	if ( FAILED(hr) )
	{
		AssertOnce( "Error creating offscreen surface!\n" );
		return {};
	}

	// Blit from the back buffer to our scratch buffer
	hr = Dx9Device()->GetRenderTargetData( pTmpSurface, pSurfaceBits );
	if ( FAILED(hr) )
	{
		AssertOnce( "Error copying bits into the offscreen surface!\n" );
		return {};
	}

	format = ImageLoader::D3DFormatToImageFormat( desc.Format );
	return pSurfaceBits;
}


//-----------------------------------------------------------------------------
// Copy bits from a host-memory surface
//-----------------------------------------------------------------------------
void CShaderAPIDx8::CopyBitsFromHostSurface( IDirect3DSurface* pSurfaceBits, 
	const Rect_t &dstRect, unsigned char *pData, ImageFormat srcFormat, ImageFormat dstFormat, int nDstStride )
{
	// Copy out the bits...
	RECT rect;
	rect.left   = dstRect.x; 
	rect.right  = dstRect.x + dstRect.width; 
	rect.top    = dstRect.y; 
	rect.bottom = dstRect.y + dstRect.height;

	D3DLOCKED_RECT lockedRect;
	constexpr DWORD flags = D3DLOCK_READONLY | D3DLOCK_NOSYSLOCK;

	tmZone( TELEMETRY_LEVEL1, TMZF_NONE, "%s", __FUNCTION__ );

	HRESULT hr = pSurfaceBits->LockRect( &lockedRect, &rect, flags );
	if ( !FAILED( hr ) )
	{
		unsigned char *pImage = static_cast<unsigned char *>(lockedRect.pBits);
		ShaderUtil()->ConvertImageFormat( pImage, srcFormat,
			pData, dstFormat, dstRect.width, dstRect.height, lockedRect.Pitch, nDstStride );

		hr = pSurfaceBits->UnlockRect( );
		Assert( SUCCEEDED(hr) );
	}
}


//-----------------------------------------------------------------------------
// Reads from the current read buffer  + stretches
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ReadPixels( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *pData, ImageFormat dstFormat, int nDstStride )
{
	LOCK_SHADERAPI();
	Assert( pDstRect );
	
	Rect_t srcRect;
	if ( !pSrcRect )
	{
		srcRect.x = srcRect.y = 0;
		srcRect.width = m_nWindowWidth;
		srcRect.height = m_nWindowHeight;
		pSrcRect = &srcRect;
	}

	ImageFormat format;
	se::win::com::com_ptr<IDirect3DSurface> pSurfaceBits = GetBackBufferImage( pSrcRect, pDstRect, format );
	if ( pSurfaceBits )
	{
		CopyBitsFromHostSurface( pSurfaceBits, *pDstRect, pData, format, dstFormat, nDstStride );
	}
}


//-----------------------------------------------------------------------------
// Reads from the current read buffer
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ReadPixels( int x, int y, int width, int height, unsigned char *pData, ImageFormat dstFormat )
{
	Rect_t rect;
	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height;

	ImageFormat format;
	se::win::com::com_ptr<IDirect3DSurface> pSurfaceBits = GetBackBufferImage( &rect, &rect, format );
	if (pSurfaceBits)
	{
		CopyBitsFromHostSurface( pSurfaceBits, rect, pData, format, dstFormat, 0 );
	}
}


//-----------------------------------------------------------------------------
// Binds a particular material to render with
//-----------------------------------------------------------------------------
void CShaderAPIDx8::Bind( IMaterial* pMaterial )
{
	LOCK_SHADERAPI();
	IMaterialInternal* pMatInt = static_cast<IMaterialInternal*>( pMaterial );

	bool bMaterialChanged;
	if ( m_pMaterial && pMatInt && m_pMaterial->InMaterialPage() && pMatInt->InMaterialPage() )
	{
		bMaterialChanged = ( m_pMaterial->GetMaterialPage() != pMatInt->GetMaterialPage() );
	}
	else
	{
		bMaterialChanged = ( m_pMaterial != pMatInt ) || ( m_pMaterial && m_pMaterial->InMaterialPage() ) || ( pMatInt && pMatInt->InMaterialPage() );
	}

	if ( bMaterialChanged )
	{
		FlushBufferedPrimitives();
#ifdef RECORDING
		RECORD_DEBUG_STRING( ( char * )pMaterial->GetName() );
		IShader *pShader = pMatInt->GetShader();
		if( pShader && pShader->GetName() )
		{
			RECORD_DEBUG_STRING( pShader->GetName() );
		}
		else
		{
			RECORD_DEBUG_STRING( "<NULL SHADER>" );
		}
#endif
		m_pMaterial = pMatInt;

#if ( defined( PIX_INSTRUMENTATION ) || defined( NVPERFHUD ) )
		PIXifyName( s_pPIXMaterialName, sizeof( s_pPIXMaterialName ), m_pMaterial->GetName() );
#endif
	}
}

// Get the currently bound material
IMaterialInternal* CShaderAPIDx8::GetBoundMaterial()
{
	return m_pMaterial;
}


//-----------------------------------------------------------------------------
// Binds a standard texture
//-----------------------------------------------------------------------------
void CShaderAPIDx8::BindStandardTexture( Sampler_t sampler, StandardTextureId_t id )
{
	if ( m_StdTextureHandles[id] != INVALID_SHADERAPI_TEXTURE_HANDLE )
	{
		BindTexture( sampler, m_StdTextureHandles[id] );
	}
	else
	{
		ShaderUtil()->BindStandardTexture( sampler, id );
	}
}

void CShaderAPIDx8::BindStandardVertexTexture( VertexTextureSampler_t sampler, StandardTextureId_t id )
{
	ShaderUtil()->BindStandardVertexTexture( sampler, id );
}

void CShaderAPIDx8::GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id )
{
	ShaderUtil()->GetStandardTextureDimensions( pWidth, pHeight, id );
}


//-----------------------------------------------------------------------------
// Gets the lightmap dimensions
//-----------------------------------------------------------------------------
void CShaderAPIDx8::GetLightmapDimensions( int *w, int *h )
{
	ShaderUtil()->GetLightmapDimensions( w, h );
}


//-----------------------------------------------------------------------------
// Selection mode methods
//-----------------------------------------------------------------------------
int CShaderAPIDx8::SelectionMode( bool selectionMode )
{
	LOCK_SHADERAPI();
	int numHits = m_NumHits;
	if (m_InSelectionMode)
	{
		WriteHitRecord();
	}
	m_InSelectionMode = selectionMode;
	m_pCurrSelectionRecord = m_pSelectionBuffer;
	m_NumHits = 0;
	return numHits;
}

bool CShaderAPIDx8::IsInSelectionMode() const
{
	return m_InSelectionMode;
}

void CShaderAPIDx8::SelectionBuffer( unsigned int* pBuffer, int size )
{
	LOCK_SHADERAPI();
	Assert( !m_InSelectionMode );
	Assert( pBuffer && size );
	m_pSelectionBufferEnd = pBuffer + size;
	m_pSelectionBuffer = pBuffer;
	m_pCurrSelectionRecord = pBuffer;
}

void CShaderAPIDx8::ClearSelectionNames( )
{
	LOCK_SHADERAPI();
	if (m_InSelectionMode)
	{
		WriteHitRecord();
	}
	m_SelectionNames.Clear();
}

void CShaderAPIDx8::LoadSelectionName( int name )
{
	LOCK_SHADERAPI();
	if (m_InSelectionMode)
	{
		WriteHitRecord();
		Assert( m_SelectionNames.Count() > 0 );
		m_SelectionNames.Top() = name;
	}
}

void CShaderAPIDx8::PushSelectionName( int name )
{
	LOCK_SHADERAPI();
	if (m_InSelectionMode)
	{
		WriteHitRecord();
		m_SelectionNames.Push(name);
	}
}

void CShaderAPIDx8::PopSelectionName()
{
	LOCK_SHADERAPI();
	if (m_InSelectionMode)
	{
		WriteHitRecord();
		m_SelectionNames.Pop();
	}
}

void CShaderAPIDx8::WriteHitRecord( )
{
	FlushBufferedPrimitives();

	if (m_SelectionNames.Count() && (m_SelectionMinZ != FLT_MAX))
	{
		Assert( m_pCurrSelectionRecord + m_SelectionNames.Count() + 3 <	m_pSelectionBufferEnd );
		*m_pCurrSelectionRecord++ = m_SelectionNames.Count();
		// NOTE: because of rounding, "(uint32)(float)UINT32_MAX" yields zero(!), hence the use of doubles.
		// [ ALSO: As of Nov 2011, VS2010 exhibits a debug build code-gen bug if we cast the result to int32 instead of uint32 ]
	    *m_pCurrSelectionRecord++ = (uint32)( 0.5 + m_SelectionMinZ*(double)((uint32)~0) );
	    *m_pCurrSelectionRecord++ = (uint32)( 0.5 + m_SelectionMaxZ*(double)((uint32)~0) );
		for (int i = 0; i < m_SelectionNames.Count(); ++i)
		{
			*m_pCurrSelectionRecord++ = m_SelectionNames[i];
		}

		++m_NumHits;
	}

	m_SelectionMinZ = FLT_MAX;
	m_SelectionMaxZ = FLT_MIN;
}

// We hit somefin in selection mode
void CShaderAPIDx8::RegisterSelectionHit( float minz, float maxz )
{
	if (minz < 0)
		minz = 0;
	if (maxz > 1)
		maxz = 1;
	if (m_SelectionMinZ > minz)
		m_SelectionMinZ = minz;
	if (m_SelectionMaxZ < maxz)
		m_SelectionMaxZ = maxz;
}

int CShaderAPIDx8::GetCurrentNumBones( void ) const
{
	return m_DynamicState.m_NumBones;
}

bool CShaderAPIDx8::IsHWMorphingEnabled( ) const
{
	return m_DynamicState.m_bHWMorphingEnabled;
}


//-----------------------------------------------------------------------------
// Inserts the lighting block into the code
//-----------------------------------------------------------------------------

// If you change the number of lighting combinations, change this enum
enum
{
	DX8_LIGHTING_COMBINATION_COUNT = 22,
	DX9_LIGHTING_COMBINATION_COUNT = 35
};

#define MAX_LIGHTS 4


// NOTE: These should match g_lightType* in vsh_prep.pl!
static int g_DX8LightCombinations[][4] = 
{
	// static		ambient				local1				local2

	// This is a special case for no lighting at all.
	{ LIGHT_NONE,	LIGHT_NONE,			LIGHT_NONE,			LIGHT_NONE },			

	// This is a special case so that we don't have to do the ambient cube
	// when we only have static lighting
	{ LIGHT_STATIC,	LIGHT_NONE,			LIGHT_NONE,			LIGHT_NONE },			

	{ LIGHT_NONE,	LIGHT_AMBIENTCUBE,	LIGHT_NONE,			LIGHT_NONE },			
	{ LIGHT_NONE,	LIGHT_AMBIENTCUBE,	LIGHT_SPOT,			LIGHT_NONE },
	{ LIGHT_NONE,	LIGHT_AMBIENTCUBE,	LIGHT_POINT,		LIGHT_NONE },
	{ LIGHT_NONE,	LIGHT_AMBIENTCUBE,	LIGHT_DIRECTIONAL,	LIGHT_NONE },
	{ LIGHT_NONE,	LIGHT_AMBIENTCUBE,	LIGHT_SPOT,			LIGHT_SPOT },
	{ LIGHT_NONE,	LIGHT_AMBIENTCUBE,	LIGHT_SPOT,			LIGHT_POINT, },			
	{ LIGHT_NONE,	LIGHT_AMBIENTCUBE,	LIGHT_SPOT,			LIGHT_DIRECTIONAL, },
	{ LIGHT_NONE,	LIGHT_AMBIENTCUBE,	LIGHT_POINT,		LIGHT_POINT, },
	{ LIGHT_NONE,	LIGHT_AMBIENTCUBE,	LIGHT_POINT,		LIGHT_DIRECTIONAL, },
	{ LIGHT_NONE,	LIGHT_AMBIENTCUBE,	LIGHT_DIRECTIONAL,	LIGHT_DIRECTIONAL, },

	{ LIGHT_STATIC,	LIGHT_AMBIENTCUBE,	LIGHT_NONE,			LIGHT_NONE },			
	{ LIGHT_STATIC,	LIGHT_AMBIENTCUBE,	LIGHT_SPOT,			LIGHT_NONE },
	{ LIGHT_STATIC,	LIGHT_AMBIENTCUBE,	LIGHT_POINT,		LIGHT_NONE },
	{ LIGHT_STATIC,	LIGHT_AMBIENTCUBE,	LIGHT_DIRECTIONAL,	LIGHT_NONE },
	{ LIGHT_STATIC,	LIGHT_AMBIENTCUBE,	LIGHT_SPOT,			LIGHT_SPOT },
	{ LIGHT_STATIC,	LIGHT_AMBIENTCUBE,	LIGHT_SPOT,			LIGHT_POINT, },			
	{ LIGHT_STATIC,	LIGHT_AMBIENTCUBE,	LIGHT_SPOT,			LIGHT_DIRECTIONAL, },
	{ LIGHT_STATIC,	LIGHT_AMBIENTCUBE,	LIGHT_POINT,		LIGHT_POINT, },
	{ LIGHT_STATIC,	LIGHT_AMBIENTCUBE,	LIGHT_POINT,		LIGHT_DIRECTIONAL, },
	{ LIGHT_STATIC,	LIGHT_AMBIENTCUBE,	LIGHT_DIRECTIONAL,	LIGHT_DIRECTIONAL, }
};

// This is just for getting light combos for DX8
// For DX9, use GetDX9LightState()
// It is up to the shader cpp files to use the right method
int CShaderAPIDx8::GetCurrentLightCombo( void ) const
{
	Assert( g_pHardwareConfig->Caps().m_nDXSupportLevel <= 81 );

	Assert( m_DynamicState.m_NumLights <= 2 );

	COMPILE_TIME_ASSERT( DX8_LIGHTING_COMBINATION_COUNT == 
		sizeof( g_DX8LightCombinations ) / sizeof( g_DX8LightCombinations[0] ) );
	
	// hack . . do this a cheaper way.
	bool bUseAmbientCube;
	if( m_DynamicState.m_AmbientLightCube[0][0] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[0][1] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[0][2] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[1][0] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[1][1] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[1][2] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[2][0] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[2][1] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[2][2] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[3][0] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[3][1] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[3][2] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[4][0] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[4][1] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[4][2] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[5][0] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[5][1] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[5][2] == 0.0f )
	{
		bUseAmbientCube = false;
	}
	else
	{
		bUseAmbientCube = true;
	}

	Assert( m_pRenderMesh );

	const VertexShaderLightTypes_t *pLightType = m_DynamicState.m_LightType;

	if( m_DynamicState.m_NumLights == 0 && !bUseAmbientCube )
	{
		if( m_pRenderMesh->HasColorMesh() )
			return 1;		// special case for static lighting only
		else
			return 0;		// special case for no lighting at all.
	}
	
	int i;
	// hack - skip the first two for now since we don't know if the ambient cube is needed or not.
	// dimhotepus: Dx9 -> Dx8 to fix buffer overlow.
	for( i = 2; i < DX8_LIGHTING_COMBINATION_COUNT; ++i )
	{
		int j;
		for( j = 0; j < m_DynamicState.m_NumLights; ++j )
		{
			if( pLightType[j] != g_DX8LightCombinations[i][j+2] )
				break;
		}
		if( j == m_DynamicState.m_NumLights )
		{
			while( j < 2 )
			{
				if (g_DX8LightCombinations[i][j+2] != LIGHT_NONE)
					break;
				++j;
			}
			if( j == 2 )
			{
				if( m_pRenderMesh->HasColorMesh() )
				{
					return i + 10;
				}
				else
				{
					return i;
				}
			}
		}
	}

	// should never get here!
	Assert( 0 );
	return 0;
}

void CShaderAPIDx8::GetDX9LightState( LightState_t *state ) const
{
	// hack . . do this a cheaper way.
	if( m_DynamicState.m_AmbientLightCube[0][0] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[0][1] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[0][2] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[1][0] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[1][1] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[1][2] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[2][0] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[2][1] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[2][2] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[3][0] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[3][1] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[3][2] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[4][0] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[4][1] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[4][2] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[5][0] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[5][1] == 0.0f && 
		m_DynamicState.m_AmbientLightCube[5][2] == 0.0f )
	{
		state->m_bAmbientLight = false;
	}
	else
	{
		state->m_bAmbientLight = true;
	}

	Assert( m_pRenderMesh );
	Assert( m_DynamicState.m_NumLights <= 4 );

	if ( g_pHardwareConfig->SupportsPixelShaders_2_b() )
	{
		Assert( m_DynamicState.m_NumLights <= MAX_LIGHTS );		// 2b hardware gets four lights
	}
	else
	{
		Assert( m_DynamicState.m_NumLights <= (MAX_LIGHTS-2) );	// 2.0 hardware gets two less
	}

#ifdef OSX
	state->m_nNumLights = MIN(MAX_NUM_LIGHTS,m_DynamicState.m_NumLights);
#else
	state->m_nNumLights = m_DynamicState.m_NumLights;
#endif	
	
	state->m_nNumLights = m_DynamicState.m_NumLights;
	state->m_bStaticLightVertex = m_pRenderMesh->HasColorMesh();
	state->m_bStaticLightTexel = false; // For now
}

MaterialFogMode_t CShaderAPIDx8::GetCurrentFogType( void ) const
{
	return m_DynamicState.m_SceneFog;
}

void CShaderAPIDx8::RecordString( [[maybe_unused]] const char *pStr )
{
	RECORD_STRING( pStr );
}

void CShaderAPIDx8::EvictManagedResourcesInternal()
{
	if ( !ThreadOwnsDevice() || !ThreadInMainThread() )
	{
		ShaderUtil()->OnThreadEvent( SHADER_THREAD_EVICT_RESOURCES );
		return;
	}

	if ( mat_debugalttab.GetBool() )
	{
		Warning( "mat_debugalttab: CShaderAPIDx8::EvictManagedResourcesInternal\n" );
	}

	if ( auto *device = Dx9Device(); device )
	{
		device->EvictManagedResources();
	}
}

void CShaderAPIDx8::EvictManagedResources( void )
{
	LOCK_SHADERAPI();
	Assert(ThreadOwnsDevice());
	// Tell other material system applications to release resources
	SendIPCMessage( EVICT_MESSAGE );
	EvictManagedResourcesInternal();
}

bool CShaderAPIDx8::IsDebugTextureListFresh( int numFramesAllowed /* = 1 */ )
{
	return ( m_nDebugDataExportFrame <= m_CurrentFrame ) && ( m_nDebugDataExportFrame >= m_CurrentFrame - numFramesAllowed );
}

bool CShaderAPIDx8::SetDebugTextureRendering( bool bEnable )
{
	bool bVal = m_bDebugTexturesRendering;
	m_bDebugTexturesRendering = bEnable;
	return bVal;
}

void CShaderAPIDx8::EnableDebugTextureList( bool bEnable )
{
	m_bEnableDebugTextureList = bEnable;
}

void CShaderAPIDx8::EnableGetAllTextures( bool bEnable )
{
	m_bDebugGetAllTextures = bEnable;
}

KeyValues* CShaderAPIDx8::GetDebugTextureList()
{
	return m_pDebugTextureList;
}

int CShaderAPIDx8::GetTextureMemoryUsed( TextureMemoryType eTextureMemory )
{
	switch ( eTextureMemory )
	{
	case MEMORY_BOUND_LAST_FRAME:
		return m_nTextureMemoryUsedLastFrame;
	case MEMORY_TOTAL_LOADED:
		return m_nTextureMemoryUsedTotal;
	case MEMORY_ESTIMATE_PICMIP_1:
		return m_nTextureMemoryUsedPicMip1;
	case MEMORY_ESTIMATE_PICMIP_2:
		return m_nTextureMemoryUsedPicMip2;
	default:
		return 0;
	}
}


// Allocate and delete query objects.
ShaderAPIOcclusionQuery_t CShaderAPIDx8::CreateOcclusionQueryObject( void )
{
	// don't allow this on <80 because it falls back to wireframe in that case
	if( m_DeviceSupportsCreateQuery == 0 || g_pHardwareConfig->Caps().m_nDXSupportLevel < 80 )
		return INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE;

	// While we're deactivated, m_OcclusionQueryObjects just holds NULL pointers.
	// Create a dummy one here and let ReacquireResources create the actual D3D object.
	if ( IsDeactivated() )
		return INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE;

	IDirect3DQuery9 *pQuery = NULL;
	HRESULT hr = Dx9Device()->CreateQuery( D3DQUERYTYPE_OCCLUSION, &pQuery );
	return ( SUCCEEDED( hr ) ) ? (ShaderAPIOcclusionQuery_t)pQuery : INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE;
}

void CShaderAPIDx8::DestroyOcclusionQueryObject( ShaderAPIOcclusionQuery_t handle )
{
	IDirect3DQuery9 *pQuery = (IDirect3DQuery9 *)handle;

	ULONG nRetVal = pQuery->Release();
	Assert( nRetVal == 0 );
}

// Bracket drawing with begin and end so that we can get counts next frame.
void CShaderAPIDx8::BeginOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t handle )
{
	IDirect3DQuery9 *pQuery = (IDirect3DQuery9 *)handle;

	HRESULT hr = pQuery->Issue( D3DISSUE_BEGIN );
	Assert( SUCCEEDED( hr ) );
}

void CShaderAPIDx8::EndOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t handle )
{
	IDirect3DQuery9 *pQuery = (IDirect3DQuery9 *)handle;

	HRESULT hr = pQuery->Issue( D3DISSUE_END );
	Assert( SUCCEEDED( hr ) );
}

// Get the number of pixels rendered between begin and end on an earlier frame.
// Calling this in the same frame is a huge perf hit!
int CShaderAPIDx8::OcclusionQuery_GetNumPixelsRendered( ShaderAPIOcclusionQuery_t handle, bool bFlush )
{
	LOCK_SHADERAPI();
	IDirect3DQuery9 *pQuery = (IDirect3DQuery9 *)handle;

	tmZone( TELEMETRY_LEVEL1, TMZF_NONE, "%s", __FUNCTION__ );

	DWORD nPixels;
	HRESULT hr = pQuery->GetData( &nPixels, sizeof( nPixels ), bFlush ? D3DGETDATA_FLUSH : 0 );

	// This means that the query will not finish and resulted in an error, game should use
	// the previous query's results and not reissue the query
	if ( ( hr == D3DERR_DEVICELOST ) || ( hr == D3DERR_NOTAVAILABLE ) )
		return OCCLUSION_QUERY_RESULT_ERROR;

	// This means the query isn't finished yet, game will have to use the previous query's
	// results and not reissue the query; wait for query to finish later.
	if ( hr == S_FALSE )
		return OCCLUSION_QUERY_RESULT_PENDING;

	// NOTE: This appears to work around a driver bug for ATI on Vista
	if ( nPixels & 0x80000000 )
	{
		nPixels = 0;
	}
	return nPixels;
}


void CShaderAPIDx8::SetPixelShaderFogParams( int reg, ShaderFogMode_t fogMode )
{
	m_DelayedShaderConstants.iPixelShaderFogParams = reg; //save it off in case the ShaderFogMode_t disables fog. We only find out later.
	float fogParams[4];

	if( (GetPixelFogMode() != MATERIAL_FOG_NONE) && (fogMode != SHADER_FOGMODE_DISABLED) )
	{
		float ooFogRange = 1.0f;

		float fStart = m_VertexShaderFogParams[0];
		float fEnd = m_VertexShaderFogParams[1];

		// Check for divide by zero
		if ( fEnd != fStart )
		{
			ooFogRange = 1.0f / ( fEnd - fStart );
		}

		fogParams[0] = fStart * ooFogRange;		// fogStart / ( fogEnd - fogStart )
		fogParams[1] = m_DynamicState.m_FogZ;	// water height
		fogParams[2] = clamp( m_flFogMaxDensity, 0.0f, 1.0f ); // Max fog density
		fogParams[3] = ooFogRange;				// 1 / ( fogEnd - fogStart );

		if (GetPixelFogMode() == MATERIAL_FOG_LINEAR_BELOW_FOG_Z)
		{
			// terms are unused for height fog, forcing 1.0 allows unified PS math
			fogParams[0] = 0.0f;
			fogParams[2] = 1.0f;
		}
	}
	else
	{
		//emulating MATERIAL_FOG_NONE by setting the parameters so that CalcRangeFog() always returns 0. Gets rid of a dynamic combo across the ps2x set.
		fogParams[0] = 0.0f;
		fogParams[1] = m_DynamicState.m_FogZ; // water height
		fogParams[2] = 1.0f; // Max fog density
		fogParams[3] = 0.0f;
	}

	// cFogEndOverFogRange, cFogOne, unused, cOOFogRange
	SetPixelShaderConstant( reg, fogParams, 1 );
}

void CShaderAPIDx8::SetPixelShaderFogParams( int reg )
{
	SetPixelShaderFogParams( reg, m_TransitionTable.CurrentShadowState()->m_FogMode );
}

void CShaderAPIDx8::SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture )
{
	LOCK_SHADERAPI();
	SetFlashlightStateEx( state, worldToTexture, NULL );
}

void CShaderAPIDx8::SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture )
{
	LOCK_SHADERAPI();
	// fixme: do a test here.
	FlushBufferedPrimitives();
	m_FlashlightState = state;
	m_FlashlightWorldToTexture = worldToTexture;
	m_pFlashlightDepthTexture = pFlashlightDepthTexture;

	if ( !g_pHardwareConfig->SupportsPixelShaders_2_b() )
	{
		m_FlashlightState.m_bEnableShadows = false;
		m_pFlashlightDepthTexture = NULL;
	}
}

const FlashlightState_t &CShaderAPIDx8::GetFlashlightState( VMatrix &worldToTexture ) const
{
	worldToTexture = m_FlashlightWorldToTexture;
	return m_FlashlightState;
}

const FlashlightState_t &CShaderAPIDx8::GetFlashlightStateEx( VMatrix &worldToTexture, ITexture **ppFlashlightDepthTexture ) const
{
	worldToTexture = m_FlashlightWorldToTexture;
	*ppFlashlightDepthTexture = m_pFlashlightDepthTexture;
	return m_FlashlightState;
}

bool CShaderAPIDx8::SupportsMSAAMode( int nMSAAMode )
{
	HRESULT hr = D3D()->CheckDeviceMultiSampleType( m_DisplayAdapter, m_DeviceType, 
													m_PresentParameters.BackBufferFormat,
													m_PresentParameters.Windowed,
													ComputeMultisampleType( nMSAAMode ), NULL );
	return SUCCEEDED( hr );
}

bool CShaderAPIDx8::SupportsCSAAMode( int nNumSamples, int nQualityLevel )
{
#ifdef DX_TO_GL_ABSTRACTION
	// GL_NV_framebuffer_multisample_coverage
	return false;
#endif

	// Only nVidia does this kind of AA
	if ( g_pHardwareConfig->Caps().m_VendorID != VENDORID_NVIDIA )
		return false;

	DWORD dwQualityLevels = 0;
	HRESULT hr = D3D()->CheckDeviceMultiSampleType( m_DisplayAdapter, m_DeviceType, 
													 m_PresentParameters.BackBufferFormat,
													 m_PresentParameters.Windowed,
													 ComputeMultisampleType( nNumSamples ), &dwQualityLevels );

	return SUCCEEDED( hr ) && (int)dwQualityLevels >= nQualityLevel;
}

bool CShaderAPIDx8::SupportsShadowDepthTextures( void )
{
	return g_pHardwareConfig->Caps().m_bSupportsShadowDepthTextures;
}

bool CShaderAPIDx8::SupportsBorderColor( void ) const
{
	return g_pHardwareConfig->Caps().m_bSupportsBorderColor;
}

bool CShaderAPIDx8::SupportsFetch4( void )
{
	return g_pHardwareConfig->Caps().m_bSupportsFetch4;
}

ImageFormat CShaderAPIDx8::GetShadowDepthTextureFormat( void )
{
	return g_pHardwareConfig->Caps().m_ShadowDepthTextureFormat;
}

ImageFormat CShaderAPIDx8::GetNullTextureFormat( void )
{
	return g_pHardwareConfig->Caps().m_NullTextureFormat;
}

void CShaderAPIDx8::SetShadowDepthBiasFactors( float fShadowSlopeScaleDepthBias, float fShadowDepthBias )
{
	m_fShadowSlopeScaleDepthBias = fShadowSlopeScaleDepthBias;
	m_fShadowDepthBias = fShadowDepthBias;
}

void CShaderAPIDx8::ClearVertexAndPixelShaderRefCounts()
{
	LOCK_SHADERAPI();
	ShaderManager()->ClearVertexAndPixelShaderRefCounts();
}

void CShaderAPIDx8::PurgeUnusedVertexAndPixelShaders()
{
	LOCK_SHADERAPI();
	ShaderManager()->PurgeUnusedVertexAndPixelShaders();
}

bool CShaderAPIDx8::UsingSoftwareVertexProcessing() const
{
	return g_pHardwareConfig->Caps().m_bSoftwareVertexProcessing;
}

ITexture *CShaderAPIDx8::GetRenderTargetEx( int nRenderTargetID )
{
	return ShaderUtil()->GetRenderTargetEx( nRenderTargetID );
}

float CShaderAPIDx8::GetLightMapScaleFactor( void ) const
{
	switch( HardwareConfig()->GetHDRType() )
	{
		case HDR_TYPE_FLOAT:
			return 1.0;

		case HDR_TYPE_INTEGER:
			return 16.0;

		case HDR_TYPE_NONE:
		default:
			return GammaToLinearFullRange( 2.0 );	// light map scale
	}
}

void CShaderAPIDx8::SetToneMappingScaleLinear( const Vector &scale )
{
	if ( g_pHardwareConfig->SupportsPixelShaders_2_0() )
	{
		// Flush buffered primitives before changing the tone map scalar!
		FlushBufferedPrimitives();

		Vector scale_to_use = scale;
		m_ToneMappingScale.AsVector3D() = scale_to_use;
		
		bool mode_uses_srgb=false;

		switch( HardwareConfig()->GetHDRType() )
		{
			case HDR_TYPE_NONE:
				m_ToneMappingScale.x = 1.0;										// output scale
				m_ToneMappingScale.z = 1.0;										// reflection map scale
				break;

			case HDR_TYPE_FLOAT:
				m_ToneMappingScale.x = scale_to_use.x;							// output scale
				m_ToneMappingScale.z = 1.0;										// reflection map scale
				break;

			case HDR_TYPE_INTEGER:
				mode_uses_srgb = true;
				m_ToneMappingScale.x = scale_to_use.x;							// output scale
				m_ToneMappingScale.z = 16.0;									// reflection map scale
				break;
		}

		m_ToneMappingScale.y = GetLightMapScaleFactor();	// light map scale

		// w component gets gamma scale
		m_ToneMappingScale.w = LinearToGammaFullRange( m_ToneMappingScale.x );
		SetPixelShaderConstant( TONE_MAPPING_SCALE_PSH_CONSTANT, m_ToneMappingScale.Base() );

		// We have to change the fog color since we tone map directly in the shaders in integer HDR mode.
		if ( HardwareConfig()->GetHDRType() == HDR_TYPE_INTEGER && m_TransitionTable.CurrentShadowState() )
		{
			// Get the shadow state in sync since it depends on SetToneMappingScaleLinear.
			ApplyFogMode( m_TransitionTable.CurrentShadowState()->m_FogMode, mode_uses_srgb, m_TransitionTable.CurrentShadowState()->m_bDisableFogGammaCorrection );
		}
	}
}

const Vector & CShaderAPIDx8::GetToneMappingScaleLinear( void ) const
{
	return m_ToneMappingScale.AsVector3D();
}

void CShaderAPIDx8::EnableLinearColorSpaceFrameBuffer( bool bEnable )
{
	LOCK_SHADERAPI();
	m_TransitionTable.EnableLinearColorSpaceFrameBuffer( bEnable );
}


void CShaderAPIDx8::SetPSNearAndFarZ( int pshReg )
{
	VMatrix m;
	GetMatrix( MATERIAL_PROJECTION, m.m[0] );

	// m[2][2] =  F/(N-F)   (flip sign if RH)
	// m[3][2] = NF/(N-F)

	float vNearFar[4];

	float N =     m[3][2] / m[2][2];
	float F = (m[3][2]*N) / (N + m[3][2]);

	vNearFar[0] = N;
	vNearFar[1] = F;


	SetPixelShaderConstant( pshReg, vNearFar, 1 );
}


void CShaderAPIDx8::SetFloatRenderingParameter( int parm_number, float value )
{
	LOCK_SHADERAPI();
	if ( parm_number < ssize( FloatRenderingParameters ))
		FloatRenderingParameters[parm_number] = value;
}

void CShaderAPIDx8::SetIntRenderingParameter( int parm_number, int value )
{
	LOCK_SHADERAPI();
	if ( parm_number < ssize( IntRenderingParameters ))
		IntRenderingParameters[parm_number] = value;
}

void CShaderAPIDx8::SetVectorRenderingParameter( int parm_number, Vector const & value )
{
	LOCK_SHADERAPI();
	if ( parm_number < ssize( VectorRenderingParameters ))
		VectorRenderingParameters[parm_number] = value;
}

float CShaderAPIDx8::GetFloatRenderingParameter( int parm_number ) const
{
	LOCK_SHADERAPI();
	if ( parm_number < ssize( FloatRenderingParameters ))
		return FloatRenderingParameters[parm_number];
	else
		return 0.0;
}

int CShaderAPIDx8::GetIntRenderingParameter( int parm_number ) const
{
	LOCK_SHADERAPI();
	if ( parm_number < ssize( IntRenderingParameters ))
		return IntRenderingParameters[parm_number];
	else
		return 0;
}

Vector CShaderAPIDx8::GetVectorRenderingParameter( int parm_number ) const
{
	LOCK_SHADERAPI();
	if ( parm_number < ssize( VectorRenderingParameters ))
		return VectorRenderingParameters[parm_number];
	else
		return Vector( 0, 0, 0 );
}

// stencil entry points
void CShaderAPIDx8::SetStencilEnable( bool onoff )
{
	LOCK_SHADERAPI();
	SetRenderState( D3DRS_STENCILENABLE, onoff?TRUE:FALSE, true );
}

void CShaderAPIDx8::SetStencilFailOperation( StencilOperation_t op )
{
	LOCK_SHADERAPI();
	SetRenderState( D3DRS_STENCILFAIL, op, true );
}

void CShaderAPIDx8::SetStencilZFailOperation( StencilOperation_t op )
{
	LOCK_SHADERAPI();
	SetRenderState( D3DRS_STENCILZFAIL, op, true );
}

void CShaderAPIDx8::SetStencilPassOperation( StencilOperation_t op )
{
	LOCK_SHADERAPI();
	SetRenderState( D3DRS_STENCILPASS, op, true );
}

void CShaderAPIDx8::SetStencilCompareFunction( StencilComparisonFunction_t cmpfn )
{
	LOCK_SHADERAPI();
	SetRenderState( D3DRS_STENCILFUNC, cmpfn, true );
}

void CShaderAPIDx8::SetStencilReferenceValue( int ref )
{
	LOCK_SHADERAPI();
	SetRenderState( D3DRS_STENCILREF, ref, true );
}

void CShaderAPIDx8::SetStencilTestMask( uint32 msk )
{
	LOCK_SHADERAPI();
	SetRenderState( D3DRS_STENCILMASK, msk, true );
}

void CShaderAPIDx8::SetStencilWriteMask( uint32 msk )
{
	LOCK_SHADERAPI();
	SetRenderState( D3DRS_STENCILWRITEMASK, msk, true );
}

void CShaderAPIDx8::ClearStencilBufferRectangle(
	int xmin, int ymin, int xmax, int ymax,int value)
{
	LOCK_SHADERAPI();
	D3DRECT clear;
	clear.x1 = xmin;
	clear.y1 = ymin;
	clear.x2 = xmax;
	clear.y2 = ymax;

	Dx9Device()->Clear( 
		1, &clear, D3DCLEAR_STENCIL, 0, 0, value );	
}

int CShaderAPIDx8::CompareSnapshots( StateSnapshot_t snapshot0, StateSnapshot_t snapshot1 )
{
	LOCK_SHADERAPI();
	// dimhotepus: Comment unused shadows.
	//const ShadowState_t &shadow0 = m_TransitionTable.GetSnapshot(snapshot0);
	//const ShadowState_t &shadow1 = m_TransitionTable.GetSnapshot(snapshot1);
	const ShadowShaderState_t &shader0 = m_TransitionTable.GetSnapshotShader(snapshot0);
	const ShadowShaderState_t &shader1 = m_TransitionTable.GetSnapshotShader(snapshot1);

	int dVertex = shader0.m_VertexShader - shader1.m_VertexShader;
	if ( dVertex )
		return dVertex;
	int dVCombo = shader0.m_nStaticVshIndex - shader1.m_nStaticVshIndex;
	if ( dVCombo)
		return dVCombo;

	int dPixel = shader0.m_PixelShader - shader1.m_PixelShader;
	if ( dPixel )
		return dPixel;
	int dPCombo = shader0.m_nStaticPshIndex - shader1.m_nStaticPshIndex;
	if ( dPCombo)
		return dPCombo;

	return snapshot0 - snapshot1;
}

// ------------ New Vertex/Index Buffer interface ----------------------------

void CShaderAPIDx8::BindVertexBuffer( int streamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions )
{
	LOCK_SHADERAPI();
	MeshMgr()->BindVertexBuffer( streamID, pVertexBuffer, nOffsetInBytes, nFirstVertex, nVertexCount, fmt, nRepetitions );
}

void CShaderAPIDx8::BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes )
{
	LOCK_SHADERAPI();
	MeshMgr()->BindIndexBuffer( pIndexBuffer, nOffsetInBytes );
}

void CShaderAPIDx8::Draw( MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount )
{
	LOCK_SHADERAPI();
	MeshMgr()->Draw( primitiveType, nFirstIndex, nIndexCount );
}

// ------------ End ----------------------------

float CShaderAPIDx8::GammaToLinear_HardwareSpecific( float fGamma ) const
{
		return SrgbGammaToLinear( fGamma );
	}

float CShaderAPIDx8::LinearToGamma_HardwareSpecific( float fLinear ) const
{
		return SrgbLinearToGamma( fLinear );
	}


bool CShaderAPIDx8::ShouldWriteDepthToDestAlpha( void ) const
{
	return g_pHardwareConfig->SupportsPixelShaders_2_b() &&
			(m_SceneFogMode != MATERIAL_FOG_LINEAR_BELOW_FOG_Z) && 
			(GetIntRenderingParameter(INT_RENDERPARM_WRITE_DEPTH_TO_DESTALPHA) != 0);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CShaderAPIDx8::AcquireThreadOwnership()
{
	SetCurrentThreadAsOwner();
#if defined( DX_TO_GL_ABSTRACTION )
	Dx9Device()->AcquireThreadOwnership();
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CShaderAPIDx8::ReleaseThreadOwnership()
{
	RemoveThreadOwner();
#if defined( DX_TO_GL_ABSTRACTION )
	Dx9Device()->ReleaseThreadOwnership();
#endif
}

//-----------------------------------------------------------------------------
// debug logging
//-----------------------------------------------------------------------------
void CShaderAPIDx8::PrintfVA( char *fmt, va_list vargs )
{
#ifdef DX_TO_GL_ABSTRACTION
	#if GLMDEBUG
		GLMPrintfVA( fmt, vargs );
	#endif
#else
	AssertMsg( false, "Not implemented in native mode." );
#endif
}

void CShaderAPIDx8::Printf( PRINTF_FORMAT_STRING const char *fmt, ... )
{
#ifdef DX_TO_GL_ABSTRACTION
	#if GLMDEBUG
		va_list	vargs;

		va_start(vargs, fmt);

		GLMPrintfVA( fmt, vargs );

		va_end( vargs );
	#endif
#else
	AssertMsg( false, "Not implemented in native mode." );
#endif
}

float CShaderAPIDx8::Knob( char *knobname, float *setvalue )
{
#ifdef DX_TO_GL_ABSTRACTION
	#if GLMDEBUG
		return GLMKnob( knobname, setvalue );
	#else
		return 0.0f;
	#endif
#else
	return 0.0f;
#endif
}
