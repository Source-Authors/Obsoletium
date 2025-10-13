//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: tracks VB allocations (and compressed/uncompressed vertex memory usage)
//
//===========================================================================//

#include "materialsystem/imaterial.h"
#include "imeshdx8.h"
#include "convar.h"
#include "tier1/utlhash.h"
#include "tier1/utlstack.h"

#include "materialsystem/ivballoctracker.h"


//-----------------------------------------------------------------------------
//
// Types
//
//-----------------------------------------------------------------------------

#if ENABLE_VB_ALLOC_TRACKER

// FIXME: combine this into the lower bits of VertexFormat_t
using VertexElementMap_t = uint64_t;

enum class Saving_t
{
	SAVING_COMPRESSION = 0,
	SAVING_REMOVAL     = 1,
};

struct ElementData
{
	VertexElement_t element;
	int uncompressed;		// uncompressed vertex size
	int currentCompressed;	// current compressed vertex element size
	int idealCompressed;	// ideal future compressed vertex element size
	const char *name;
};

class CounterData
{
public:
	CounterData() : m_memCount( 0 ), m_vertCount( 0 )
	{
		for ( auto &e : m_elementsCompressed )
		{
			e = 0;
		}
		for ( auto &e : m_elementsUncompressed )
		{
			e = 0;
		}

		m_AllocatorName[ 0 ] = 0;
	}

	static constexpr inline intp MAX_NAME_SIZE = 128;
	intp		m_memCount;
	int			m_vertCount;
	int			m_elementsCompressed[ VERTEX_ELEMENT_NUMELEMENTS ];		// Number of compressed verts using each element
	int			m_elementsUncompressed[ VERTEX_ELEMENT_NUMELEMENTS ];	// Number of uncompressed verts using each element
	char		m_AllocatorName[ MAX_NAME_SIZE ];
};

class AllocData
{
public:
	AllocData( void * buffer, int bufferSize, VertexFormat_t fmt, int numVerts, unsigned allocatorHash )
		: m_fmt( fmt ), m_buffer( buffer ), m_bufferSize( bufferSize ), m_numVerts( numVerts ), m_allocatorHash( allocatorHash ) {}
	AllocData() : m_fmt( 0 ), m_buffer( nullptr ), m_bufferSize( 0 ), m_numVerts( 0 ), m_allocatorHash( 0 ) {}

	VertexFormat_t	m_fmt;
	void		*	m_buffer;
	int				m_bufferSize;
	int				m_numVerts;
	unsigned		m_allocatorHash;
};

typedef CUtlHashFixed	< CounterData, 64	>	CCounterTable;
typedef CUtlHashFixed	< AllocData, 4096	>	CAllocTable;
// dimhotepus: short -> unsigned.
typedef CUtlStack		< unsigned			>	CAllocNameHashes;

#endif // ENABLE_VB_ALLOC_TRACKER


class CVBAllocTracker final : public IVBAllocTracker
{
public:
	void			CountVB( void * buffer, bool isDynamic, int bufferSize, int vertexSize, VertexFormat_t fmt ) override;
	void			UnCountVB( void * buffer ) override;
	bool			TrackMeshAllocations( const char * allocatorName ) override;

	void			DumpVBAllocs();

#if ENABLE_VB_ALLOC_TRACKER

public:
	CVBAllocTracker() : m_bSuperSpew( false ) { m_MeshAllocatorName[0] = '\0'; }

private:

	UtlHashFixedHandle_t	TrackAlloc( void * buffer, int   bufferSize, VertexFormat_t   fmt, int   numVerts, unsigned   allocatorHash );
	bool					KillAlloc(  void * buffer, int & bufferSize, VertexFormat_t & fmt, int & numVerts, unsigned & allocatorHash );

	UtlHashFixedHandle_t	GetCounterHandle( const char * allocatorName, unsigned allocatorHash );

	void					SpewElements( const char * allocatorName, unsigned nameHash );
	int						ComputeVertexSize( VertexElementMap_t map, VertexFormat_t fmt, bool compressed );
	VertexElementMap_t		ComputeElementMap( VertexFormat_t fmt, int vertexSize, bool isDynamic );
	void					UpdateElements( CounterData & data, VertexFormat_t fmt, int numVerts, int vertexSize,
											bool isDynamic, bool isCompressed );

	template<Saving_t savingType>
	void					AddSaving( intp & alreadySaved, intp & yetToSave, const char *allocatorName, VertexElement_t element );
	void					SpewExpectedSavings( void );
	void					UpdateData( const char * allocatorName, unsigned allocatorKey, int bufferSize, VertexFormat_t fmt,
										int numVerts, int vertexSize, bool isDynamic, bool isCompressed );

	const char *			GetNameString( int allocatorKey );
	void					SpewData( const char * allocatorName, unsigned nameHash = 0 );
	void					SpewDataSometimes( int inc );


	static constexpr inline int SPEW_RATE  = 64;
	static constexpr inline int MAX_ALLOCATOR_NAME_SIZE = 128;
	char m_MeshAllocatorName[ MAX_ALLOCATOR_NAME_SIZE ];
	bool m_bSuperSpew;

	CCounterTable			m_VBCountTable;
	CAllocTable				m_VBAllocTable;
	CAllocNameHashes		m_VBTableNameHashes;

	// We use a mutex since allocation tracking is accessed from multiple loading threads.
	// CThreadFastMutex is used as contention is expected to be low during loading.
	CThreadFastMutex		m_VBAllocMutex;
#endif // ENABLE_VB_ALLOC_TRACKER
};


//-----------------------------------------------------------------------------
//
// Global data
//
//-----------------------------------------------------------------------------

#if ENABLE_VB_ALLOC_TRACKER

// FIXME: do this in a better way:
static constexpr ElementData positionElement	= { VERTEX_ELEMENT_POSITION,		12, 12,  8, "POSITION    "	}; // (UNDONE: need vertex shader to scale, may cause cracking w/ static props)
static constexpr ElementData normalElement		= { VERTEX_ELEMENT_NORMAL,			12,  4,  4, "NORMAL      "	}; // (UNDONE: PC (2x16-byte Ravi method) or 360 (D3DDECLTYPE_HEND3N))
static constexpr ElementData colorElement		= { VERTEX_ELEMENT_COLOR,			 4,  4,  4, "COLOR       "	}; // (already minimal)
static constexpr ElementData specularElement	= { VERTEX_ELEMENT_SPECULAR,		 4,  4,  4, "SPECULAR    "	}; // (already minimal)
static constexpr ElementData tangentSElement	= { VERTEX_ELEMENT_TANGENT_S,		12, 12,  4, "TANGENT_S   "	}; // (all-but-unused)
static constexpr ElementData tangentTElement	= { VERTEX_ELEMENT_TANGENT_T,		12, 12,  4, "TANGENT_T   "	}; // (all-but-unused)
static constexpr ElementData wrinkleElement		= { VERTEX_ELEMENT_WRINKLE,			 4,  4,  0, "WRINKLE     "	}; // (UNDONE: compress it as a SHORTN in Position.w - is it [0,1]?)
static constexpr ElementData boneIndexElement	= { VERTEX_ELEMENT_BONEINDEX,		 4,  4,  4, "BONEINDEX   "	}; // (already minimal)
static constexpr ElementData boneWeight1Element	= { VERTEX_ELEMENT_BONEWEIGHTS1,	 4,  4,  4, "BONEWEIGHT1 "	}; // (unused)
static constexpr ElementData boneWeight2Element	= { VERTEX_ELEMENT_BONEWEIGHTS2,	 8,  8,  4, "BONEWEIGHT2 "	}; // (UNDONE: take care w.r.t cracking in flex regions)
static constexpr ElementData boneWeight3Element	= { VERTEX_ELEMENT_BONEWEIGHTS3,	12, 12,  8, "BONEWEIGHT3 "	}; // (unused)
static constexpr ElementData boneWeight4Element	= { VERTEX_ELEMENT_BONEWEIGHTS4,	16, 16,  8, "BONEWEIGHT4 "	}; // (unused)
static constexpr ElementData userData1Element	= { VERTEX_ELEMENT_USERDATA1,		 4,  4,  4, "USERDATA1   "	}; // (unused)
static constexpr ElementData userData2Element	= { VERTEX_ELEMENT_USERDATA2,		 8,  8,  4, "USERDATA2   "	}; // (unused)
static constexpr ElementData userData3Element	= { VERTEX_ELEMENT_USERDATA3,		12, 12,  4, "USERDATA3   "	}; // (unused)
#if ( COMPRESSED_NORMALS_TYPE == COMPRESSED_NORMALS_SEPARATETANGENTS_SHORT2 )
static constexpr ElementData userData4Element	= { VERTEX_ELEMENT_USERDATA4,		16,  4,  4, "USERDATA4   "	}; // (UNDONE: PC (2x16-byte Ravi method) or 360 (D3DDECLTYPE_HEND3N))
#else // ( COMPRESSED_NORMALS_TYPE == COMPRESSED_NORMALS_COMBINEDTANGENTS_UBYTE4 )
static constexpr ElementData userData4Element	= { VERTEX_ELEMENT_USERDATA4,		16,  0,  0, "USERDATA4   "	}; // (UNDONE: PC (2x16-byte Ravi method) or 360 (D3DDECLTYPE_HEND3N))
#endif
static constexpr ElementData texCoord1D0Element	= { VERTEX_ELEMENT_TEXCOORD1D_0,	 4,  4,  4, "TEXCOORD1D_0"	}; // (not worth compressing)
static constexpr ElementData texCoord1D1Element	= { VERTEX_ELEMENT_TEXCOORD1D_1,	 4,  4,  4, "TEXCOORD1D_1"	}; // (not worth compressing)
static constexpr ElementData texCoord1D2Element	= { VERTEX_ELEMENT_TEXCOORD1D_2,	 4,  4,  4, "TEXCOORD1D_2"	}; // (not worth compressing)
static constexpr ElementData texCoord1D3Element	= { VERTEX_ELEMENT_TEXCOORD1D_3,	 4,  4,  4, "TEXCOORD1D_3"	}; // (not worth compressing)
static constexpr ElementData texCoord1D4Element	= { VERTEX_ELEMENT_TEXCOORD1D_4,	 4,  4,  4, "TEXCOORD1D_4"	}; // (not worth compressing)
static constexpr ElementData texCoord1D5Element	= { VERTEX_ELEMENT_TEXCOORD1D_5,	 4,  4,  4, "TEXCOORD1D_5"	}; // (not worth compressing)
static constexpr ElementData texCoord1D6Element	= { VERTEX_ELEMENT_TEXCOORD1D_6,	 4,  4,  4, "TEXCOORD1D_6"	}; // (not worth compressing)
static constexpr ElementData texCoord1D7Element	= { VERTEX_ELEMENT_TEXCOORD1D_7,	 4,  4,  4, "TEXCOORD1D_7"	}; // (not worth compressing)
static constexpr ElementData texCoord2D0Element	= { VERTEX_ELEMENT_TEXCOORD2D_0,	 8,  8,  4, "TEXCOORD2D_0"	}; // (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData texCoord2D1Element	= { VERTEX_ELEMENT_TEXCOORD2D_1,	 8,  8,  4, "TEXCOORD2D_1"	}; // (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData texCoord2D2Element	= { VERTEX_ELEMENT_TEXCOORD2D_2,	 8,  8,  4, "TEXCOORD2D_2"	}; // (all-but-unused)
static constexpr ElementData texCoord2D3Element	= { VERTEX_ELEMENT_TEXCOORD2D_3,	 8,  8,  4, "TEXCOORD2D_3"	}; // (unused)
static constexpr ElementData texCoord2D4Element	= { VERTEX_ELEMENT_TEXCOORD2D_4,	 8,  8,  4, "TEXCOORD2D_4"	}; // (unused)
static constexpr ElementData texCoord2D5Element	= { VERTEX_ELEMENT_TEXCOORD2D_5,	 8,  8,  4, "TEXCOORD2D_5"	}; // (unused)
static constexpr ElementData texCoord2D6Element	= { VERTEX_ELEMENT_TEXCOORD2D_6,	 8,  8,  4, "TEXCOORD2D_6"	}; // (unused)
static constexpr ElementData texCoord2D7Element	= { VERTEX_ELEMENT_TEXCOORD2D_7,	 8,  8,  4, "TEXCOORD2D_7"	}; // (unused)
static constexpr ElementData texCoord3D0Element	= { VERTEX_ELEMENT_TEXCOORD3D_0,	12, 12,  8, "TEXCOORD3D_0"	}; // FIXME: used how much? (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData texCoord3D1Element	= { VERTEX_ELEMENT_TEXCOORD3D_1,	12, 12,  8, "TEXCOORD3D_1"	}; // FIXME: used how much? (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData texCoord3D2Element	= { VERTEX_ELEMENT_TEXCOORD3D_2,	12, 12,  8, "TEXCOORD3D_2"	}; // FIXME: used how much? (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData texCoord3D3Element	= { VERTEX_ELEMENT_TEXCOORD3D_3,	12, 12,  8, "TEXCOORD3D_3"	}; // FIXME: used how much? (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData texCoord3D4Element	= { VERTEX_ELEMENT_TEXCOORD3D_4,	12, 12,  8, "TEXCOORD3D_4"	}; // FIXME: used how much? (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData texCoord3D5Element	= { VERTEX_ELEMENT_TEXCOORD3D_5,	12, 12,  8, "TEXCOORD3D_5"	}; // FIXME: used how much? (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData texCoord3D6Element	= { VERTEX_ELEMENT_TEXCOORD3D_6,	12, 12,  8, "TEXCOORD3D_6"	}; // FIXME: used how much? (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData texCoord3D7Element	= { VERTEX_ELEMENT_TEXCOORD3D_7,	12, 12,  8, "TEXCOORD3D_7"	}; // FIXME: used how much? (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData texCoord4D0Element	= { VERTEX_ELEMENT_TEXCOORD4D_0,	16, 16,  8, "TEXCOORD4D_0"	}; // FIXME: used how much? (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData texCoord4D1Element	= { VERTEX_ELEMENT_TEXCOORD4D_1,	16, 16,  8, "TEXCOORD4D_1"	}; // FIXME: used how much? (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData texCoord4D2Element	= { VERTEX_ELEMENT_TEXCOORD4D_2,	16, 16,  8, "TEXCOORD4D_2"	}; // FIXME: used how much? (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData texCoord4D3Element	= { VERTEX_ELEMENT_TEXCOORD4D_3,	16, 16,  8, "TEXCOORD4D_3"	}; // FIXME: used how much? (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData texCoord4D4Element	= { VERTEX_ELEMENT_TEXCOORD4D_4,	16, 16,  8, "TEXCOORD4D_4"	}; // FIXME: used how much? (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData texCoord4D5Element	= { VERTEX_ELEMENT_TEXCOORD4D_5,	16, 16,  8, "TEXCOORD4D_5"	}; // FIXME: used how much? (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData texCoord4D6Element	= { VERTEX_ELEMENT_TEXCOORD4D_6,	16, 16,  8, "TEXCOORD4D_6"	}; // FIXME: used how much? (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData texCoord4D7Element	= { VERTEX_ELEMENT_TEXCOORD4D_7,	16, 16,  8, "TEXCOORD4D_7"	}; // FIXME: used how much? (UNDONE: need vertex shader to take scale, account for clamping)
static constexpr ElementData elementTable[ VERTEX_ELEMENT_NUMELEMENTS ] = {	positionElement,
																		normalElement,
																		colorElement,
																		specularElement,
																		tangentSElement,
																		tangentTElement,
																		wrinkleElement,
																		boneIndexElement,
																		boneWeight1Element, boneWeight2Element, boneWeight3Element, boneWeight4Element,
																		userData1Element,   userData2Element,   userData3Element,   userData4Element,
																		texCoord1D0Element, texCoord1D1Element, texCoord1D2Element, texCoord1D3Element, texCoord1D4Element, texCoord1D5Element, texCoord1D6Element, texCoord1D7Element,
																		texCoord2D0Element, texCoord2D1Element, texCoord2D2Element, texCoord2D3Element, texCoord2D4Element, texCoord2D5Element, texCoord2D6Element, texCoord2D7Element,
																		texCoord3D0Element, texCoord3D1Element, texCoord3D2Element, texCoord3D3Element, texCoord3D4Element, texCoord3D5Element, texCoord3D6Element, texCoord3D7Element,
																		texCoord4D0Element, texCoord4D1Element, texCoord4D2Element, texCoord4D3Element, texCoord4D4Element, texCoord4D5Element, texCoord4D6Element, texCoord4D7Element,
																		};

static ConVar mem_vballocspew( "mem_vballocspew", "0", FCVAR_CHEAT, "How often to spew vertex buffer allocation stats - 1: every alloc, 2+: every 2+ allocs, 0: off" );

#endif // ENABLE_VB_ALLOC_TRACKER

//-----------------------------------------------------------------------------
// Singleton instance exposed to the engine
//-----------------------------------------------------------------------------

CVBAllocTracker g_VBAllocTrackerShaderAPI;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CVBAllocTracker, IVBAllocTracker, 
						VB_ALLOC_TRACKER_INTERFACE_VERSION, g_VBAllocTrackerShaderAPI );

//-----------------------------------------------------------------------------
//
// VB alloc-tracking code starts here
//
//-----------------------------------------------------------------------------

#if ENABLE_VB_ALLOC_TRACKER

UtlHashFixedHandle_t CVBAllocTracker::TrackAlloc( void * buffer, int bufferSize, VertexFormat_t fmt, int numVerts, unsigned allocatorHash )
{
	AllocData newData( buffer, bufferSize, fmt, numVerts, allocatorHash );
	auto handle = m_VBAllocTable.Insert( (intp)buffer, newData );
	if ( handle == m_VBAllocTable.InvalidHandle() )
	{
		Warning( "[VBMEM] VBMemAllocTable hash collision (grow table).\n" );
	}
	return handle;
}

bool CVBAllocTracker::KillAlloc( void * buffer, int & bufferSize, VertexFormat_t & fmt, int & numVerts, unsigned & allocatorHash )
{
	auto handle = m_VBAllocTable.Find( (intp)buffer );
	if ( handle != m_VBAllocTable.InvalidHandle() )
	{
		AllocData & data = m_VBAllocTable.Element( handle );
		bufferSize		= data.m_bufferSize;
		fmt				= data.m_fmt;
		numVerts		= data.m_numVerts;
		allocatorHash	= data.m_allocatorHash;
		m_VBAllocTable.Remove( handle );
		return true;
	}
	Warning( "[VBMEM] VBMemAllocTable failed to find alloc entry...\n" );
	return false;
}

UtlHashFixedHandle_t CVBAllocTracker::GetCounterHandle( const char * allocatorName, unsigned allocatorHash )
{
	auto handle = m_VBCountTable.Find( allocatorHash );
	if ( handle == m_VBCountTable.InvalidHandle() )
	{
		CounterData newData;
		Assert( !Q_isempty( allocatorName ) );
		V_strcpy_safe( newData.m_AllocatorName, allocatorName );
		handle = m_VBCountTable.Insert( allocatorHash, newData );
		m_VBTableNameHashes.Push( allocatorHash );
	}
	if ( handle == m_VBCountTable.InvalidHandle() )
	{
		Warning( "[VBMEM] CounterData hash collision (grow table).\n" );
	}
	return handle;
}

void CheckForElementTableUpdates( const ElementData & element )
{
	// Ensure that 'elementTable' gets updated if VertexElement_t ever changes:
	intp tableIndex = &element - &elementTable[0];
	Assert( tableIndex == element.element );
	if ( tableIndex != element.element )
	{
		static int timesToSpew = 20;
		if ( timesToSpew > 0 )
		{
			Warning( "VertexElement_t structure has changed, ElementData table in cvballoctracker needs updating!\n" );
			timesToSpew--;
		}
	}
}

void CVBAllocTracker::SpewElements( const char * allocatorName, unsigned nameHash )
{
	unsigned allocatorHash = allocatorName ? HashString( allocatorName ) : nameHash;
	auto handle = GetCounterHandle( allocatorName, allocatorHash );
	if ( handle != m_VBCountTable.InvalidHandle() )
	{
		CounterData & data = m_VBCountTable.Element( handle );
		intp originalSum = 0, currentSum = 0, idealSum = 0;
		for (int i = 0;i < VERTEX_ELEMENT_NUMELEMENTS;i++)
		{
			CheckForElementTableUpdates( elementTable[ i ] );
			const int numCompressed = data.m_elementsCompressed[ i ];
			const int numUncompressed = data.m_elementsUncompressed[ i ];
			const int numVerts = numCompressed + numUncompressed;
			originalSum += numVerts*elementTable[ i ].uncompressed;
			currentSum  += numCompressed*elementTable[ i ].currentCompressed + numUncompressed*elementTable[ i ].uncompressed;
			idealSum    += numVerts*elementTable[ i ].idealCompressed;
		}

		if ( originalSum > 0 )
		{
			Msg( "[VBMEM]   ----elements (%s)----:\n", data.m_AllocatorName);
			for (int i = 0;i < VERTEX_ELEMENT_NUMELEMENTS;i++)
			{
				// We count vertices (converted to bytes via elementTable)
				int numCompressed = data.m_elementsCompressed[ i ];
				int numUncompressed = data.m_elementsUncompressed[ i ];
				int numVerts = numCompressed + numUncompressed;
				const ElementData & elementData = elementTable[ i ];
				if ( numVerts > 0 )
				{
					Msg( "              element:  %s 'U', %s 'C', %s 'I', %6.2f MiB 'D', %s\n",
						V_pretifymem( numVerts*elementData.uncompressed, 2, true ),
						V_pretifymem( numCompressed*elementData.currentCompressed + numUncompressed*elementData.uncompressed, 2, true ),
						V_pretifymem( numVerts*elementData.idealCompressed, 2, true ),
						-( numCompressed*elementData.currentCompressed + numUncompressed*elementData.uncompressed - numVerts*elementData.idealCompressed ) / ( 1024.0f*1024.0f ),
						elementData.name );
				}
			}
			Msg( "[VBMEM]         total:  %s 'U', %s 'C', %s 'I', %6.2f MiB 'D'\n",
				V_pretifymem( originalSum, 2, true ),
				V_pretifymem( currentSum, 2, true ),
				V_pretifymem( idealSum, 2, true ),
				-( currentSum - idealSum ) / ( 1024.0f*1024.0f ) );
			Msg( "[VBMEM]   ----elements (%s)----:\n", data.m_AllocatorName);
		}
	}
}

int CVBAllocTracker::ComputeVertexSize( VertexElementMap_t map, VertexFormat_t fmt, bool compressed )
{
	int vertexSize = 0;
	for ( int i = 0;i < VERTEX_ELEMENT_NUMELEMENTS;i++ )
	{
		const ElementData & element = elementTable[ i ];
		CheckForElementTableUpdates( element );
		VertexElementMap_t LSB = 1;
		if ( map & ( LSB << i ) )
		{
			vertexSize += compressed ? element.currentCompressed : element.uncompressed;
		}
	}

	// On PC (see CVertexBufferBase::ComputeVertexDescription() in meshbase.cpp)
	// vertex strides are aligned to 16 bytes:
	bool bCacheAlign = ( fmt & VERTEX_FORMAT_USE_EXACT_FORMAT ) == 0;
	if ( bCacheAlign && ( vertexSize > 16 ) && IsPC() )
	{
		vertexSize = AlignValue(vertexSize, 16);
	}

	return vertexSize;
}

VertexElementMap_t CVBAllocTracker::ComputeElementMap( VertexFormat_t fmt, int vertexSize, bool isDynamic )
{
	VertexElementMap_t map = 0, LSB = 1;
	if ( fmt & VERTEX_POSITION	) map |= LSB <<   VERTEX_ELEMENT_POSITION;
	if ( fmt & VERTEX_NORMAL	) map |= LSB <<   VERTEX_ELEMENT_NORMAL;
	if ( fmt & VERTEX_COLOR		) map |= LSB <<   VERTEX_ELEMENT_COLOR;
	if ( fmt & VERTEX_SPECULAR	) map |= LSB <<   VERTEX_ELEMENT_SPECULAR;
	if ( fmt & VERTEX_TANGENT_S	) map |= LSB <<   VERTEX_ELEMENT_TANGENT_S;
	if ( fmt & VERTEX_TANGENT_T	) map |= LSB <<   VERTEX_ELEMENT_TANGENT_T;
	if ( fmt & VERTEX_WRINKLE	) map |= LSB <<   VERTEX_ELEMENT_WRINKLE;
	if ( fmt & VERTEX_BONE_INDEX) map |= LSB <<   VERTEX_ELEMENT_BONEINDEX;
	int numBones = NumBoneWeights( fmt );
	if ( numBones > 0			) map |= LSB << ( VERTEX_ELEMENT_BONEWEIGHTS1 + numBones - 1 );
	int userDataSize = UserDataSize( fmt );
	if ( userDataSize > 0		) map |= LSB << ( VERTEX_ELEMENT_USERDATA1 + userDataSize - 1 );
	for ( int i = 0; i < VERTEX_MAX_TEXTURE_COORDINATES; ++i )
	{
		VertexElement_t texCoordElements[4] = { VERTEX_ELEMENT_TEXCOORD1D_0, VERTEX_ELEMENT_TEXCOORD2D_0, VERTEX_ELEMENT_TEXCOORD3D_0, VERTEX_ELEMENT_TEXCOORD4D_0 };
		int nCoordSize = TexCoordSize( i, fmt );
		if ( nCoordSize > 0 )
		{
			Assert( i < 4 );
			if ( i < 4 )
			{
				map |= LSB << ( texCoordElements[ nCoordSize - 1 ] + i );
			}
		}
	}

	if ( map == 0 )
	{
		if ( !isDynamic )
		{
			// We expect all (non-dynamic) VB allocs to specify a vertex format
			// Warning("[VBMEM] unknown vertex format\n");
			return 0;
		}
	}
	else
	{
		if ( vertexSize != 0 )
		{
			// Make sure elementTable above matches external computations of vertex size
			// FIXME: make this assert dependent on whether the current VB is compressed or not
			VertexCompressionType_t compressionType = CompressionType( fmt );
			bool isCompressedAlloc = ( compressionType == VERTEX_COMPRESSION_ON );
			// FIXME: once we've finalised which elements we're compressing for ship, update
			//        elementTable to reflect that and re-enable this assert for compressed verts
			if ( !isCompressedAlloc )
			{
				Assert( vertexSize == ComputeVertexSize( map, fmt, isCompressedAlloc ) );
			}
		}
	}

	return map;
}

void CVBAllocTracker::UpdateElements( CounterData & data, VertexFormat_t fmt, int numVerts, int vertexSize,
									bool isDynamic, bool isCompressed )
{
	VertexElementMap_t map = ComputeElementMap( fmt, vertexSize, isDynamic );
	if ( map != 0 )
	{
		auto compressed = isCompressed ? data.m_elementsCompressed : data.m_elementsUncompressed;
		for (int i = 0;i < VERTEX_ELEMENT_NUMELEMENTS;i++)
		{
			// Count vertices (get bytes from our elements table)
			VertexElementMap_t LSB = 1;
			if ( map & ( LSB << i ) )
			{
				compressed[ i ] += numVerts;
			}
		}
	}
}

template <Saving_t savingType>
void CVBAllocTracker::AddSaving( intp & alreadySaved, intp & yetToSave, const char *allocatorName, VertexElement_t element )
{
	auto handle = GetCounterHandle( allocatorName, HashString( allocatorName ) );
	if ( handle != m_VBCountTable.InvalidHandle() )
	{
		CheckForElementTableUpdates( elementTable[ element ] );
		CounterData & counterData = m_VBCountTable.Element( handle );
		const ElementData & elementData = elementTable[ element ];
		int numVerts        = counterData.m_vertCount;
		int numCompressed   = counterData.m_elementsCompressed[ element ];
		int numUncompressed = counterData.m_elementsUncompressed[ element ];

		if constexpr (savingType == Saving_t::SAVING_COMPRESSION)
		{
			alreadySaved 	+= numCompressed*(   elementData.uncompressed - elementData.currentCompressed );
			yetToSave    	+= numUncompressed*( elementData.uncompressed - elementData.currentCompressed );
		}
		else if constexpr (savingType == Saving_t::SAVING_REMOVAL)
		{
			alreadySaved	+= elementData.uncompressed*( numVerts - ( numUncompressed + numCompressed ) );
			yetToSave		+= numUncompressed*elementData.uncompressed + numCompressed*elementData.uncompressed;
		}
		else
		{
			static_assert(false, "Unknown saving type.");
		}
	}
}

void CVBAllocTracker::SpewExpectedSavings( void )
{
	intp alreadySaved = 0, yetToSave = 0;

	// We have removed bone weights+indices from static props
	AddSaving<Saving_t::SAVING_REMOVAL>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (prop_static)",		VERTEX_ELEMENT_BONEWEIGHTS2 );
	AddSaving<Saving_t::SAVING_REMOVAL>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (prop_static)",		VERTEX_ELEMENT_BONEINDEX );
	// We have removed vertex colors from all models (color should only ever be in stream1, for static vertex lighting)
	AddSaving<Saving_t::SAVING_REMOVAL>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (prop_dynamic)",		VERTEX_ELEMENT_COLOR );
	AddSaving<Saving_t::SAVING_REMOVAL>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (prop_static)",		VERTEX_ELEMENT_COLOR );
	AddSaving<Saving_t::SAVING_REMOVAL>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (character)",			VERTEX_ELEMENT_COLOR );
																					
	// We expect to compress texcoords (DONE: normals+tangents, boneweights) for all studiomdls
	AddSaving<Saving_t::SAVING_COMPRESSION>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (prop_dynamic)",	VERTEX_ELEMENT_NORMAL	 );
	AddSaving<Saving_t::SAVING_COMPRESSION>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (prop_static)",		VERTEX_ELEMENT_NORMAL	 );
	AddSaving<Saving_t::SAVING_COMPRESSION>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (character)",		VERTEX_ELEMENT_NORMAL	 );
	AddSaving<Saving_t::SAVING_COMPRESSION>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (prop_dynamic)",	VERTEX_ELEMENT_USERDATA4 );
	AddSaving<Saving_t::SAVING_COMPRESSION>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (prop_static)",		VERTEX_ELEMENT_USERDATA4 );
	AddSaving<Saving_t::SAVING_COMPRESSION>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (character)",		VERTEX_ELEMENT_USERDATA4 );
	AddSaving<Saving_t::SAVING_COMPRESSION>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (prop_dynamic)",	VERTEX_ELEMENT_TEXCOORD2D_0 );
	AddSaving<Saving_t::SAVING_COMPRESSION>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (prop_static)",		VERTEX_ELEMENT_TEXCOORD2D_0 );
	AddSaving<Saving_t::SAVING_COMPRESSION>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (character)",		VERTEX_ELEMENT_TEXCOORD2D_0 );
	AddSaving<Saving_t::SAVING_COMPRESSION>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (character)",		VERTEX_ELEMENT_BONEWEIGHTS1 );
	AddSaving<Saving_t::SAVING_COMPRESSION>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (character)",		VERTEX_ELEMENT_BONEWEIGHTS2 );
	AddSaving<Saving_t::SAVING_COMPRESSION>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (prop_dynamic)",	VERTEX_ELEMENT_BONEWEIGHTS1 );
	AddSaving<Saving_t::SAVING_COMPRESSION>( alreadySaved, yetToSave, "R_StudioCreateStaticMeshes (prop_dynamic)",	VERTEX_ELEMENT_BONEWEIGHTS2 );

	// UNDONE: compress bone weights for studiomdls?			(issue: possible flex artifacts, but 2xSHORTN probably ok)
	// UNDONE: compress positions (+wrinkle) for studiomdls?	(issue: possible flex artifacts)
	// UNDONE: disable tangents for non-bumped models			(issue: forcedmaterialoverride support... don't think that needs tangents, though
	//                                                                  however, if we use UBYTE4 normal+tangent encoding, removing tangents saves nothing)

	Msg("[VBMEM]\n");
	Msg("[VBMEM] Total expected memory saving by disabling/compressing vertex elements: %s\n", V_pretifymem( yetToSave, 2, true ) );
	Msg("[VBMEM] ( total memory already saved: %s)\n", V_pretifymem( alreadySaved, 2, true ) );
	Msg("[VBMEM]  - compression of model texcoords, [DONE: normals+tangents, bone weights]\n" );
	Msg("[VBMEM]  - avoidance of 4-KiB alignment wastage for color meshes (on 360)\n" );
	Msg("[VBMEM]  - [DONE: removal of unneeded bone weights+indices on models]\n" );
	Msg("[VBMEM]\n");
}

void CVBAllocTracker::UpdateData( const char * allocatorName, unsigned allocatorKey, int bufferSize, VertexFormat_t fmt,
								 int numVerts, int vertexSize, bool isDynamic, bool isCompressed )
{
	auto handle = GetCounterHandle( allocatorName, allocatorKey );
	if ( handle != m_VBCountTable.InvalidHandle() )
	{
		CounterData & data = m_VBCountTable.Element( handle );
		data.m_memCount += bufferSize;
		Assert( data.m_memCount  >= 0 );
		data.m_vertCount += numVerts;
		Assert( data.m_vertCount  >= 0 );
		UpdateElements( data, fmt, numVerts, vertexSize, isDynamic, isCompressed );
	}
}

const char * CVBAllocTracker::GetNameString( int allocatorKey )
{
	auto handle = GetCounterHandle( NULL, allocatorKey );
	if ( handle != m_VBCountTable.InvalidHandle() )
	{
		CounterData & data = m_VBCountTable.Element( handle );
		return data.m_AllocatorName;
	}
	return "null";
}

void CVBAllocTracker::SpewData( const char * allocatorName, unsigned nameHash )
{
	unsigned allocatorHash = allocatorName ? HashString( allocatorName ) : nameHash;
	auto handle = GetCounterHandle( allocatorName, allocatorHash );
	if ( handle != m_VBCountTable.InvalidHandle() )
	{
		CounterData & data = m_VBCountTable.Element( handle );
		if ( data.m_memCount > 0 )
		{
			Msg("[VBMEM]    running mem usage: (%s verts) %s | '%s'\n",
				V_pretifynum( data.m_vertCount ),
				V_pretifymem( data.m_memCount, 2, true ),
				data.m_AllocatorName );
		}
	}
}

void CVBAllocTracker::SpewDataSometimes( int inc )
{
	static int count = 0;

	if ( inc < 0 ) count += inc;
	Assert( count >= 0 );

	int period = mem_vballocspew.GetInt();
	if ( period >= 1 )
	{
		if ( ( count % period ) == 0 )
		{
			Msg( "[VBMEM]    Status after %d VB allocs:\n", count );
			//#define ROUND_UP( _x_ ) ( ( ( _x_ ) + 31 ) & 31 )
			//Msg( "[VBMEM]    Conservative estimate of mem used to track allocs: %d\n", 4096*ROUND_UP( 4 + sizeof( CUtlPtrLinkedList<AllocData> ) ) + count*ROUND_UP( sizeof( AllocData ) + 8 ) );
			SpewData( "total_static" );
			SpewData( "unknown"      );
		}
	}

	if ( inc > 0 ) count += inc;
}

void CVBAllocTracker::DumpVBAllocs()
{
	AUTO_LOCK_D( m_VBAllocMutex, 10 );

	Msg("[VBMEM] ----running totals----\n" );
	for ( intp i = m_VBTableNameHashes.Count() - 1; i >= 0; i-- )
	{
		unsigned nameHash = m_VBTableNameHashes.Element( i );
		SpewElements( NULL, nameHash );
	}

	Msg("[VBMEM]\n");
	Msg("[VBMEM]   'U' - original memory usage (all vertices uncompressed)\n" );
	Msg("[VBMEM]   'C' - current memory usage (some compression)\n" );
	Msg("[VBMEM]   'I' - ideal memory usage (all verts maximally compressed)\n" );
	Msg("[VBMEM]   'D' - difference between C and I (-> how much more compression could save)\n" );
	Msg("[VBMEM]   'W' - memory wasted due to 4-KiB vertex buffer alignment\n" );
	Msg("[VBMEM]\n");
	for ( intp i = m_VBTableNameHashes.Count() - 1; i >= 0; i-- )
	{
		unsigned nameHash = m_VBTableNameHashes.Element( i );
		SpewData( NULL, nameHash );
	}
	SpewExpectedSavings();
	Msg("[VBMEM] ----running totals----\n" );
}

#endif // ENABLE_VB_ALLOC_TRACKER

void CVBAllocTracker::CountVB( void * buffer, bool isDynamic, int bufferSize, int vertexSize, VertexFormat_t fmt )
{
#if ENABLE_VB_ALLOC_TRACKER
	AUTO_LOCK_D( m_VBAllocMutex, 10 );

	// Update VB memory counts for the relevant allocation type
	// (NOTE: we have 'unknown', 'dynamic' and 'total' counts)
	const char * allocatorName = Q_isempty( m_MeshAllocatorName ) ? "unknown" : m_MeshAllocatorName;
	if ( isDynamic ) allocatorName = "total_dynamic";

	int          numVerts = vertexSize > 0 ? bufferSize / vertexSize : 0;
	const unsigned     totalStaticKey = HashString( "total_static" );
	const unsigned     key = HashString( allocatorName );
	bool         isCompressed = ( VERTEX_COMPRESSION_NONE != CompressionType( fmt ) );

	if ( Q_isempty( m_MeshAllocatorName ) )
	{
		Warning("[VBMEM] unknown allocation!\n");
	}

	// Add to the VB memory counters
	TrackAlloc( buffer, bufferSize, fmt, numVerts, key );
	if ( !isDynamic )
	{
		// Keep dynamic allocs out of the total (dynamic VBs don't get compressed)
		UpdateData( "total_static", totalStaticKey, bufferSize, fmt, numVerts, vertexSize, isDynamic, isCompressed );
	}
	UpdateData( allocatorName, key, bufferSize, fmt, numVerts, vertexSize, isDynamic, isCompressed );

	if ( m_bSuperSpew )
	{
		// Spew every alloc
		Msg( "[VBMEM] VB-alloc  | %s | %s | %s\n", V_pretifymem( bufferSize ), ( isDynamic ? "DYNamic" : " STAtic" ), allocatorName );
		SpewData( allocatorName );
	}
	SpewDataSometimes( +1 );
#endif // ENABLE_VB_ALLOC_TRACKER
}

void CVBAllocTracker::UnCountVB( void * buffer )
{
#if ENABLE_VB_ALLOC_TRACKER
	AUTO_LOCK_D( m_VBAllocMutex, 10 );

	int bufferSize;
	VertexFormat_t fmt;
	int numVerts;
	unsigned key;

	// We have to store allocation data because the caller often doesn't know what it alloc'd :o/
	if ( KillAlloc( buffer, bufferSize, fmt, numVerts, key ) )
	{
		const unsigned totalKey   = HashString( "total_static" );
		const unsigned dynamicKey = HashString( "total_dynamic" );

		const bool isCompressed	= ( VERTEX_COMPRESSION_NONE != CompressionType( fmt ) );
		const bool isDynamic		= ( key == dynamicKey );

		// Subtract from the VB memory counters
		if ( !isDynamic )
		{
			UpdateData( NULL, totalKey, -bufferSize, fmt, -numVerts, 0, isDynamic, isCompressed );
		}
		UpdateData( NULL, key, -bufferSize, fmt, -numVerts, 0, isDynamic, isCompressed );

		const char * nameString = GetNameString( key );

		if ( m_bSuperSpew )
		{
			Msg( "[VBMEM] VB-free   | %s | %s | %s\n", V_pretifymem( bufferSize ), ( isDynamic ? "DYNamic" : " STAtic" ), nameString );
			SpewData( nameString );
		}
		SpewDataSometimes( -1 );
	}
#endif // ENABLE_VB_ALLOC_TRACKER
}

bool CVBAllocTracker::TrackMeshAllocations( const char * allocatorName )
{
#if ENABLE_VB_ALLOC_TRACKER
	// Tracks mesh allocations by name (set this before an alloc, clear it after)

	if ( !Q_isempty( m_MeshAllocatorName ) )
	{
		return true;
	}

	AUTO_LOCK_D( m_VBAllocMutex, 10 );

	if ( allocatorName )
	{
		Assert( Q_isempty( m_MeshAllocatorName ) );
		V_strcpy_safe( m_MeshAllocatorName, allocatorName );
	}
	else
	{
		m_MeshAllocatorName[0] = '\0';
	}
#endif // ENABLE_VB_ALLOC_TRACKER

	return false;
}

#ifndef RETAIL

static void CC_DumpVBMemAllocs()
{
#if ( ENABLE_VB_ALLOC_TRACKER == 0 )
	Warning( "ENABLE_VB_ALLOC_TRACKER must be 1 to enable VB mem alloc tracking\n");
#else
	g_VBAllocTrackerShaderAPI.DumpVBAllocs();
#endif
}

static ConCommand mem_dumpvballocs( "mem_dumpvballocs", CC_DumpVBMemAllocs, "Dump VB memory allocation stats.", FCVAR_CHEAT );

#endif // RETAIL
