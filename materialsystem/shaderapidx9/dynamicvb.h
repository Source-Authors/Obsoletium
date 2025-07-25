//========= Copyright Valve Corporation, All rights reserved. ============//
//
// D. Sim Dietrich Jr.
// sim.dietrich@nvidia.com

#ifndef DYNAMICVB_H
#define DYNAMICVB_H

#ifdef _WIN32
#pragma once
#endif

#include "locald3dtypes.h"
#include "recording.h"
#include "shaderapidx8_global.h"
#include "shaderapidx8.h"
#include "imeshdx8.h"
#include "materialsystem/ivballoctracker.h"
#include "gpubufferallocator.h"

#include "tier1/memstack.h"
#include "tier1/utllinkedlist.h"
#include "tier0/dbg.h"

#include "windows/com_error_category.h"

// Helper function to unbind an vertex buffer
void Unbind( IDirect3DVertexBuffer9 *pVertexBuffer );


class CVertexBuffer
{
public:
	CVertexBuffer( IDirect3DDevice9Ex * pD3D, VertexFormat_t fmt, DWORD theFVF, int vertexSize,
					int theVertexCount, const char *pTextureBudgetName, bool bSoftwareVertexProcessing, bool dynamic = false );
	~CVertexBuffer();
	
	IDirect3DVertexBuffer* GetInterface() const 
	{ 
		// If this buffer still exists, then Late Creation didn't happen. Best case: we'll render the wrong image. Worst case: Crash.
		Assert( !m_pSysmemBuffer );
		return m_pVB; 
	}
	
	// Use at beginning of frame to force a flush of VB contents on first draw
	void FlushAtFrameStart() { m_bFlush = true; }
	
	// lock, unlock
	unsigned char* Lock( int numVerts, int& baseVertexIndex );	
	unsigned char* Modify( bool bReadOnly, int firstVertex, int numVerts );	
	void Unlock( int numVerts );

	void HandleLateCreation( IDirect3DDevice9Ex *pD3D );

	// Vertex size
	int VertexSize() const { return m_VertexSize; }

	// Vertex count
	int VertexCount() const { return m_VertexCount; }

	static int BufferCount()
	{
#ifdef _DEBUG
		return s_BufferCount;
#else
		return 0;
#endif
	}

	// UID
	unsigned int UID() const 
	{ 
#ifdef RECORDING
		return m_UID; 
#else
		return 0;
#endif
	}

	void HandlePerFrameTextureStats( int frame )
	{
#ifdef VPROF_ENABLED
		if ( m_Frame != frame && !m_bDynamic )
		{
			m_Frame = frame;
			m_pFrameCounter += m_nBufferSize;
		}
#endif
	}
	
	// Do we have enough room without discarding?
	bool HasEnoughRoom( int numVertices ) const;

	// Is this dynamic?
	bool IsDynamic() const { return m_bDynamic; }
	bool IsExternal() const { return m_bExternalMemory; }

	// Block until this part of the vertex buffer is free
	void BlockUntilUnused( int nBufferSize );

	// used to alter the characteristics after creation
	// allows one dynamic vb to be shared for multiple formats
	void ChangeConfiguration( int vertexSize, int totalSize ) 
	{
		Assert( m_bDynamic && !m_bLocked && vertexSize );
		m_VertexSize = vertexSize;
		m_VertexCount = m_nBufferSize / vertexSize;
	}

	// Compute the next offset for the next lock
	int NextLockOffset( ) const;

	// Returns the allocated size
	int AllocationSize() const;

	// Returns the number of vertices we have enough room for
	int NumVerticesUntilFlush() const
	{
		return (m_nBufferSize - NextLockOffset()) / m_VertexSize;
	}

private:
	void Create( IDirect3DDevice9Ex *pD3D );
	inline void ReallyUnlock( [[maybe_unused]] int unlockBytes )
	{
#if DX_TO_GL_ABSTRACTION
		// Knowing how much data was actually written is critical for performance under OpenGL.
		m_pVB->UnlockActualSize( unlockBytes );
#else
		const HRESULT hr = m_pVB->Unlock();
		if (FAILED(hr))
		{
			Warning( __FUNCTION__ ": IDirect3DVertexBuffer9::Unlock() failed w/e %s.\n",
				se::win::com::com_error_category().message(hr).c_str() );
		}
#endif
	}

	enum LOCK_FLAGS
	{
		LOCKFLAGS_FLUSH  = D3DLOCK_NOSYSLOCK | D3DLOCK_DISCARD,
		LOCKFLAGS_APPEND = D3DLOCK_NOSYSLOCK | D3DLOCK_NOOVERWRITE
	};

	se::win::com::com_ptr<IDirect3DVertexBuffer9> m_pVB;

	VertexFormat_t	m_VertexBufferFormat;		// yes, Vertex, only used for allocation tracking
	int				m_nBufferSize;
	int				m_Position;
	int				m_VertexCount;
	int				m_VertexSize;
	DWORD			m_TheFVF;
	byte			*m_pSysmemBuffer;
	int				m_nSysmemBufferStartBytes;

	uint			m_nLockCount;
	unsigned char	m_bDynamic : 1;
	unsigned char	m_bLocked : 1;
	unsigned char	m_bFlush : 1;
	unsigned char	m_bExternalMemory : 1;
	unsigned char	m_bSoftwareVertexProcessing : 1;
	unsigned char	m_bLateCreateShouldDiscard : 1;

#ifdef VPROF_ENABLED
	int				m_Frame;
	uintp				*m_pFrameCounter;
	uintp				*m_pGlobalCounter;
#endif

#ifdef _DEBUG
	static int		s_BufferCount;
#endif

#ifdef RECORDING
	unsigned int	m_UID;
#endif
};

// constructor, destructor

inline CVertexBuffer::CVertexBuffer(IDirect3DDevice9Ex * pD3D, VertexFormat_t fmt, DWORD theFVF, 
	int vertexSize, int vertexCount, const char *pTextureBudgetName,
	bool bSoftwareVertexProcessing, bool dynamic ) :
		m_VertexBufferFormat( fmt ),
		m_nBufferSize(vertexSize * vertexCount),
		m_Position(0),
		m_VertexCount(vertexCount),
		m_VertexSize(vertexSize), 
		m_TheFVF( theFVF ),
		m_bDynamic(dynamic),
		m_bLocked(false), 
		m_bFlush(true),
		m_bExternalMemory( false ),
		m_bSoftwareVertexProcessing( bSoftwareVertexProcessing ),
		m_bLateCreateShouldDiscard( false )
#ifdef VPROF_ENABLED
		, m_Frame( -1 )
		, m_pFrameCounter( nullptr )
#endif
{
	MEM_ALLOC_CREDIT_( pTextureBudgetName );
	
	m_nSysmemBufferStartBytes = 0;
	m_nLockCount = 0;

#ifdef RECORDING
	// assign a UID
	static unsigned int uid = 0;
	m_UID = uid++;
#endif

#ifdef _DEBUG
	++s_BufferCount;
#endif

#ifdef VPROF_ENABLED
	if ( !m_bDynamic )
	{
		char name[256];
		V_strcpy_safe( name, "TexGroup_global_" );
		V_strcat_safe( name, pTextureBudgetName, sizeof(name) );
		m_pGlobalCounter = g_VProfCurrentProfile.FindOrCreateCounter( name, COUNTER_GROUP_TEXTURE_GLOBAL );

		V_strcpy_safe( name, "TexGroup_frame_" );
		V_strcat_safe( name, pTextureBudgetName, sizeof(name) );
		m_pFrameCounter = g_VProfCurrentProfile.FindOrCreateCounter( name, COUNTER_GROUP_TEXTURE_PER_FRAME );
	}
	else
	{
		m_pGlobalCounter = g_VProfCurrentProfile.FindOrCreateCounter( "TexGroup_global_" TEXTURE_GROUP_DYNAMIC_VERTEX_BUFFER, COUNTER_GROUP_TEXTURE_GLOBAL );
	}
#endif

	if ( !g_pShaderUtil->IsRenderThreadSafe() )
	{
		m_pSysmemBuffer = ( byte * )MemAlloc_AllocAligned( m_nBufferSize, 16 );
		m_nSysmemBufferStartBytes = 0;
	}
	else
	{
		m_pSysmemBuffer = NULL;
		Create( pD3D );
	}

#ifdef VPROF_ENABLED
	if ( !m_bDynamic )
	{
		Assert( m_pGlobalCounter );
		*m_pGlobalCounter += m_nBufferSize;
	}
#endif
}


void CVertexBuffer::Create( IDirect3DDevice9Ex *pD3D )
{
	D3DVERTEXBUFFER_DESC desc;
	BitwiseClear( desc );

	desc.Format = D3DFMT_VERTEXDATA;
	desc.Type = D3DRTYPE_VERTEXBUFFER;
	desc.Usage = D3DUSAGE_WRITEONLY;

	if ( m_bDynamic )
	{
		desc.Usage |= D3DUSAGE_DYNAMIC;
		// Dynamic meshes should never be compressed (slows down writing to them)
		Assert( CompressionType( m_TheFVF ) == VERTEX_COMPRESSION_NONE );
	}
	if ( m_bSoftwareVertexProcessing )
	{
		desc.Usage |= D3DUSAGE_SOFTWAREPROCESSING;
	}

#if defined(IS_WINDOWS_PC) && defined(SHADERAPIDX9)
	desc.Pool = D3DPOOL_DEFAULT;
#else
	desc.Pool = m_bDynamic ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
#endif

	desc.Size = m_nBufferSize;
	desc.FVF = m_TheFVF;

	RECORD_COMMAND( DX8_CREATE_VERTEX_BUFFER, 6 );
	RECORD_INT( m_UID );
	RECORD_INT( m_nBufferSize );
	RECORD_INT( desc.Usage );
	RECORD_INT( desc.FVF );
	RECORD_INT( desc.Pool );
	RECORD_INT( m_bDynamic );

	HRESULT hr = pD3D->CreateVertexBuffer(
		m_nBufferSize,
		desc.Usage,
		desc.FVF,
		desc.Pool,
		&m_pVB,
		nullptr );
	if ( FAILED( hr ) )
	{
		Warning( __FUNCTION__ ": IDirect3DDevice9Ex::CreateVertexBuffer(length = %u, usage = 0x%x, format = 0x%x, pool = 0x%x) failed w/e %s. Retrying...\n",
			m_nBufferSize,
			desc.Usage,
			desc.Format,
			desc.Pool,
			se::win::com::com_error_category().message(hr).c_str() );
	
		if ( hr == D3DERR_OUTOFVIDEOMEMORY || hr == E_OUTOFMEMORY )
		{
			// Don't have the memory for this.  Try flushing all managed resources out of vid mem and try again.
			// FIXME: need to record this
			hr = pD3D->EvictManagedResources();
			if ( FAILED( hr ) )
			{
				Warning( __FUNCTION__ ": IDirect3DDevice9Ex::EvictManagedResources() failed w/e %s.\n",
					se::win::com::com_error_category().message(hr).c_str() );
			}

			hr = pD3D->CreateVertexBuffer(
				m_nBufferSize,
				desc.Usage,
				desc.FVF,
				desc.Pool,
				&m_pVB,
				NULL );
			if ( FAILED( hr ) )
			{
				Warning( __FUNCTION__ ": IDirect3DDevice9Ex::CreateVertexBuffer(length = %u, usage = 0x%x, format = 0x%x, pool = 0x%x) failed w/e %s. Skipping.\n",
					m_nBufferSize,
					desc.Usage,
					desc.Format,
					desc.Pool,
					se::win::com::com_error_category().message(hr).c_str() );
			}
		}
	}
	
	Assert( m_pVB );

#ifdef MEASURE_DRIVER_ALLOCATIONS
	int nMemUsed = 1024;
	VPROF_INCREMENT_GROUP_COUNTER( "vb count", COUNTER_GROUP_NO_RESET, 1 );
	VPROF_INCREMENT_GROUP_COUNTER( "vb driver mem", COUNTER_GROUP_NO_RESET, nMemUsed );
	VPROF_INCREMENT_GROUP_COUNTER( "total driver mem", COUNTER_GROUP_NO_RESET, nMemUsed );
#endif

#if defined( _DEBUG )
	if ( m_pVB && !m_pSysmemBuffer )
	{
		D3DVERTEXBUFFER_DESC actual_desc;
		hr = m_pVB->GetDesc( &actual_desc );
		if ( SUCCEEDED( hr ) )
		{
			Assert( memcmp( &actual_desc, &desc, sizeof( desc ) ) == 0 );
		}
		else
		{
			Warning( __FUNCTION__ ": IDirect3DVertexBuffer9::GetDesc() failed w/e %s.\n",
				se::win::com::com_error_category().message(hr).c_str() );
		}
	}
#endif

	// Track VB allocations
	g_VBAllocTracker->CountVB( m_pVB, m_bDynamic, m_nBufferSize, m_VertexSize, m_VertexBufferFormat );
}


inline CVertexBuffer::~CVertexBuffer()
{
	// Track VB allocations (even if pooled)
	if ( m_pVB )
	{
		g_VBAllocTracker->UnCountVB( m_pVB );
	}

	if ( !m_bExternalMemory )
	{
#ifdef MEASURE_DRIVER_ALLOCATIONS
		int nMemUsed = 1024;
		VPROF_INCREMENT_GROUP_COUNTER( "vb count", COUNTER_GROUP_NO_RESET, -1 );
		VPROF_INCREMENT_GROUP_COUNTER( "vb driver mem", COUNTER_GROUP_NO_RESET, -nMemUsed );
		VPROF_INCREMENT_GROUP_COUNTER( "total driver mem", COUNTER_GROUP_NO_RESET, -nMemUsed );
#endif

#ifdef VPROF_ENABLED
		if ( !m_bDynamic )
		{
			Assert( m_pGlobalCounter );
			*m_pGlobalCounter -= m_nBufferSize;
		}
#endif

#ifdef _DEBUG
		--s_BufferCount;
#endif
	}

	Unlock( 0 );

	if ( m_pSysmemBuffer )
	{
		MemAlloc_FreeAligned( m_pSysmemBuffer );
		m_pSysmemBuffer = NULL;
	}

	if ( m_pVB )
	{
		RECORD_COMMAND( DX8_DESTROY_VERTEX_BUFFER, 1 );
		RECORD_INT( m_UID );

		m_pVB.Release();
	}
}

//-----------------------------------------------------------------------------
// Compute the next offset for the next lock
//-----------------------------------------------------------------------------
inline int CVertexBuffer::NextLockOffset( ) const
{
	int nNextOffset = ( m_Position + m_VertexSize - 1 ) / m_VertexSize;
	nNextOffset *= m_VertexSize;
	return nNextOffset;
}


//-----------------------------------------------------------------------------
// Do we have enough room without discarding?
//-----------------------------------------------------------------------------
inline bool CVertexBuffer::HasEnoughRoom( int numVertices ) const
{
	return (NextLockOffset() + (numVertices * m_VertexSize)) <= m_nBufferSize;
}

//-----------------------------------------------------------------------------
// Block until this part of the index buffer is free
//-----------------------------------------------------------------------------
inline void CVertexBuffer::BlockUntilUnused( int nBufferSize )
{
	Assert( nBufferSize <= m_nBufferSize );
}


//-----------------------------------------------------------------------------
// lock, unlock
//-----------------------------------------------------------------------------
inline unsigned char* CVertexBuffer::Lock( int numVerts, int& baseVertexIndex )
{
	m_nLockCount = numVerts;

	unsigned char* pLockedData = 0;
	baseVertexIndex = 0;
	int nBufferSize = numVerts * m_VertexSize;

	// Ensure there is enough space in the VB for this data
	if ( numVerts > m_VertexCount ) 
	{ 
		Assert( 0 );
		return 0; 
	}
	
	if ( !m_pVB && !m_pSysmemBuffer )
		return 0;

	DWORD dwFlags;
	if ( m_bDynamic )
	{
		dwFlags = LOCKFLAGS_APPEND;

		// If either the user forced us to flush,
		// or there is not enough space for the vertex data,
		// then flush the buffer contents
		if ( !m_Position || m_bFlush || !HasEnoughRoom(numVerts) )
		{
			if ( m_pSysmemBuffer || !g_pShaderUtil->IsRenderThreadSafe() )
				m_bLateCreateShouldDiscard = true;
			m_bFlush = false;
			m_Position = 0;
			
			dwFlags = LOCKFLAGS_FLUSH;
		}
	}
	else
	{
		// Since we are a static VB, always lock the beginning of the buffer.
		dwFlags = D3DLOCK_NOSYSLOCK;
		m_Position = 0;
	}

	int nLockOffset = NextLockOffset( );
	RECORD_COMMAND( DX8_LOCK_VERTEX_BUFFER, 4 );
	RECORD_INT( m_UID );
	RECORD_INT( nLockOffset );
	RECORD_INT( nBufferSize );
	RECORD_INT( dwFlags );

	// If the caller isn't in the thread that owns the render lock, need to return a system memory pointer--cannot talk to GL from 
	// the non-current thread. 
	if ( !m_pSysmemBuffer && !g_pShaderUtil->IsRenderThreadSafe() )
	{
		m_pSysmemBuffer = ( byte * )MemAlloc_AllocAligned( m_nBufferSize, 16 );
		m_nSysmemBufferStartBytes = nLockOffset;
		Assert( ( m_nSysmemBufferStartBytes % m_VertexSize ) == 0 );
	}

	if ( m_pSysmemBuffer != NULL )
	{
		// Ensure that we're never moving backwards in a buffer--this code would need to be rewritten if so. 
		// We theorize this can happen if you hit the end of a buffer and then wrap before drawing--but
		// this would probably break in other places as well.
		Assert( nLockOffset >= m_nSysmemBufferStartBytes );
		pLockedData = m_pSysmemBuffer + nLockOffset;
	}
	else
	{
		const HRESULT hr = m_pVB->Lock(
			nLockOffset,
			nBufferSize, 
			reinterpret_cast< void** >( &pLockedData ),
			dwFlags );
		if (FAILED(hr))
		{
			Warning( __FUNCTION__ ": IDirect3DVertexBuffer9::Lock(offset = 0x%x, size = 0x%x, flags = 0x%x) failed w/e %s.\n",
				nLockOffset,
				nBufferSize,
				dwFlags,
				se::win::com::com_error_category().message(hr).c_str() );
		}
	}

	Assert( pLockedData != 0 );
	m_bLocked = true;

	baseVertexIndex = nLockOffset / m_VertexSize;
	return pLockedData;
}

inline unsigned char* CVertexBuffer::Modify( bool bReadOnly, int firstVertex, int numVerts )
{
	unsigned char* pLockedData = 0;
		
	// D3D still returns a pointer when you call lock with 0 verts, so just in
	// case it's actually doing something, don't even try to lock the buffer with 0 verts.
	if ( numVerts == 0 )
		return NULL;

	m_nLockCount = numVerts;

	// If this hits, m_pSysmemBuffer logic needs to be added to this code.
	Assert( g_pShaderUtil->IsRenderThreadSafe() );
	Assert( !m_pSysmemBuffer );		// if this hits, then we need to add code to handle it

	Assert( m_pVB && !m_bDynamic );

	if ( firstVertex + numVerts > m_VertexCount ) 
	{ 
		Assert( 0 );
		return NULL;
	}

	DWORD dwFlags = D3DLOCK_NOSYSLOCK;
	if ( bReadOnly )
	{
		dwFlags |= D3DLOCK_READONLY;
	}

	RECORD_COMMAND( DX8_LOCK_VERTEX_BUFFER, 4 );
	RECORD_INT( m_UID );
	RECORD_INT( firstVertex * m_VertexSize );
	RECORD_INT( numVerts * m_VertexSize );
	RECORD_INT( dwFlags );

	// mmw: for forcing all dynamic...        LOCKFLAGS_FLUSH );
	const HRESULT hr = m_pVB->Lock(
		firstVertex * m_VertexSize, 
		numVerts * m_VertexSize, 
		reinterpret_cast< void** >( &pLockedData ), 
		dwFlags );
	if (FAILED(hr))
	{
		Warning( __FUNCTION__ ": IDirect3DVertexBuffer9::Lock(offset = 0x%x, size = 0x%x, flags = 0x%x) failed w/e %s.\n",
			firstVertex * m_VertexSize,
			numVerts * m_VertexSize,
			dwFlags,
			se::win::com::com_error_category().message(hr).c_str() );
	}
	
	m_Position = firstVertex * m_VertexSize;
	Assert( pLockedData != 0 );
	m_bLocked = true;

	return pLockedData;
}

inline void CVertexBuffer::Unlock( int numVerts )
{
	if ( !m_bLocked )
		return;

	if ( !m_pVB && !m_pSysmemBuffer )
		return;

	int nLockOffset = NextLockOffset();
	int nBufferSize = numVerts * m_VertexSize;

	RECORD_COMMAND( DX8_UNLOCK_VERTEX_BUFFER, 1 );
	RECORD_INT( m_UID );

	if ( m_pSysmemBuffer != NULL )
	{
	}
	else
	{
#if DX_TO_GL_ABSTRACTION
		Assert( numVerts <= (int)m_nLockCount );
		int unlockBytes = ( m_bDynamic ? nBufferSize : ( m_nLockCount * m_VertexSize ) );
#else
		int unlockBytes = 0;
#endif

		ReallyUnlock( unlockBytes );
	}
	m_Position = nLockOffset + nBufferSize;

	m_bLocked = false;
}


inline void CVertexBuffer::HandleLateCreation( IDirect3DDevice9Ex *pD3D )
{
	if ( !m_pSysmemBuffer )
	{
		return;
	}

	if( !m_pVB )
	{
		bool bPrior = g_VBAllocTracker->TrackMeshAllocations( "HandleLateCreation" );
		Create( pD3D );
		if ( !bPrior )
		{
			g_VBAllocTracker->TrackMeshAllocations( NULL );
		}
	}

	void* pWritePtr = NULL;
	const int dataToWriteBytes = m_bDynamic ? ( m_Position - m_nSysmemBufferStartBytes ) : ( m_nLockCount * m_VertexSize );
	DWORD dwFlags = D3DLOCK_NOSYSLOCK;
	if ( m_bDynamic )
	{
		dwFlags |= ( m_bLateCreateShouldDiscard ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE );
	}
	
	// Always clear this.
	m_bLateCreateShouldDiscard = false;
	
	// Don't use the Lock function, it does a bunch of stuff we don't want.
	HRESULT hr = m_pVB->Lock(
		m_nSysmemBufferStartBytes,
		dataToWriteBytes,
		&pWritePtr,
		dwFlags);
	if (FAILED(hr))
	{
		Warning( __FUNCTION__ ": IDirect3DVertexBuffer9::Lock(offset = 0x%x, size = 0x%x, flags = 0x%x) failed w/e %s.\n",
			m_nSysmemBufferStartBytes,
			dataToWriteBytes,
			dwFlags,
			se::win::com::com_error_category().message(hr).c_str() );
	}

	// If this fails we're about to crash. Consider skipping the update and leaving 
	// m_pSysmemBuffer around to try again later. (For example in case of device loss)
	Assert( SUCCEEDED( hr ) );
	memcpy( pWritePtr, m_pSysmemBuffer + m_nSysmemBufferStartBytes, dataToWriteBytes );
	ReallyUnlock( dataToWriteBytes );

	MemAlloc_FreeAligned( m_pSysmemBuffer );
	m_pSysmemBuffer = NULL;
}


// Returns the allocated size
inline int CVertexBuffer::AllocationSize() const
{
	return m_VertexCount * m_VertexSize;
}


#endif  // DYNAMICVB_H

