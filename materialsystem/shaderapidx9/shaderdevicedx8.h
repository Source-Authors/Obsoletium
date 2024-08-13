//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef SHADERDEVICEDX8_H
#define SHADERDEVICEDX8_H

#ifdef _WIN32
#pragma once
#endif


#include "shaderdevicebase.h"
#include "shaderapidx8_global.h"
#include "tier1/utlvector.h"



//-----------------------------------------------------------------------------
// Describes which D3DDEVTYPE to use
//-----------------------------------------------------------------------------
#ifndef USE_REFERENCE_RASTERIZER
#define DX8_DEVTYPE	D3DDEVTYPE_HAL
#else
#define DX8_DEVTYPE	D3DDEVTYPE_REF
#endif
    

// PC:    By default, PIX profiling is explicitly disallowed using the D3DPERF_SetOptions(1) API on PC
// X360:  PIX_INSTRUMENTATION will only generate PIX events in RELEASE builds on 360
// Uncomment to use PIX instrumentation:
#if PIX_ENABLE
#define	PIX_INSTRUMENTATION
#endif

#define MAX_PIX_ERRORS		3

#if defined( PIX_INSTRUMENTATION ) && defined ( DX_TO_GL_ABSTRACTION ) && defined( _WIN32 )
typedef int (WINAPI *D3DPERF_BeginEvent_FuncPtr)( D3DCOLOR col, LPCWSTR wszName );
typedef int (WINAPI *D3DPERF_EndEvent_FuncPtr)( void );
typedef void (WINAPI *D3DPERF_SetMarker_FuncPtr)( D3DCOLOR col, LPCWSTR wszName );
typedef void (WINAPI *D3DPERF_SetOptions_FuncPtr)( DWORD dwOptions );
#endif

//-----------------------------------------------------------------------------
// The Base implementation of the shader device
//-----------------------------------------------------------------------------
class CShaderDeviceMgrDx8 : public CShaderDeviceMgrBase
{
	typedef CShaderDeviceMgrBase BaseClass;

public:
	// constructor, destructor
	CShaderDeviceMgrDx8();
	virtual ~CShaderDeviceMgrDx8();

	// Methods of IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Methods of IShaderDevice
	virtual unsigned	 GetAdapterCount() const;
	virtual void GetAdapterInfo( unsigned adapter, MaterialAdapterInfo_t& info ) const;
	virtual unsigned	 GetModeCount( unsigned nAdapter ) const;
	virtual void GetModeInfo( ShaderDisplayMode_t* pInfo, unsigned nAdapter, unsigned mode ) const;
	virtual void GetCurrentModeInfo( ShaderDisplayMode_t* pInfo, unsigned nAdapter ) const;
	virtual bool SetAdapter( unsigned nAdapter, int nFlags );
	virtual CreateInterfaceFn SetMode( void *hWnd, unsigned nAdapter, const ShaderDeviceInfo_t& mode );

	// Determines hardware caps from D3D
	bool ComputeCapsFromD3D( HardwareCaps_t *pCaps, unsigned nAdapter );

	// Forces caps to a specific dx level
	void ForceCapsToDXLevel( HardwareCaps_t *pCaps, int nDxLevel, const HardwareCaps_t &actualCaps );

	// Validates the mode...
	bool ValidateMode( unsigned nAdapter, const ShaderDeviceInfo_t &info ) const;

	// Returns the amount of video memory in bytes for a particular adapter
	virtual size_t GetVidMemBytes( unsigned nAdapter ) const;

#if !defined( _X360 )
	FORCEINLINE IDirect3D9 *D3D() const
	{ 
		return m_pD3D; 
	}
#endif

#if defined( PIX_INSTRUMENTATION ) && defined ( DX_TO_GL_ABSTRACTION ) && defined( _WIN32 )
	HMODULE m_hD3D9;
	D3DPERF_BeginEvent_FuncPtr m_pBeginEvent;
	D3DPERF_EndEvent_FuncPtr m_pEndEvent;
	D3DPERF_SetMarker_FuncPtr m_pSetMarker;
	D3DPERF_SetOptions_FuncPtr m_pSetOptions;
#endif

protected:
	// Determine capabilities
	bool DetermineHardwareCaps( );

private:
	// Initialize adapter information
	void InitAdapterInfo();

	// Code to detect support for texture border mode (not a simple caps check)
	void CheckBorderColorSupport( HardwareCaps_t *pCaps, unsigned nAdapter );

	// Vendor-dependent code to detect support for various flavors of shadow mapping
	void CheckVendorDependentShadowMappingSupport( HardwareCaps_t *pCaps, unsigned nAdapter );

	// Vendor-dependent code to detect Alpha To Coverage Backdoors
	void CheckVendorDependentAlphaToCoverage( HardwareCaps_t *pCaps, unsigned nAdapter );

	// Compute the effective DX support level based on all the other caps
	void ComputeDXSupportLevel( HardwareCaps_t &caps );

	// Used to enumerate adapters, attach to windows
#if !defined( _X360 )
	IDirect3D9 *m_pD3D;
#endif

	bool m_bObeyDxCommandlineOverride : 1;
	bool m_bAdapterInfoIntialized : 1;
};

extern CShaderDeviceMgrDx8* g_pShaderDeviceMgrDx8;


//-----------------------------------------------------------------------------
// IDirect3D accessor
//-----------------------------------------------------------------------------
#if defined( _X360 )

extern IDirect3D9 *m_pD3D;
inline IDirect3D9* D3D()  
{
	return m_pD3D;
}

#else

inline IDirect3D9* D3D()  
{
	return g_pShaderDeviceMgrDx8->D3D();
}

#endif

#define NUM_FRAME_SYNC_QUERIES 2
#define NUM_FRAME_SYNC_FRAMES_LATENCY 0

//-----------------------------------------------------------------------------
// The Dx8implementation of the shader device
//-----------------------------------------------------------------------------
class CShaderDeviceDx8 : public CShaderDeviceBase
{
	// Methods of IShaderDevice
public:
	bool IsUsingGraphics() const override;
	ImageFormat GetBackBufferFormat() const override;
	void GetBackBufferDimensions( int& width, int& height ) const override;
	void Present() override;
	IShaderBuffer* CompileShader( const char *pProgram, size_t nBufLen, const char *pShaderVersion ) override;
	VertexShaderHandle_t CreateVertexShader( IShaderBuffer *pBuffer ) override;
	void DestroyVertexShader( VertexShaderHandle_t hShader ) override;
	GeometryShaderHandle_t CreateGeometryShader( IShaderBuffer* pShaderBuffer ) override;
	void DestroyGeometryShader( GeometryShaderHandle_t hShader ) override;
	PixelShaderHandle_t CreatePixelShader( IShaderBuffer* pShaderBuffer ) override;
	void DestroyPixelShader( PixelShaderHandle_t hShader ) override;
	void ReleaseResources() override;
	void ReacquireResources() override;
	IMesh* CreateStaticMesh( VertexFormat_t format, const char *pBudgetGroup, IMaterial * pMaterial = NULL ) override;
	void DestroyStaticMesh( IMesh* mesh ) override;
	IVertexBuffer *CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pBudgetGroup ) override;
	void DestroyVertexBuffer( IVertexBuffer *pVertexBuffer ) override;
	IIndexBuffer *CreateIndexBuffer( ShaderBufferType_t bufferType, MaterialIndexFormat_t fmt, int nIndexCount, const char *pBudgetGroup ) override;
	void DestroyIndexBuffer( IIndexBuffer *pIndexBuffer ) override;
	IVertexBuffer *GetDynamicVertexBuffer( int nStreamID, VertexFormat_t vertexFormat, bool bBuffered = true ) override;
	IIndexBuffer *GetDynamicIndexBuffer( MaterialIndexFormat_t fmt, bool bBuffered = true ) override;
	void SetHardwareGammaRamp( float fGamma, float fGammaTVRangeMin, float fGammaTVRangeMax, float fGammaTVExponent, bool bTVEnabled ) override;
	void SpewDriverInfo() const override;
	unsigned GetCurrentAdapter() const override;
	void EnableNonInteractiveMode( MaterialNonInteractiveMode_t mode, ShaderNonInteractiveInfo_t *pInfo = NULL ) override;
	void RefreshFrontBufferNonInteractive() override;
	const char *GetDisplayDeviceName() override; 

	// Alternative method for ib/vs
	// NOTE: If this works, remove GetDynamicVertexBuffer/IndexBuffer

	// Methods of CShaderDeviceBase
public:
	bool InitDevice( void* hWnd, unsigned nAdapter, const ShaderDeviceInfo_t &info ) override;
	void ShutdownDevice() override;
	bool IsDeactivated() const override;

	// Other public methods
public:
	// constructor, destructor
	CShaderDeviceDx8();
	virtual ~CShaderDeviceDx8();

	// Call this when another app is initializing or finished initializing
	void OtherAppInitializing( bool initializing ) override;
	
	// This handles any events queued because they were called outside of the owning thread
	void HandleThreadEvent( uint32 threadEvent ) override;

	// FIXME: Make private
	// Which device are we using?
	UINT		m_DisplayAdapter;
	D3DDEVTYPE	m_DeviceType;

protected:
	enum DeviceState_t
	{
		DEVICE_STATE_OK = 0,
		DEVICE_STATE_OTHER_APP_INIT,
		DEVICE_STATE_LOST_DEVICE,
		DEVICE_STATE_NEEDS_RESET,
	};

	struct NonInteractiveRefreshState_t
	{
		IDirect3DVertexShader9 *m_pVertexShader;
		IDirect3DPixelShader9 *m_pPixelShader;
		IDirect3DPixelShader9 *m_pPixelShaderStartup;
		IDirect3DPixelShader9 *m_pPixelShaderStartupPass2;
		IDirect3DVertexDeclaration9 *m_pVertexDecl;
		ShaderNonInteractiveInfo_t m_Info;
		MaterialNonInteractiveMode_t m_Mode;
		double m_flLastPacifierTime;
		int m_nPacifierFrame;

		double m_flStartTime;
		double m_flLastPresentTime;
		double m_flPeakDt;
		double m_flTotalDt;
		int m_nSamples;
		int m_nCountAbove66;
	};

protected:
	// Creates the D3D Device
	bool CreateD3DDevice( void* pHWnd, unsigned nAdapter, const ShaderDeviceInfo_t &info );

	// Actually creates the D3D Device once the present parameters are set up
	IDirect3DDevice9* InvokeCreateDevice( void* hWnd, unsigned nAdapter, DWORD deviceCreationFlags );

	// Checks for CreateQuery support
	void DetectQuerySupport( IDirect3DDevice9* pD3DDevice );

	// Computes the presentation parameters
	void SetPresentParameters( void* hWnd, unsigned nAdapter, const ShaderDeviceInfo_t &info );

	// Computes the supersample flags
	D3DMULTISAMPLE_TYPE ComputeMultisampleType( int nSampleCount );

	// Is the device active?
	bool IsActive() const;

	// Try to reset the device, returned true if it succeeded
	bool TryDeviceReset();

	// Queue up the fact that the device was lost
	void MarkDeviceLost();

	// Deals with lost devices
	void CheckDeviceLost( bool bOtherAppInitializing );

	// Changes the window size
	bool ResizeWindow( const ShaderDeviceInfo_t &info );

	// Deals with the frame synching object
	void AllocFrameSyncObjects( void );
	void FreeFrameSyncObjects( void );

	// Alloc and free objects that are necessary for frame syncing
	void AllocFrameSyncTextureObject();
	void FreeFrameSyncTextureObject();

	// Alloc and free objects necessary for noninteractive frame refresh on the x360
	bool AllocNonInteractiveRefreshObjects();
	void FreeNonInteractiveRefreshObjects();

	// FIXME: This is for backward compat; I still haven't solved a way of decoupling this
	virtual bool OnAdapterSet() = 0;
	virtual void ResetRenderState( bool bFullReset = true ) = 0;

	// For measuring if we meed TCR 022 on the XBox (refreshing often enough)
	void UpdatePresentStats();

	bool InNonInteractiveMode() const;

	void ReacquireResourcesInternal( bool bResetState = false, bool bForceReacquire = false, char const *pszForceReason = NULL );

#ifdef DX_TO_GL_ABSTRACTION
public:
	virtual void DoStartupShaderPreloading( void );
protected:
#endif

	D3DPRESENT_PARAMETERS m_PresentParameters;
	ImageFormat			m_AdapterFormat;

	// Mode info
	int					m_DeviceSupportsCreateQuery;

	ShaderDeviceInfo_t	m_PendingVideoModeChangeConfig;
	DeviceState_t		m_DeviceState;

	bool				m_bOtherAppInitializing : 1;
	bool				m_bQueuedDeviceLost : 1;
	bool				m_IsResizing : 1;
	bool				m_bPendingVideoModeChange : 1;
	bool				m_bUsingStencil : 1;
	bool				m_bResourcesReleased : 1;

	// amount of stencil variation we have available
	int					m_iStencilBufferBits;

	// Frame sync objects
	IDirect3DQuery9		*m_pFrameSyncQueryObject[NUM_FRAME_SYNC_QUERIES];
	bool				m_bQueryIssued[NUM_FRAME_SYNC_QUERIES];
	int					m_currentSyncQuery;
	IDirect3DTexture9	*m_pFrameSyncTexture;

	CUtlString			m_sDisplayDeviceName;

	// Used for x360 only
	NonInteractiveRefreshState_t m_NonInteractiveRefresh;
	CThreadFastMutex m_nonInteractiveModeMutex;
	friend class CShaderDeviceMgrDx8;

	int	m_numReleaseResourcesRefCount;		// This is holding the number of ReleaseResources calls queued up,
											// for every ReleaseResources call there should be a matching call to
											// ReacquireResources, only the last top-level ReacquireResources will
											// have effect. Nested ReleaseResources calls are bugs.
};


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
extern IDirect3DDevice9 *g_pD3DDevice;
FORCEINLINE IDirect3DDevice9 *Dx9Device()
{
	return g_pD3DDevice;
}

extern CShaderDeviceDx8* g_pShaderDeviceDx8;


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
FORCEINLINE bool CShaderDeviceDx8::IsActive() const
{
	return ( g_pD3DDevice != NULL );
}

// used to determine if we're deactivated
FORCEINLINE bool CShaderDeviceDx8::IsDeactivated() const 
{ 
	return ( IsPC() && ( ( m_DeviceState != DEVICE_STATE_OK ) || m_bQueuedDeviceLost || m_numReleaseResourcesRefCount ) ); 
}


#endif // SHADERDEVICEDX8_H
