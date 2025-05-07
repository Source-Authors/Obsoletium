//========= Copyright Valve Corporation, All rights reserved. ============//
//
// D. Sim Dietrich Jr.
// sim.dietrich@nvidia.com

#ifndef DYNAMICIB_H
#define DYNAMICIB_H

#ifdef _WIN32
#pragma once
#endif

#include "locald3dtypes.h"
#include "recording.h"
#include "shaderapidx8_global.h"
#include "shaderapidx8.h"
#include "shaderapi/ishaderutil.h"
#include "materialsystem/ivballoctracker.h"
#include "gpubufferallocator.h"

#include "tier1/memstack.h"
#include "tier1/strtools.h"
#include "tier1/utlqueue.h"
#include "tier0/memdbgon.h"

// Helper function to unbind an index buffer
void Unbind( IDirect3DIndexBuffer9 *pIndexBuffer );

#define SPEW_INDEX_BUFFER_STALLS //uncomment to allow buffer stall spewing.

class CIndexBuffer
{
public:
	CIndexBuffer( IDirect3DDevice9 *pD3D, int count, bool bSoftwareVertexProcessing, bool dynamic = false );

	int AddRef() { return ++m_nReferenceCount; }
	int Release() 
	{
		int retVal = --m_nReferenceCount;
		if ( retVal == 0 )
			delete this;
		return retVal;
	}

	IDirect3DIndexBuffer* GetInterface() const 
	{ 
		// If this buffer still exists, then Late Creation didn't happen.
		// Best case: we'll render the wrong image.  Worst case: Crash.
		Assert( !m_pSysmemBuffer );
		return m_pIB; 
	}
	
	// Use at beginning of frame to force a flush of VB contents on first draw
	void FlushAtFrameStart() { m_bFlush = true; }
	
	// lock, unlock
	unsigned short *Lock( bool bReadOnly, int numIndices, int &startIndex, int startPosition = -1 );	
	void Unlock( int numIndices );
	void HandleLateCreation( );

	// Index position
	int IndexPosition() const { return m_Position; }
	
	// Index size
	int IndexSize() const { return sizeof(unsigned short); }

	// Index count
	int IndexCount() const { return m_IndexCount; }

	// Do we have enough room without discarding?
	bool HasEnoughRoom( int numIndices ) const;

	bool IsDynamic() const { return m_bDynamic; }
	bool IsExternal() const { return m_bExternalMemory; }

	// Block until there's a free portion of the buffer of this size, m_Position will be updated to point at where this section starts
	void BlockUntilUnused( int nAllocationSize );

#ifdef CHECK_INDICES
	void UpdateShadowIndices( unsigned short *pData )
	{
		Assert( m_LockedStartIndex + m_LockedNumIndices <= m_NumIndices );
		memcpy( m_pShadowIndices + m_LockedStartIndex, pData, m_LockedNumIndices * IndexSize() );
	}

	unsigned short GetShadowIndex( int i )
	{
		Assert( (unsigned)i < m_NumIndices );
		return m_pShadowIndices[i];
	}
#endif

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
			VPROF_INCREMENT_GROUP_COUNTER( "TexGroup_frame_" TEXTURE_GROUP_STATIC_INDEX_BUFFER, 
				COUNTER_GROUP_TEXTURE_PER_FRAME, IndexCount() * IndexSize() );
		}
#endif
	}
	
	static int BufferCount()
	{
#ifdef _DEBUG
		return s_BufferCount;
#else
		return 0;
#endif
	}

	inline int AllocationSize() const;

	inline int AllocationCount() const;

private:
	void Create( IDirect3DDevice9 *pD3D );
	inline void ReallyUnlock( [[maybe_unused]] int unlockBytes )
	{
#if DX_TO_GL_ABSTRACTION
		// Knowing how much data was actually written is critical for performance under OpenGL.
		m_pIB->UnlockActualSize( unlockBytes );
#else
		m_pIB->Unlock();
#endif
	}

	enum LOCK_FLAGS
	{
		LOCKFLAGS_FLUSH  = D3DLOCK_NOSYSLOCK | D3DLOCK_DISCARD,
		LOCKFLAGS_APPEND = D3DLOCK_NOSYSLOCK | D3DLOCK_NOOVERWRITE
	};

	LPDIRECT3DINDEXBUFFER m_pIB;

	int			m_IndexCount;
	int			m_Position;
	byte		*m_pSysmemBuffer;
	int			m_nSysmemBufferStartBytes;
	unsigned char	m_bLocked : 1;
	unsigned char	m_bFlush : 1;
	unsigned char	m_bDynamic : 1;
	unsigned char	m_bExternalMemory : 1;
	unsigned char	m_bSoftwareVertexProcessing : 1;
	unsigned char	m_bLateCreateShouldDiscard : 1;

#ifdef VPROF_ENABLED
	int				m_Frame;
#endif

	CInterlockedInt	m_nReferenceCount;

#ifdef _DEBUG
	static int		s_BufferCount;
#endif

#ifdef RECORDING
	unsigned int	m_UID;
#endif

protected:
#ifdef CHECK_INDICES
	unsigned short *m_pShadowIndices;
	unsigned int m_NumIndices;
#endif
	
	unsigned int m_LockedStartIndex;
	unsigned int m_LockedNumIndices;

private:
	// Must use reference counting functions above
	~CIndexBuffer();
};

// constructor, destructor

inline CIndexBuffer::CIndexBuffer( IDirect3DDevice9 *pD3D, int count, 
	bool bSoftwareVertexProcessing, bool dynamic ) :
		m_pIB(0), 
		m_Position(0), 
		m_bLocked(false),
		m_bFlush(true), 
		m_bDynamic(dynamic),
		m_bExternalMemory(false),
		m_bSoftwareVertexProcessing( bSoftwareVertexProcessing ),
		m_bLateCreateShouldDiscard( false )
#ifdef VPROF_ENABLED
		, m_Frame( -1 )
#endif
		, m_nReferenceCount( 0 ) 
{
	m_nSysmemBufferStartBytes = 0;
	m_LockedStartIndex = 0;
	m_LockedNumIndices = 0;

	// For write-combining, ensure we always have locked memory aligned to 4-byte boundaries
	count = AlignValue( count, 2 );
	m_IndexCount = count; 

	MEM_ALLOC_CREDIT_( m_bDynamic ? ( "D3D: " TEXTURE_GROUP_DYNAMIC_INDEX_BUFFER ) : ( "D3D: " TEXTURE_GROUP_STATIC_INDEX_BUFFER ) );

#ifdef CHECK_INDICES
	m_pShadowIndices = NULL;
#endif

#ifdef RECORDING
	// assign a UID
	static unsigned int uid = 0;
	m_UID = uid++;
#endif

#ifdef _DEBUG
	++s_BufferCount;
#endif

#ifdef CHECK_INDICES
	m_pShadowIndices = new unsigned short[ m_IndexCount ];
	m_NumIndices = m_IndexCount;
#endif

	if ( g_pShaderUtil->GetThreadMode() != MATERIAL_SINGLE_THREADED || !ThreadInMainThread() )
	{
		m_pSysmemBuffer = ( byte * )malloc( count * IndexSize() );
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
		VPROF_INCREMENT_GROUP_COUNTER( "TexGroup_global_" TEXTURE_GROUP_STATIC_INDEX_BUFFER, 
			COUNTER_GROUP_TEXTURE_GLOBAL, IndexCount() * IndexSize() );
	}
#endif
}


void CIndexBuffer::Create( IDirect3DDevice9 *pD3D )
{
	D3DINDEXBUFFER_DESC desc;
	memset( &desc, 0x00, sizeof( desc ) );
	desc.Format = D3DFMT_INDEX16;
	desc.Size = sizeof(unsigned short) * m_IndexCount;
	desc.Type = D3DRTYPE_INDEXBUFFER;
	desc.Pool = D3DPOOL_DEFAULT;
	desc.Usage = D3DUSAGE_WRITEONLY;
	if ( m_bDynamic )
	{
		desc.Usage |= D3DUSAGE_DYNAMIC;
	}
	if ( m_bSoftwareVertexProcessing )
	{
		desc.Usage |= D3DUSAGE_SOFTWAREPROCESSING;
	}

	RECORD_COMMAND( DX8_CREATE_INDEX_BUFFER, 6 );
	RECORD_INT( m_UID );
	RECORD_INT( m_IndexCount * IndexSize() );
	RECORD_INT( desc.Usage );
	RECORD_INT( desc.Format );
	RECORD_INT( desc.Pool );
	RECORD_INT( m_bDynamic );

	HRESULT hr = pD3D->CreateIndexBuffer( 
		m_IndexCount * IndexSize(),
		desc.Usage,
		desc.Format,
		desc.Pool, 
		&m_pIB, 
		NULL );
	if ( hr != D3D_OK )
	{
		Warning( "CreateIndexBuffer failed!\n" );
	}

	if ( ( hr == D3DERR_OUTOFVIDEOMEMORY ) || ( hr == E_OUTOFMEMORY ) )
	{
		// Don't have the memory for this.  Try flushing all managed resources
		// out of vid mem and try again.
		// FIXME: need to record this
		pD3D->EvictManagedResources();
		hr = pD3D->CreateIndexBuffer( m_IndexCount * IndexSize(),
			desc.Usage, desc.Format, desc.Pool, &m_pIB, NULL );
	}

	Assert( m_pIB );
	Assert( hr == D3D_OK );

#ifdef MEASURE_DRIVER_ALLOCATIONS
	int nMemUsed = 1024;
	VPROF_INCREMENT_GROUP_COUNTER( "ib count", COUNTER_GROUP_NO_RESET, 1 );
	VPROF_INCREMENT_GROUP_COUNTER( "ib driver mem", COUNTER_GROUP_NO_RESET, nMemUsed );
	VPROF_INCREMENT_GROUP_COUNTER( "total driver mem", COUNTER_GROUP_NO_RESET, nMemUsed );
#endif

#if defined( _DEBUG )
	if ( IsPC() && m_pIB && !m_pSysmemBuffer )
	{
		D3DINDEXBUFFER_DESC aDesc;
		m_pIB->GetDesc( &aDesc );
		Assert( memcmp( &aDesc, &desc, sizeof( desc ) ) == 0 );
	}
#endif
}


inline CIndexBuffer::~CIndexBuffer()
{
#ifdef _DEBUG
	if ( !m_bExternalMemory )
	{
		--s_BufferCount;
	}
#endif

	Unlock(0);

#ifdef CHECK_INDICES
	if ( m_pShadowIndices )
	{
		delete [] m_pShadowIndices;
		m_pShadowIndices = NULL;
	}
#endif

	if ( m_pSysmemBuffer )
	{
		free( m_pSysmemBuffer );
		m_pSysmemBuffer = NULL;
	}

#ifdef MEASURE_DRIVER_ALLOCATIONS
	if ( !m_bExternalMemory )
	{
		int nMemUsed = 1024;
		VPROF_INCREMENT_GROUP_COUNTER( "ib count", COUNTER_GROUP_NO_RESET, -1 );
		VPROF_INCREMENT_GROUP_COUNTER( "ib driver mem", COUNTER_GROUP_NO_RESET, -nMemUsed );
		VPROF_INCREMENT_GROUP_COUNTER( "total driver mem", COUNTER_GROUP_NO_RESET, -nMemUsed );
	}
#endif

	if ( m_pIB )
	{
		RECORD_COMMAND( DX8_DESTROY_INDEX_BUFFER, 1 );
		RECORD_INT( m_UID );

		m_pIB->Release();
	}

#ifdef VPROF_ENABLED
	if ( !m_bExternalMemory )
	{
		if ( !m_bDynamic )
		{
			VPROF_INCREMENT_GROUP_COUNTER( "TexGroup_global_" TEXTURE_GROUP_STATIC_INDEX_BUFFER,
				COUNTER_GROUP_TEXTURE_GLOBAL, - IndexCount() * IndexSize() );
		}
		else if ( IsX360() )
		{
			VPROF_INCREMENT_GROUP_COUNTER( "TexGroup_global_" TEXTURE_GROUP_DYNAMIC_INDEX_BUFFER,
				COUNTER_GROUP_TEXTURE_GLOBAL, - IndexCount() * IndexSize() );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Do we have enough room without discarding?
//-----------------------------------------------------------------------------
inline bool CIndexBuffer::HasEnoughRoom( int numIndices ) const
{
	return ( numIndices + m_Position ) <= m_IndexCount;
}

//-----------------------------------------------------------------------------
// Block until this part of the index buffer is free
//-----------------------------------------------------------------------------
inline void CIndexBuffer::BlockUntilUnused( int nAllocationSize )
{
	Assert( nAllocationSize <= m_IndexCount );
}


//-----------------------------------------------------------------------------
// lock, unlock
//-----------------------------------------------------------------------------
inline unsigned short* CIndexBuffer::Lock( bool bReadOnly, int numIndices, int& startIndex, int startPosition )
{
	Assert( !m_bLocked );

	unsigned short* pLockedData = NULL;

	// For write-combining, ensure we always have locked memory aligned to 4-byte boundaries
	if( m_bDynamic )
		numIndices = AlignValue( numIndices, 2 );

	// Ensure there is enough space in the IB for this data
	if ( numIndices > m_IndexCount ) 
	{ 
		Error( "too many indices for index buffer. . tell a programmer (%d>%d)\n", numIndices, m_IndexCount );
		return 0; 
	}
	
	if ( !m_pIB && !m_pSysmemBuffer )
		return 0;

	DWORD dwFlags;
	
	if ( m_bDynamic )
	{
		// startPosition now can be != -1, when calling in here with a static (staging) buffer.
		dwFlags = LOCKFLAGS_APPEND;
	
		// If either user forced us to flush,
		// or there is not enough space for the vertex data,
		// then flush the buffer contents
		// xbox must not append at position 0 because nooverwrite cannot be guaranteed
		
		if ( !m_Position || m_bFlush || !HasEnoughRoom(numIndices) )
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
		dwFlags = D3DLOCK_NOSYSLOCK;
	}
	
	if ( bReadOnly )
	{
		dwFlags |= D3DLOCK_READONLY;
	}

	int position = m_Position;
	if( startPosition >= 0 )
	{
		position = startPosition;
	}

	RECORD_COMMAND( DX8_LOCK_INDEX_BUFFER, 4 );
	RECORD_INT( m_UID );
	RECORD_INT( position * IndexSize() );
	RECORD_INT( numIndices * IndexSize() );
	RECORD_INT( dwFlags );

	m_LockedStartIndex = position;
	m_LockedNumIndices = numIndices;

	HRESULT hr = D3D_OK;

	// If the caller isn't in the thread that owns the render lock, need to return a system memory pointer--cannot talk to GL from 
	// the non-current thread. 
	if ( !m_pSysmemBuffer && !g_pShaderUtil->IsRenderThreadSafe() )
	{
		m_pSysmemBuffer = ( byte * )malloc( m_IndexCount * IndexSize() );
		m_nSysmemBufferStartBytes = position * IndexSize();
	}

	if ( m_pSysmemBuffer != NULL )
	{
		// Ensure that we're never moving backwards in a buffer--this code would need to be rewritten if so. 
		// We theorize this can happen if you hit the end of a buffer and then wrap before drawing--but
		// this would probably break in other places as well.
		Assert( position * IndexSize() >= m_nSysmemBufferStartBytes );
		pLockedData = ( unsigned short * )( m_pSysmemBuffer + ( position * IndexSize() ) );
	}
	else 
	{
		hr = m_pIB->Lock( position * IndexSize(), numIndices * IndexSize(), 
						   reinterpret_cast< void** >( &pLockedData ), dwFlags );
	}

	switch ( hr )
	{
		case D3DERR_INVALIDCALL:
			Msg( "D3DERR_INVALIDCALL - Index Buffer Lock Failed in %s on line %d(offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, position * IndexSize(), numIndices * IndexSize(), dwFlags );
			break;
		case D3DERR_DRIVERINTERNALERROR:
			Msg( "D3DERR_DRIVERINTERNALERROR - Index Buffer Lock Failed in %s on line %d (offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, position * IndexSize(), numIndices * IndexSize(), dwFlags );
			break;
		case D3DERR_OUTOFVIDEOMEMORY:
			Msg( "D3DERR_OUTOFVIDEOMEMORY - Index Buffer Lock Failed in %s on line %d (offset %d, size %d, flags 0x%x)\n", V_UnqualifiedFileName(__FILE__), __LINE__, position * IndexSize(), numIndices * IndexSize(), dwFlags );
			break;
	}

	Assert( pLockedData != NULL );
	   
	startIndex = position;

	Assert( m_bLocked == false );
	m_bLocked = true;
	return pLockedData;
}

inline void CIndexBuffer::Unlock( int numIndices )
{
	Assert( (m_Position + numIndices) <= m_IndexCount );

	if ( !m_bLocked )
		return;

	// For write-combining, ensure we always have locked memory aligned to 4-byte boundaries
//	if( m_bDynamic )
//		numIndices = AlignValue( numIndices, 2 );

	if ( !IsX360() && !m_pIB && !m_pSysmemBuffer )
		return;

	RECORD_COMMAND( DX8_UNLOCK_INDEX_BUFFER, 1 );
	RECORD_INT( m_UID );

	if ( m_pSysmemBuffer )
	{
	}
	else 
	{
#if DX_TO_GL_ABSTRACTION
		// Knowing how much data was actually written is critical for performance under OpenGL.
		// Important notes: numIndices indicates how much we could move the current position. For dynamic buffer, it should indicate the # of actually written indices, for static buffers it's typically 0.
		// If it's a dynamic buffer (where we actually care about perf), assume the caller isn't lying about numIndices, otherwise just assume they wrote the entire thing.
		// If you modify this code, be sure to test on both AMD and NVidia drivers!
		Assert( numIndices <= (int)m_LockedNumIndices );
		int unlockBytes = ( m_bDynamic ? numIndices : m_LockedNumIndices ) * IndexSize();
#else
		int unlockBytes = 0; 
#endif
		ReallyUnlock( unlockBytes );
	}

	m_Position += numIndices;
	m_bLocked = false;

	m_LockedStartIndex = 0;
	m_LockedNumIndices = 0;
}


inline void CIndexBuffer::HandleLateCreation( )
{
	if ( !m_pSysmemBuffer )
	{
		return;
	}

	if( !m_pIB )
	{
		bool bPrior = g_VBAllocTracker->TrackMeshAllocations( "HandleLateCreation" );
		Create( Dx9Device() );
		if ( !bPrior )
		{
			g_VBAllocTracker->TrackMeshAllocations( NULL );
		}
	}
	
	void* pWritePtr = NULL;
	const int dataToWriteBytes = ( m_Position * IndexSize() ) - m_nSysmemBufferStartBytes;
	DWORD dwFlags = D3DLOCK_NOSYSLOCK;
	if ( m_bDynamic )
		dwFlags |= ( m_bLateCreateShouldDiscard ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE );
	
	// Always clear this.
	m_bLateCreateShouldDiscard = false;
	
	// Don't use the Lock function, it does a bunch of stuff we don't want.
	[[maybe_unused]] HRESULT hr = m_pIB->Lock( m_nSysmemBufferStartBytes, 
	                         dataToWriteBytes,
				             &pWritePtr,
				             dwFlags);

	// If this fails we're about to crash. Consider skipping the update and leaving 
	// m_pSysmemBuffer around to try again later. (For example in case of device loss)
	Assert( SUCCEEDED( hr ) ); 
	memcpy( pWritePtr, m_pSysmemBuffer + m_nSysmemBufferStartBytes, dataToWriteBytes );
	ReallyUnlock( dataToWriteBytes );

	free( m_pSysmemBuffer );
	m_pSysmemBuffer = NULL;
}


// Returns the allocated size
inline int CIndexBuffer::AllocationSize() const
{
	return m_IndexCount * IndexSize();
}

inline int CIndexBuffer::AllocationCount() const
{
	return m_IndexCount;
}

#include "tier0/memdbgoff.h"

#endif  // DYNAMICIB_H

