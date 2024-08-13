//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef SHADERDEVICEDX10_H
#define SHADERDEVICEDX10_H

#ifdef _WIN32
#pragma once
#endif


#include "shaderdevicebase.h"
#include "tier1/utlvector.h"
#include "tier1/utlrbtree.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlstring.h"
#include "com_ptr.h"


//-----------------------------------------------------------------------------
// Forward declaration
//-----------------------------------------------------------------------------
struct IDXGIFactory1;
struct IDXGIAdapter1;
struct IDXGIOutput;
struct IDXGISwapChain;
struct ID3D10Device;
struct ID3D10RenderTargetView;
struct ID3D10VertexShader;
struct ID3D10PixelShader;
struct ID3D10GeometryShader;
struct ID3D10InputLayout;
struct ID3D10ShaderReflection;


//-----------------------------------------------------------------------------
// The Base implementation of the shader device
//-----------------------------------------------------------------------------
class CShaderDeviceMgrDx10 : public CShaderDeviceMgrBase
{
	typedef CShaderDeviceMgrBase BaseClass;

public:
	// constructor, destructor
	CShaderDeviceMgrDx10();
	virtual ~CShaderDeviceMgrDx10();

	// Methods of IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Methods of IShaderDeviceMgr
	virtual unsigned	 GetAdapterCount() const;
	virtual void GetAdapterInfo( unsigned adapter, MaterialAdapterInfo_t& info ) const;
	virtual unsigned	 GetModeCount( unsigned nAdapter ) const;
	virtual void GetModeInfo( ShaderDisplayMode_t* pInfo, unsigned nAdapter, unsigned mode ) const;
	virtual void GetCurrentModeInfo( ShaderDisplayMode_t* pInfo, unsigned nAdapter ) const;
	virtual bool SetAdapter( unsigned nAdapter, int nFlags );
	virtual CreateInterfaceFn SetMode( void *hWnd, unsigned nAdapter, const ShaderDeviceInfo_t& mode );

private:
	// Initialize adapter information
	void InitAdapterInfo();

	// Determines hardware caps from D3D
	bool ComputeCapsFromD3D( HardwareCaps_t *pCaps, IDXGIAdapter1 *pAdapter, IDXGIOutput *pOutput );

	// Returns the amount of video memory in bytes for a particular adapter
	virtual size_t GetVidMemBytes( unsigned nAdapter ) const;

	// Returns the appropriate adapter output to use
	se::win::com::com_ptr<IDXGIOutput> GetAdapterOutput( unsigned nAdapter ) const;

	// Returns the adapter interface for a particular adapter
	se::win::com::com_ptr<IDXGIAdapter1> GetAdapter( unsigned nAdapter ) const;

	// Used to enumerate adapters, attach to windows
	se::win::com::com_ptr<IDXGIFactory1> m_pDXGIFactory;

	bool m_bObeyDxCommandlineOverride: 1;

	friend class CShaderDeviceDx10;
};


//-----------------------------------------------------------------------------
// The Dx10 implementation of the shader device
//-----------------------------------------------------------------------------
class CShaderDeviceDx10 : public CShaderDeviceBase
{
public:
	// constructor, destructor
	CShaderDeviceDx10();
	virtual ~CShaderDeviceDx10();

public:
	// Methods of IShaderDevice
	bool IsUsingGraphics() const override;
	unsigned GetCurrentAdapter() const override;
	ImageFormat GetBackBufferFormat() const override;
	void GetBackBufferDimensions( int& width, int& height ) const override;
	void SpewDriverInfo() const override;
	void Present() override;
	IShaderBuffer* CompileShader( const char *pProgram, size_t nBufLen, const char *pShaderVersion ) override;
	VertexShaderHandle_t CreateVertexShader( IShaderBuffer *pShader ) override;
	void DestroyVertexShader( VertexShaderHandle_t hShader ) override;
	GeometryShaderHandle_t CreateGeometryShader( IShaderBuffer* pShaderBuffer ) override;
	void DestroyGeometryShader( GeometryShaderHandle_t hShader ) override;
	PixelShaderHandle_t CreatePixelShader( IShaderBuffer* pShaderBuffer ) override;
	void DestroyPixelShader( PixelShaderHandle_t hShader ) override;
	void ReleaseResources() override {}
	void ReacquireResources() override {}
	IMesh* CreateStaticMesh( VertexFormat_t format, const char *pTextureBudgetGroup, IMaterial * pMaterial ) override;
	void DestroyStaticMesh( IMesh* mesh ) override;
	IVertexBuffer *CreateVertexBuffer( ShaderBufferType_t type, VertexFormat_t fmt, int nVertexCount, const char *pTextureBudgetGroup ) override;
	void DestroyVertexBuffer( IVertexBuffer *pVertexBuffer ) override;
	IIndexBuffer *CreateIndexBuffer( ShaderBufferType_t type, MaterialIndexFormat_t fmt, int nIndexCount, const char *pTextureBudgetGroup ) override;
	void DestroyIndexBuffer( IIndexBuffer *pIndexBuffer ) override;
	IVertexBuffer *GetDynamicVertexBuffer( int nStreamID, VertexFormat_t vertexFormat, bool bBuffered = true ) override;
	IIndexBuffer *GetDynamicIndexBuffer( MaterialIndexFormat_t fmt, bool bBuffered = true ) override;
	void SetHardwareGammaRamp( float fGamma, float fGammaTVRangeMin, float fGammaTVRangeMax, float fGammaTVExponent, bool bTVEnabled ) override;

	// A special path used to tick the front buffer while loading on the 360
	void EnableNonInteractiveMode( MaterialNonInteractiveMode_t, ShaderNonInteractiveInfo_t * ) override {}
	void RefreshFrontBufferNonInteractive( ) override {}
	void HandleThreadEvent( uint32 ) override {}

	virtual const char *GetDisplayDeviceName() override;

public:
	// Methods of CShaderDeviceBase
	bool InitDevice( void *hWnd, unsigned nAdapter, const ShaderDeviceInfo_t& mode ) override;
	void ShutdownDevice() override;
	bool IsDeactivated() const override { return false; }
	virtual bool DetermineHardwareCaps();

	// Other public methods
	ID3D10VertexShader* GetVertexShader( VertexShaderHandle_t hShader ) const;
	ID3D10GeometryShader* GetGeometryShader( GeometryShaderHandle_t hShader ) const;
	ID3D10PixelShader* GetPixelShader( PixelShaderHandle_t hShader ) const;
	ID3D10InputLayout* GetInputLayout( VertexShaderHandle_t hShader, VertexFormat_t format );

	// FIXME: Make private
	// Which device are we using?
	UINT		m_DisplayAdapter;
	D3D10_DRIVER_TYPE		m_DriverType;

private:
	struct InputLayout_t
	{
		ID3D10InputLayout *m_pInputLayout;
		VertexFormat_t m_VertexFormat;
	};

	typedef CUtlRBTree< InputLayout_t, unsigned short > InputLayoutDict_t;

	static bool InputLayoutLessFunc( const InputLayout_t &lhs, const InputLayout_t &rhs )	
	{ 
		return ( lhs.m_VertexFormat < rhs.m_VertexFormat );	
	}

	struct VertexShader_t
	{
		ID3D10VertexShader *m_pShader;
		ID3D10ShaderReflection *m_pInfo;
		unsigned char *m_pByteCode;
		size_t m_nByteCodeLen;
		InputLayoutDict_t m_InputLayouts;

		VertexShader_t() : m_InputLayouts( 0, 0, InputLayoutLessFunc ) {}
	};

	struct GeometryShader_t
	{
		ID3D10GeometryShader *m_pShader;
		ID3D10ShaderReflection *m_pInfo;
	};

	struct PixelShader_t
	{
		ID3D10PixelShader *m_pShader;
		ID3D10ShaderReflection *m_pInfo;
	};

	typedef CUtlFixedLinkedList< VertexShader_t >::IndexType_t VertexShaderIndex_t;
	typedef CUtlFixedLinkedList< GeometryShader_t >::IndexType_t GeometryShaderIndex_t;
	typedef CUtlFixedLinkedList< PixelShader_t >::IndexType_t PixelShaderIndex_t;

	void SetupHardwareCaps();
	void ReleaseInputLayouts( VertexShaderIndex_t nIndex );

	se::win::com::com_ptr<IDXGIOutput> m_pOutput;
	se::win::com::com_ptr<ID3D10Device> m_pDevice;
	se::win::com::com_ptr<IDXGISwapChain> m_pSwapChain;
	se::win::com::com_ptr<ID3D10RenderTargetView> m_pRenderTargetView;

	CUtlFixedLinkedList<VertexShader_t> m_VertexShaderDict;
	CUtlFixedLinkedList<GeometryShader_t> m_GeometryShaderDict;
	CUtlFixedLinkedList<PixelShader_t> m_PixelShaderDict;

	CUtlString m_sDisplayDeviceName;

	friend se::win::com::com_ptr<ID3D10Device> D3D10Device();
	friend se::win::com::com_ptr<ID3D10RenderTargetView> D3D10RenderTargetView();
};


//-----------------------------------------------------------------------------
// Inline methods of CShaderDeviceDx10
//-----------------------------------------------------------------------------
inline ID3D10VertexShader* CShaderDeviceDx10::GetVertexShader( VertexShaderHandle_t hShader ) const
{
	if ( hShader != VERTEX_SHADER_HANDLE_INVALID )
		return m_VertexShaderDict[ (VertexShaderIndex_t)hShader ].m_pShader;
	return NULL;
}

inline ID3D10GeometryShader* CShaderDeviceDx10::GetGeometryShader( GeometryShaderHandle_t hShader ) const
{
	if ( hShader != GEOMETRY_SHADER_HANDLE_INVALID )
		return m_GeometryShaderDict[ (GeometryShaderIndex_t)hShader ].m_pShader;
	return NULL;
}

inline ID3D10PixelShader* CShaderDeviceDx10::GetPixelShader( PixelShaderHandle_t hShader ) const
{
	if ( hShader != PIXEL_SHADER_HANDLE_INVALID )
		return m_PixelShaderDict[ (PixelShaderIndex_t)hShader ].m_pShader;
	return NULL;
}


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
extern CShaderDeviceDx10* g_pShaderDeviceDx10;


//-----------------------------------------------------------------------------
// Utility methods
//-----------------------------------------------------------------------------
inline se::win::com::com_ptr<ID3D10Device> D3D10Device()
{
	return g_pShaderDeviceDx10->m_pDevice;	
}

inline se::win::com::com_ptr<ID3D10RenderTargetView> D3D10RenderTargetView()
{
	return g_pShaderDeviceDx10->m_pRenderTargetView;	
}


#endif // SHADERDEVICEDX10_H