//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Vertex/Pixel Shaders
//
//===========================================================================//
#define DISABLE_PROTECTED_THINGS
#ifdef _WIN32
#elif POSIX
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#define closesocket close
#define WSAGetLastError() errno
#undef SOCKET
typedef int SOCKET;
#define SOCKET_ERROR (-1)
#define SD_SEND 0x01
#define INVALID_SOCKET (~0)
#endif

#include "togl/rendermechanism.h"

#include "tier1/utlsymbol.h"
#include "tier1/utlvector.h"
#include "tier1/utldict.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlbuffer.h"
#include "tier1/UtlStringMap.h"
#include "tier1/lzmaDecoder.h"
#include "tier1/utlmap.h"
#include "tier1/KeyValues.h"
#include "tier1/convar.h"
#include "tier1/diff.h"
#include "tier1/checksum_crc.h"

#include "tier0/fasttimer.h"
#include "tier0/vprof.h"
#include "tier0/icommandline.h"
#include "tier0/dbg.h"

#include "tier2/tier2.h"

#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/IShader.h"
#include "materialsystem/shader_vcs_version.h"

#include "vertexshaderdx8.h"
#include "locald3dtypes.h"
#include "recording.h"
#include "IShaderSystem.h"
#include "shaderdevicedx8.h"
#include "shaderapidx8.h"
#include "shaderapidx8_global.h"
#include "filesystem.h"

#include "datacache/idatacache.h"
#include "filesystem/IQueuedLoader.h"
#include "shaderapi/ishaderutil.h"

#include "Color.h"

#ifdef REMOTE_DYNAMIC_SHADER_COMPILE

# if defined (POSIX)

#  include <sys/types.h>
#  include <sys/socket.h>

# else

#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <system_error>

# endif

#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


// It currently includes windows.h and we don't want that.
#ifdef USE_ACTUAL_DX

#include <d3dcompiler.h>
#include "bzip2/bzlib.h"
#include "com_ptr.h"
#include "windows/com_error_category.h"

#else

int BZ2_bzBuffToBuffDecompress( 
      char*         dest, 
      unsigned int* destLen,
      char*         source, 
      unsigned int  sourceLen,
      int           small, 
      int           verbosity 
   )
{
	return 0;
}

#endif

static ConVar mat_remoteshadercompile( "mat_remoteshadercompile", "127.0.0.1", FCVAR_CHEAT, "Remote shader compiler server IP address" );
// dimhotepus: Allow to select remote shader compiler port.
static ConVar mat_remoteshadercompile_port( "mat_remoteshadercompile_port", "20000", FCVAR_CHEAT, "Remote shader compiler server port", true, 1024, true, 65535 );

//#define PROFILE_SHADER_CREATE

//#define NO_AMBIENT_CUBE
#define MAX_BONES 3

// debugging aid
#define MAX_SHADER_HISTORY	16

#define SHADER_FNAME_EXTENSION	".vcs"

#ifdef DYNAMIC_SHADER_COMPILE
volatile static char s_ShaderCompileString[]="dynamic_shader_compile_is_on";
#endif

#ifdef DYNAMIC_SHADER_COMPILE
static void MatFlushShaders( void );
#endif

// D3D to OpenGL translator
//static D3DToGL_ASM sg_D3DToOpenGLTranslator;	// Remove the _ASM to switch to the new translator
//static D3DToGL sg_NewD3DToOpenGLTranslator;		// Remove the _ASM to switch to the new translator

#ifdef PROFILE_SHADER_CREATE
static FILE *GetDebugFileHandle( void )
{
	static FILE *fp = NULL;
	if( !fp )
	{
		fp = fopen( "shadercreate.txt", "w" );
		Assert( fp );
	}
	return fp;
}
#endif // PROFILE_SHADER_CREATE

#ifdef DX_TO_GL_ABSTRACTION
	// mat_autoload_glshaders instructs the engine to load a cached shader table at startup
	// it will try for glshaders.cfg first, then fall back to glbaseshaders.cfg if not found
ConVar mat_autoload_glshaders( "mat_autoload_glshaders", "1" );

	// mat_autosave_glshaders instructs the engine to save out the shader table at key points
	// to the filename glshaders.cfg
	//
ConVar mat_autosave_glshaders( "mat_autosave_glshaders", "1" );


#endif
//-----------------------------------------------------------------------------
// Explicit instantiation of shader buffer implementation
//-----------------------------------------------------------------------------
template class CShaderBuffer<ID3DBlob>;


//-----------------------------------------------------------------------------
// Used to find unique shaders
//-----------------------------------------------------------------------------
#ifdef MEASURE_DRIVER_ALLOCATIONS
static CUtlMap< CRC32_t, int, int > s_UniqueVS( 0, 0, DefLessFunc( CRC32_t ) );
static CUtlMap< CRC32_t, int, int > s_UniquePS( 0, 0, DefLessFunc( CRC32_t ) );
static CUtlMap< se::win::com::com_ptr<IDirect3DVertexShader9>, CRC32_t, int > s_VSLookup( (intp)0, (intp)0, DefLessFunc( se::win::com::com_ptr<IDirect3DVertexShader9> ) );
static CUtlMap< se::win::com::com_ptr<IDirect3DPixelShader9>, CRC32_t, int > s_PSLookup( (intp)0, (intp)0, DefLessFunc( se::win::com::com_ptr<IDirect3DPixelShader9> ) );
#endif

static int s_NumPixelShadersCreated = 0;
static int s_NumVertexShadersCreated = 0;

static void RegisterVS( const void* pShaderBits, intp nShaderSize, const se::win::com::com_ptr<IDirect3DVertexShader9> &pShader )
{
#ifdef MEASURE_DRIVER_ALLOCATIONS
	CRC32_t crc;
	CRC32_Init( &crc );
	CRC32_ProcessBuffer( &crc, pShaderBits, nShaderSize );
	CRC32_Final( &crc );

	s_VSLookup.Insert( pShader, crc );

	auto nIndex = s_UniqueVS.Find( crc );
	if ( nIndex != s_UniqueVS.InvalidIndex() )
	{
		++s_UniqueVS[nIndex];
	}
	else
	{
		int nMemUsed = 23 * 1024;
		s_UniqueVS.Insert( crc, 1 );
		VPROF_INCREMENT_GROUP_COUNTER( "unique vs count", COUNTER_GROUP_NO_RESET, 1 );
		VPROF_INCREMENT_GROUP_COUNTER( "vs driver mem", COUNTER_GROUP_NO_RESET, nMemUsed );
		VPROF_INCREMENT_GROUP_COUNTER( "total driver mem", COUNTER_GROUP_NO_RESET, nMemUsed );
	}
#endif
}

static void RegisterPS( const void* pShaderBits, intp nShaderSize, const se::win::com::com_ptr<IDirect3DPixelShader9> &pShader )
{
#ifdef MEASURE_DRIVER_ALLOCATIONS
	CRC32_t crc;
	CRC32_Init( &crc );
	CRC32_ProcessBuffer( &crc, pShaderBits, nShaderSize );
	CRC32_Final( &crc );

	s_PSLookup.Insert( pShader, crc );

	auto nIndex = s_UniquePS.Find( crc );
	if ( nIndex != s_UniquePS.InvalidIndex() )
	{
		++s_UniquePS[nIndex];
	}
	else
	{
		int nMemUsed = 400;
		s_UniquePS.Insert( crc, 1 );
		VPROF_INCREMENT_GROUP_COUNTER( "unique ps count", COUNTER_GROUP_NO_RESET, 1 );
		VPROF_INCREMENT_GROUP_COUNTER( "ps driver mem", COUNTER_GROUP_NO_RESET, nMemUsed );
		VPROF_INCREMENT_GROUP_COUNTER( "total driver mem", COUNTER_GROUP_NO_RESET, nMemUsed );
	}
#endif
}

static void UnregisterVS( const se::win::com::com_ptr<IDirect3DVertexShader9> &pShader )
{
#ifdef MEASURE_DRIVER_ALLOCATIONS
	auto nCRCIndex = s_VSLookup.Find( pShader );
	if ( nCRCIndex == s_VSLookup.InvalidIndex() )
		return;

	CRC32_t crc = s_VSLookup[nCRCIndex];
	s_VSLookup.RemoveAt( nCRCIndex );

	auto nIndex = s_UniqueVS.Find( crc );
	if ( nIndex != s_UniqueVS.InvalidIndex() )
	{
		if ( --s_UniqueVS[nIndex] <= 0 )
		{
			int nMemUsed = 23 * 1024;
			VPROF_INCREMENT_GROUP_COUNTER( "unique vs count", COUNTER_GROUP_NO_RESET, -1 );
			VPROF_INCREMENT_GROUP_COUNTER( "vs driver mem", COUNTER_GROUP_NO_RESET, -nMemUsed );
			VPROF_INCREMENT_GROUP_COUNTER( "total driver mem", COUNTER_GROUP_NO_RESET, -nMemUsed );
			s_UniqueVS.Remove( nIndex );
		}
	}
#endif
}

static void UnregisterPS( const se::win::com::com_ptr<IDirect3DPixelShader9> &pShader )
{
#ifdef MEASURE_DRIVER_ALLOCATIONS
	auto nCRCIndex = s_PSLookup.Find( pShader );
	if ( nCRCIndex == s_PSLookup.InvalidIndex() )
		return;

	CRC32_t crc = s_PSLookup[nCRCIndex];
	s_PSLookup.RemoveAt( nCRCIndex );

	auto nIndex = s_UniquePS.Find( crc );
	if ( nIndex != s_UniquePS.InvalidIndex() )
	{
		if ( --s_UniquePS[nIndex] <= 0 )
		{
			int nMemUsed = 400;
			VPROF_INCREMENT_GROUP_COUNTER( "unique ps count", COUNTER_GROUP_NO_RESET, -1 );
			VPROF_INCREMENT_GROUP_COUNTER( "ps driver mem", COUNTER_GROUP_NO_RESET, -nMemUsed );
			VPROF_INCREMENT_GROUP_COUNTER( "total driver mem", COUNTER_GROUP_NO_RESET, -nMemUsed );
			s_UniquePS.Remove( nIndex );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// The lovely low-level dx call to create a vertex shader
//-----------------------------------------------------------------------------
static HardwareShader_t CreateD3DVertexShader( const DWORD *pByteCode, intp numBytes, const char *pShaderName, char *debugLabel = NULL )
{
	MEM_ALLOC_D3D_CREDIT();

	if ( !pByteCode )
	{
		Assert(false);
		return INVALID_HARDWARE_SHADER;
	}

	// Compute the vertex specification
	se::win::com::com_ptr<IDirect3DVertexShader9> shader;

#ifdef DX_TO_GL_ABSTRACTION
	HRESULT hr = Dx9Device()->CreateVertexShader( pByteCode, &shader, pShaderName, debugLabel );
#else
	HRESULT hr = Dx9Device()->CreateVertexShader( pByteCode, &shader );
#endif

	// NOTE: This isn't recorded before the CreateVertexShader because
	// we don't know the value of shader until after the CreateVertexShader.
	RECORD_COMMAND( DX8_CREATE_VERTEX_SHADER, 3 );
	RECORD_PTR( hShader ); // hack hack hack
	RECORD_INT( numBytes );
	RECORD_STRUCT( pByteCode, numBytes );

	if ( SUCCEEDED( hr ) )
	{
		s_NumVertexShadersCreated++;
		RegisterVS( pByteCode, numBytes, shader );

		return shader.Detach();
	}
	
	Assert(false);
	Warning( __FUNCTION__ ": IDirect3DDevice9Ex::CreateVertexShader(function = 0x%p) failed w/e %s.\n",
		pByteCode,
		se::win::com::com_error_category().message(hr).c_str() );

	return INVALID_HARDWARE_SHADER;
}

static void PatchPixelShaderForAtiMsaaHack(DWORD *pShader, DWORD dwTexCoordMask) 
{ 
	bool bIsSampler, bIsTexCoord; 
		
	// Should be able to patch only ps2.0 
	if (*pShader != 0xFFFF0200) 
		return; 
		
	pShader++; 
		
	while (pShader) 
	{ 
		switch (*pShader & D3DSI_OPCODE_MASK) 
		{ 
		case D3DSIO_COMMENT: 
			// Process comment 
			pShader = pShader + (*pShader >> 16) + 1; 
			break; 
				
		case D3DSIO_END: 
			// End of shader 
			return; 
				
		case D3DSIO_DCL: 
			bIsSampler = (*(pShader + 1) & D3DSP_TEXTURETYPE_MASK) != D3DSTT_UNKNOWN; 
			bIsTexCoord = (((*(pShader + 2) & D3DSP_REGTYPE_MASK) >> D3DSP_REGTYPE_SHIFT) + 
				((*(pShader + 2) & D3DSP_REGTYPE_MASK2) >> D3DSP_REGTYPE_SHIFT2)) == D3DSPR_TEXTURE; 
				
			if (!bIsSampler && bIsTexCoord) 
			{ 
				DWORD dwTexCoord = *(pShader + 2) & D3DSP_REGNUM_MASK; 
				DWORD mask = 0x01; 
				for (DWORD i = 0; i < 16; i++) 
				{ 
					if (((dwTexCoordMask & mask) == mask) && (dwTexCoord == i)) 
					{ 
						// If found -- patch and get out 
//						*(pShader + 2) |= D3DSPDM_PARTIALPRECISION; 
						*(pShader + 2) |= D3DSPDM_MSAMPCENTROID; 
						break; 
					} 
					mask <<= 1; 
				} 
			} 
			// Intentionally fall through...
			[[fallthrough]];
				
		default: 
			// Skip instruction 
			pShader = pShader + ((*pShader & D3DSI_INSTLENGTH_MASK) >> D3DSI_INSTLENGTH_SHIFT) + 1; 
		} 
	}
} 

static ConVar mat_force_ps_patch( "mat_force_ps_patch", "0" );
static ConVar mat_disable_ps_patch( "mat_disable_ps_patch", "0", FCVAR_ALLOWED_IN_COMPETITIVE );

//-----------------------------------------------------------------------------
// The lovely low-level dx call to create a pixel shader
//-----------------------------------------------------------------------------
static HardwareShader_t CreateD3DPixelShader( DWORD *pByteCode, unsigned int nCentroidMask, intp numBytes, const char* pShaderName, char *debugLabel = NULL )
{
	MEM_ALLOC_D3D_CREDIT();
	
	if ( !pByteCode )
		return INVALID_HARDWARE_SHADER;

	if ( IsPC() && nCentroidMask && 
		( HardwareConfig()->NeedsATICentroidHack()  ||
		  mat_force_ps_patch.GetInt() ) )
	{
		if ( !mat_disable_ps_patch.GetInt() )
		{
			PatchPixelShaderForAtiMsaaHack( pByteCode, nCentroidMask );
		}
	}

	se::win::com::com_ptr<IDirect3DPixelShader> shader;

#if defined( DX_TO_GL_ABSTRACTION ) 
#if defined( OSX ) 
	const HRESULT hr = Dx9Device()->CreatePixelShader( pByteCode, &shader, pShaderName, debugLabel );
#else
	const HRESULT hr = Dx9Device()->CreatePixelShader( pByteCode, &shader, pShaderName, debugLabel, &nCentroidMask );
#endif
#else
	const HRESULT hr = Dx9Device()->CreatePixelShader( pByteCode, &shader );
#endif
	
	// NOTE: We have to do this after creating the pixel shader since we don't know
	// lookup.m_PixelShader yet!!!!!!!
	RECORD_COMMAND( DX8_CREATE_PIXEL_SHADER, 3 );
	RECORD_PTR( shader );  // hack hack hack
	RECORD_INT( numBytes );
	RECORD_STRUCT( pByteCode, numBytes );
	
	if ( SUCCEEDED( hr ) )
	{
		s_NumPixelShadersCreated++;
		RegisterPS( pByteCode, numBytes, shader );

		return shader.Detach();
	}
	
	Assert(false);
	Warning( __FUNCTION__ ": IDirect3DDevice9Ex::CreatePixelShader(function = 0x%p) failed w/e %s.\n",
		pByteCode,
		se::win::com::com_error_category().message(hr).c_str() );

	return INVALID_HARDWARE_SHADER;
}

template<class T> intp BinarySearchCombos( uint32 nStaticComboID, intp nCombos, T const *pRecords )
{
	// Use binary search - data is sorted
	intp nLowerIdx = 1;
	intp nUpperIdx = nCombos;
	for (;;)
	{
		if ( nUpperIdx < nLowerIdx )
			return -1;

		intp nMiddleIndex = ( nLowerIdx + nUpperIdx ) / 2;
		uint32 nProbe = pRecords[nMiddleIndex-1].m_nStaticComboID;
		if ( nStaticComboID < nProbe )
		{
			nUpperIdx = nMiddleIndex - 1;
		}
		else
		{
			if ( nStaticComboID > nProbe )
				nLowerIdx = nMiddleIndex + 1;
			else
				return nMiddleIndex - 1;
		}
	}
}

inline intp FindShaderStaticCombo( uint32 nStaticComboID, const ShaderHeader_t& header, StaticComboRecord_t *pRecords )
{
	if ( header.m_nVersion < 5 )
		return -1;

	return BinarySearchCombos( nStaticComboID, header.m_nNumStaticCombos, pRecords );
}

// cache redundant i/o fetched components of the vcs files
struct ShaderFileCache_t
{
	CUtlSymbol			m_Name;
	CUtlSymbol			m_Filename;
	ShaderHeader_t		m_Header;
	bool				m_bVertexShader;

	// valid for diff version only - contains the microcode used as the reference for diff algorithm
	CUtlBuffer			m_ReferenceCombo;

	// valid for ver5 only - contains the directory
	CUtlVector< StaticComboRecord_t >	m_StaticComboRecords;
	CUtlVector< StaticComboAliasRecord_t > m_StaticComboDupRecords;

	ShaderFileCache_t()
	{
		// invalid until version established
		m_Header.m_nVersion = 0;
		m_bVertexShader = false;
	}

	bool IsValid() const
	{
		return m_Header.m_nVersion != 0;
	}

	bool IsOldVersion() const
	{
		return m_Header.m_nVersion < 5;
	}

	int IsVersion6() const
	{
		return ( m_Header.m_nVersion == 6 );
	}

	intp FindCombo( uint32 nStaticComboID )
	{
		intp nSearchAliases = BinarySearchCombos( nStaticComboID, m_StaticComboDupRecords.Count(), m_StaticComboDupRecords.Base() );
		if ( nSearchAliases != -1 )
			nStaticComboID = m_StaticComboDupRecords[nSearchAliases].m_nSourceStaticCombo;
		return FindShaderStaticCombo( nStaticComboID, m_Header, m_StaticComboRecords.Base() );
	}

	bool operator==( const ShaderFileCache_t& a ) const
	{
		return m_Name == a.m_Name && m_bVertexShader == a.m_bVertexShader;
	}
};


//-----------------------------------------------------------------------------
// Vertex + pixel shader manager
//-----------------------------------------------------------------------------
class CShaderManager final : public IShaderManager
{
public:
	CShaderManager();
	~CShaderManager();

	// Methods of IShaderManager
	void				Init() override;
	void				Shutdown() override;
	IShaderBuffer		*CompileShader( const char *pProgram, size_t nBufLen, const char *pShaderVersion ) override;
	VertexShaderHandle_t CreateVertexShader( IShaderBuffer* pShaderBuffer ) override;
	void				DestroyVertexShader( VertexShaderHandle_t hShader ) override;
	PixelShaderHandle_t CreatePixelShader( IShaderBuffer* pShaderBuffer ) override;
	void				DestroyPixelShader( PixelShaderHandle_t hShader ) override;
	VertexShader_t		CreateVertexShader( const char *pVertexShaderFile, int nStaticVshIndex = 0, char *debugLabel = NULL ) override;
	PixelShader_t		CreatePixelShader( const char *pPixelShaderFile, int nStaticPshIndex = 0, char *debugLabel = NULL ) override;
	void				SetVertexShader( VertexShader_t shader ) override;
	void				SetPixelShader( PixelShader_t shader ) override;
	void				BindVertexShader( VertexShaderHandle_t shader ) override;
	void				BindPixelShader( PixelShaderHandle_t shader ) override;
	void				*GetCurrentVertexShader() override;
	void				*GetCurrentPixelShader() override;
	void				ResetShaderState() override;
	void				ClearVertexAndPixelShaderRefCounts() override;
	void				PurgeUnusedVertexAndPixelShaders() override;

#if defined( DX_TO_GL_ABSTRACTION )
	void		DoStartupShaderPreloading() override;
#endif

	void				SpewVertexAndPixelShaders();
	void				FlushShaders();

	bool				CreateDynamicCombos_Ver4( void *pContext, uint8 *pComboBuffer );
	bool				CreateDynamicCombos_Ver5( void *pContext, uint8 *pComboBuffer, char *debugLabel = NULL );

	static void			QueuedLoaderCallback( void *pContext, void *pContext2, const void *pData, int nSize, LoaderError_t loaderError );

private:
	typedef CUtlFixedLinkedList< IDirect3DVertexShader9* >::IndexType_t VertexShaderIndex_t;
	typedef CUtlFixedLinkedList< IDirect3DPixelShader9* >::IndexType_t PixelShaderIndex_t;
	
	struct ShaderStaticCombos_t
	{
		int					m_nCount;

		// Can't use CUtlVector here since you CUtlLinkedList<CUtlVector<>> doesn't work.
		HardwareShader_t	*m_pHardwareShaders;
		struct ShaderCreationData_t
		{
			CUtlVector<uint8> 	ByteCode;
			uint32 				iCentroidMask;
		};

		ShaderCreationData_t *m_pCreationData;
	};
	
	struct ShaderLookup_t
	{
		CUtlSymbol				m_Name;
		int						m_nStaticIndex;
		ShaderStaticCombos_t	m_ShaderStaticCombos;
		DWORD					m_Flags;
		int						m_nRefCount;
		size_t					m_hShaderFileCache;

		// for queued loading, bias an aligned optimal buffer forward to correct location
		int						m_nDataOffset;

		// diff version, valid during load only
		ShaderDictionaryEntry_t	*m_pComboDictionary;

		ShaderLookup_t()
		{
			m_nStaticIndex = 0;
			m_ShaderStaticCombos.m_nCount = 0;
			m_ShaderStaticCombos.m_pHardwareShaders = 0;
			m_ShaderStaticCombos.m_pCreationData = 0;
			m_Flags = 0;
			m_nRefCount = 0;
			m_hShaderFileCache = 0;
			m_nDataOffset = 0;
			m_pComboDictionary = NULL;
		}
		void IncRefCount()
		{
			m_nRefCount++;
		}
		bool operator==( const ShaderLookup_t& a ) const
		{
			return m_Name == a.m_Name && m_nStaticIndex == a.m_nStaticIndex;
		}
	};

#ifdef DYNAMIC_SHADER_COMPILE
	struct Combo_t
	{
		CUtlSymbol m_ComboName;
		int m_nMin;
		int m_nMax;
	};

	struct ShaderCombos_t
	{
		CUtlVector<Combo_t> m_StaticCombos;
		CUtlVector<Combo_t> m_DynamicCombos;
		int GetNumDynamicCombos( void ) const
		{
			int combos = 1;
			for( auto &combo : m_DynamicCombos )
			{
				combos *= ( combo.m_nMax - combo.m_nMin + 1 );
			}
			return combos;
		}

		int GetNumStaticCombos( void ) const
		{
			int combos = 1;
			for( auto &combo : m_StaticCombos )
			{
				combos *= ( combo.m_nMax - combo.m_nMin + 1 );
			}
			return combos;
		}
	};
#endif

private:
	void					CreateStaticShaders();
	void					DestroyStaticShaders();

#if defined ( DYNAMIC_SHADER_COMPILE ) && defined( REMOTE_DYNAMIC_SHADER_COMPILE )
	void					InitRemoteShaderCompile();
	void					DeinitRemoteShaderCompile();
#endif

	// The low-level dx call to set the vertex shader state
	void					SetVertexShaderState( HardwareShader_t shader, DataCacheHandle_t hCachedShader = DC_INVALID_HANDLE );

	// The low-level dx call to set the pixel shader state
	void					SetPixelShaderState( HardwareShader_t shader, DataCacheHandle_t hCachedShader = DC_INVALID_HANDLE );

	// Destroys all shaders
	void					DestroyAllShaders();

	// Destroy a particular vertex shader
	void					DestroyVertexShader( VertexShader_t shader );
	// Destroy a particular pixel shader
	void					DestroyPixelShader( PixelShader_t shader );

	bool					LoadAndCreateShaders( ShaderLookup_t &lookup, bool bVertexShader, char *debugLabel = NULL );
	FileHandle_t			OpenFileAndLoadHeader( const char *pFileName, ShaderHeader_t *pHeader );

#ifdef DYNAMIC_SHADER_COMPILE
	bool					LoadAndCreateShaders_Dynamic( ShaderLookup_t &lookup, bool bVertexShader );
	const ShaderCombos_t	*FindOrCreateShaderCombos( const char *pShaderName );
	HardwareShader_t		CompileShader( const char *pShaderName, int nStaticIndex, int nDynamicIndex, bool bVertexShader );
#endif

	void					DisassembleShader( ShaderLookup_t *pLookup, int dynamicCombo, uint8 *pByteCode, size_t size );
	void					WriteTranslatedFile( ShaderLookup_t *pLookup, int dynamicCombo, char *pFileContents, char *pFileExtension );

	// DX_TO_GL_ABSTRACTION only, no-op otherwise
	
	void					SaveShaderCache( char *cacheName );	// query GLM pair cache for all active shader pairs and write them to disk in named file
	bool					LoadShaderCache( char *cacheName );	// read named file, establish compiled shader sets for each vertex+static and pixel+static, then link pairs as listed in table
																// return true on success, false if file not found

	// old void					WarmShaderCache();

	CUtlFixedLinkedList< ShaderLookup_t > m_VertexShaderDict;
	CUtlFixedLinkedList< ShaderLookup_t > m_PixelShaderDict;

	CUtlSymbolTable m_ShaderSymbolTable;

#ifdef DYNAMIC_SHADER_COMPILE	
	CUtlStringMap<ShaderCombos_t>	 m_ShaderNameToCombos;
#endif
	
	// The current vertex and pixel shader
	HardwareShader_t	m_HardwareVertexShader;
	HardwareShader_t	m_HardwarePixelShader;

	CUtlFixedLinkedList< se::win::com::com_ptr<IDirect3DVertexShader9> > m_RawVertexShaderDict;
	CUtlFixedLinkedList< se::win::com::com_ptr<IDirect3DPixelShader9> > m_RawPixelShaderDict;

	CUtlFixedLinkedList< ShaderFileCache_t > m_ShaderFileCache;

	// false, creates during init.
	// true, creates on access, helps reduce d3d memory for tools, but causes i/o hitches.
	bool m_bCreateShadersOnDemand;

#if defined( _DEBUG )
	// for debugging (can't resolve UtlSym)
	// need some history because 360 d3d has rips related to sequencing
	char	vshDebugName[MAX_SHADER_HISTORY][64];
	int		vshDebugIndex;
	char	pshDebugName[MAX_SHADER_HISTORY][64];
	int		pshDebugIndex;
#endif

#if defined ( DYNAMIC_SHADER_COMPILE ) && defined( REMOTE_DYNAMIC_SHADER_COMPILE )
	SOCKET m_RemoteShaderCompileSocket;
#endif

};


//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
static CShaderManager s_ShaderManager;
IShaderManager *g_pShaderManager = &s_ShaderManager;

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CShaderManager::CShaderManager() : 
	m_VertexShaderDict( 32 ),
	m_PixelShaderDict( 32 ),
	m_ShaderSymbolTable( 0, 32, true /* caseInsensitive */ ),
	m_ShaderFileCache( 32 )
{
	m_HardwareVertexShader = nullptr;
	m_HardwarePixelShader = nullptr;

	m_bCreateShadersOnDemand = false;

#ifdef DYNAMIC_SHADER_COMPILE
#ifdef REMOTE_DYNAMIC_SHADER_COMPILE
	m_RemoteShaderCompileSocket = INVALID_SOCKET;
#endif
#endif

#ifdef _DEBUG
	memset( &vshDebugName, 0, sizeof(vshDebugName) );
	vshDebugIndex = 0;
	memset( &pshDebugName, 0, sizeof(pshDebugName) );
	pshDebugIndex = 0;
#endif
}

CShaderManager::~CShaderManager()
{
#if defined ( DYNAMIC_SHADER_COMPILE ) && defined( REMOTE_DYNAMIC_SHADER_COMPILE )
	DeinitRemoteShaderCompile();
#endif
}

#define REMOTE_SHADER_COMPILE_PORT "20000"

#if defined ( DYNAMIC_SHADER_COMPILE ) && defined( REMOTE_DYNAMIC_SHADER_COMPILE )
void CShaderManager::InitRemoteShaderCompile()
{
	DeinitRemoteShaderCompile();
	
	addrinfo hints = {};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	addrinfo *result = nullptr;
	int rc = getaddrinfo( mat_remoteshadercompile.GetString(),
		mat_remoteshadercompile_port.GetString(), &hints, &result );
	if ( rc != 0 )
	{
		DWarning( "remote shader compiler", 0,
			"getaddrinfo %s:%s failed: %s.\n",
			mat_remoteshadercompile.GetString(),
			mat_remoteshadercompile_port.GetString(),
			std::system_category().message(::WSAGetLastError()).c_str() );
		Assert( 0 );
	}

	// Attempt to connect to an address until one succeeds
	for( addrinfo *ptr = result; ptr; ptr = ptr->ai_next )
	{
		// Create a SOCKET for connecting to remote shader compilation server
		m_RemoteShaderCompileSocket = socket( ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol );
		if ( m_RemoteShaderCompileSocket == INVALID_SOCKET )
		{
			DWarning( "remote shader compiler", 0,
				"socket(%d, %d, %d) failed: %s.\n",
				ptr->ai_family,
				ptr->ai_socktype,
				ptr->ai_protocol,
				std::system_category().message(::WSAGetLastError()).c_str() );
			freeaddrinfo( result );
			Assert( 0 );
			continue;
		}

		// Connect to server.
		rc = connect( m_RemoteShaderCompileSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if ( rc == SOCKET_ERROR )
		{
			closesocket( m_RemoteShaderCompileSocket );
			m_RemoteShaderCompileSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}
	
	freeaddrinfo( result );

	if ( m_RemoteShaderCompileSocket == INVALID_SOCKET )
	{
		DWarning( "remote shader compiler", 0,
			"Unable to connect to remote shader compilation server %s:%s!\n",
			mat_remoteshadercompile.GetString(),
			mat_remoteshadercompile_port.GetString() );
		Assert ( 0 );
	}
}

void CShaderManager::DeinitRemoteShaderCompile()
{
	if ( m_RemoteShaderCompileSocket != INVALID_SOCKET )
	{
		if ( shutdown( m_RemoteShaderCompileSocket, SD_SEND ) == SOCKET_ERROR )
		{
			DWarning( "remote shader compiler", 0,
				"shutdown(%s:%s, SD_SEND) failed: %s.\n",
				mat_remoteshadercompile.GetString(),
				mat_remoteshadercompile_port.GetString(),
				std::system_category().message(::WSAGetLastError()).c_str() );
		}

		if ( closesocket( m_RemoteShaderCompileSocket ) == SOCKET_ERROR )
		{
			DWarning( "remote shader compiler", 0,
				"closesocket(%s:%s) failed: %s.\n",
				mat_remoteshadercompile.GetString(),
				mat_remoteshadercompile_port.GetString(),
				std::system_category().message(::WSAGetLastError()).c_str() );
		}

		m_RemoteShaderCompileSocket = INVALID_SOCKET;
	}
}
#endif // defined ( DYNAMIC_SHADER_COMPILE ) && defined( REMOTE_DYNAMIC_SHADER_COMPILE )

//-----------------------------------------------------------------------------
// Initialization, shutdown
//-----------------------------------------------------------------------------
void CShaderManager::Init()
{
	// only used by PC to help tools reduce d3d footprint
	m_bCreateShadersOnDemand = ShaderUtil()->InEditorMode() || CommandLine()->CheckParm( "-shadersondemand" );

#ifdef DYNAMIC_SHADER_COMPILE
#ifdef REMOTE_DYNAMIC_SHADER_COMPILE
	InitRemoteShaderCompile();
#endif
#endif // DYNAMIC_SHADER_COMPILE

	CreateStaticShaders();
}

void CShaderManager::Shutdown()
{
#ifdef DYNAMIC_SHADER_COMPILE
	if ( m_pShaderCompiler )
	{
		Sys_UnloadModule( m_pShaderCompiler );
		m_pShaderCompiler = 0;
	}
#endif

#ifdef DX_TO_GL_ABSTRACTION
	if (mat_autosave_glshaders.GetInt())
	{
		SaveShaderCache("glshaders.cfg");
	}
#endif

	DestroyAllShaders();
	DestroyStaticShaders();
}


//-----------------------------------------------------------------------------
// Compiles shaders
//-----------------------------------------------------------------------------
IShaderBuffer *CShaderManager::CompileShader( const char *pProgram, size_t nBufLen, const char *pShaderVersion )
{
	UINT nCompileFlags = D3DCOMPILE_AVOID_FLOW_CONTROL;

#ifdef _DEBUG
	nCompileFlags |= D3DCOMPILE_DEBUG;
#endif

	se::win::com::com_ptr<ID3DBlob> pCompiledShader, pErrorMessages;
	HRESULT hr = D3DCompile( pProgram, nBufLen,
		nullptr, nullptr, nullptr, "main", pShaderVersion, nCompileFlags, 0,
		&pCompiledShader, &pErrorMessages );

	if ( FAILED( hr ) )
	{
		if ( pErrorMessages )
		{
			const char *pErrorMessage = (const char *)pErrorMessages->GetBufferPointer();
			Warning( "Shader compilation failed! Reported the following errors:\n%s\n", pErrorMessage );
		}
		return NULL;
	}

	// NOTE: This uses small block heap allocator; so I'm not going
	// to bother creating a memory pool.
	return new CShaderBuffer( std::move(pCompiledShader) );
}


VertexShaderHandle_t CShaderManager::CreateVertexShader( IShaderBuffer* pShaderBuffer )
{
	// dimhotepus: Use CreateD3DVertexShader instead of DirectX API call.
	HardwareShader_t shader = CreateD3DVertexShader( (const DWORD*)pShaderBuffer->GetBits(), pShaderBuffer->GetSize(), nullptr, nullptr );

	// Create the vertex shader
	se::win::com::com_ptr<IDirect3DVertexShader9> pVertexShader;
	pVertexShader.Attach(static_cast<IDirect3DVertexShader9*>(shader));

	// Insert the shader into the dictionary of shaders
	VertexShaderIndex_t i = m_RawVertexShaderDict.AddToTail( std::move(pVertexShader) );
	return (VertexShaderHandle_t)i;
}

void CShaderManager::DestroyVertexShader( VertexShaderHandle_t hShader )
{
	if ( hShader == VERTEX_SHADER_HANDLE_INVALID )
		return;

	VertexShaderIndex_t i = (VertexShaderIndex_t)hShader;
	se::win::com::com_ptr<IDirect3DVertexShader9> &pVertexShader = m_RawVertexShaderDict[ i ];

	UnregisterVS( pVertexShader );

	m_RawVertexShaderDict.Remove( i );
}

PixelShaderHandle_t CShaderManager::CreatePixelShader( IShaderBuffer* pShaderBuffer )
{
  // dimhotepus: Use CreateD3DPixelShader instead of DirectX API call.
	HardwareShader_t shader = CreateD3DPixelShader( static_cast<DWORD *>(const_cast<void*>(pShaderBuffer->GetBits())), 0, pShaderBuffer->GetSize(), nullptr, nullptr );

	// Create the vertex shader
	se::win::com::com_ptr<IDirect3DPixelShader9> pPixelShader;
	pPixelShader.Attach(static_cast<IDirect3DPixelShader9*>(shader));

	// Insert the shader into the dictionary of shaders
	PixelShaderIndex_t i = m_RawPixelShaderDict.AddToTail( std::move(pPixelShader) );
	return (PixelShaderHandle_t)i;
}

void CShaderManager::DestroyPixelShader( PixelShaderHandle_t hShader )
{
	if ( hShader == PIXEL_SHADER_HANDLE_INVALID )
		return;

	PixelShaderIndex_t i = (PixelShaderIndex_t)hShader;
	se::win::com::com_ptr<IDirect3DPixelShader9> &pPixelShader = m_RawPixelShaderDict[ i ];

	UnregisterPS( pPixelShader );

	m_RawPixelShaderDict.Remove( i );
}


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
HardwareShader_t s_pIllegalMaterialPS = INVALID_HARDWARE_SHADER;

//-----------------------------------------------------------------------------
// Static methods
//-----------------------------------------------------------------------------
void CShaderManager::CreateStaticShaders()
{
	MEM_ALLOC_D3D_CREDIT();

	if ( !HardwareConfig()->SupportsVertexAndPixelShaders() )
	{
		return;
	}

	if ( IsPC() )
	{
		// GR - hack for illegal materials
		const DWORD psIllegalMaterial[] =
		{
			#ifdef DX_TO_GL_ABSTRACTION
				// Use a PS 2.0 binary shader on DX_TO_GL_ABSTRACTION
				0xffff0200, 0x05000051, 0xa00f0000, 0x3f800000,
				0x00000000, 0x3f800000, 0x3f800000, 0x02000001,
				0x800f0000, 0xa0e40000, 0x02000001, 0x800f0800,
				0x80e40000, 0x0000ffff
			#else
				0xffff0101, 0x00000051, 0xa00f0000, 0x00000000, 0x3f800000, 0x00000000, 
				0x3f800000, 0x00000001, 0x800f0000, 0xa0e40000, 0x0000ffff
			#endif
		};
		// create default shader
#if defined(_X360) || !defined(DX_TO_GL_ABSTRACTION)
		Dx9Device()->CreatePixelShader( psIllegalMaterial, ( IDirect3DPixelShader9 ** )&s_pIllegalMaterialPS );
#else
		Dx9Device()->CreatePixelShader( psIllegalMaterial, ( IDirect3DPixelShader9 ** )&s_pIllegalMaterialPS, NULL );
#endif
	}
}

void CShaderManager::DestroyStaticShaders()
{
	// GR - invalid material hack
	// destroy internal shader
	if ( s_pIllegalMaterialPS != INVALID_HARDWARE_SHADER )
	{
		( ( IDirect3DPixelShader9 * )s_pIllegalMaterialPS )->Release();
		s_pIllegalMaterialPS = INVALID_HARDWARE_SHADER;
	}
}

#ifdef DYNAMIC_SHADER_COMPILE
static const char *GetShaderSourcePath( void )
{
	static char shaderDir[MAX_PATH];
	// GR - just in case init this...
	static bool bHaveShaderDir = false;
	if( !bHaveShaderDir )
	{
		bHaveShaderDir = true;
#		if ( defined( DYNAMIC_SHADER_COMPILE_CUSTOM_PATH ) )
		{
			V_strcpy_safe( shaderDir, DYNAMIC_SHADER_COMPILE_CUSTOM_PATH );
		}
#		else
		{
			V_strcpy_safe( shaderDir, __FILE__ );
			V_StripFilename( shaderDir );
			V_StripLastDir( shaderDir );
			V_strcat_safe( shaderDir, "stdshaders" );
		}
#		endif
	}
	return shaderDir;
}
#endif

#ifdef DYNAMIC_SHADER_COMPILE
const CShaderManager::ShaderCombos_t *CShaderManager::FindOrCreateShaderCombos( const char *pShaderName )
{
	if( m_ShaderNameToCombos.Defined( pShaderName ) )
	{
		return &m_ShaderNameToCombos[pShaderName];
	}

	ShaderCombos_t &combos = m_ShaderNameToCombos[pShaderName];

	char filename[MAX_PATH];
	// try the vsh dir first.
	Q_strncpy( filename, GetShaderSourcePath(), MAX_PATH );
	Q_strncat( filename, "\\", MAX_PATH, COPY_ALL_CHARACTERS );
	Q_strncat( filename, pShaderName, MAX_PATH, COPY_ALL_CHARACTERS );
	Q_strncat( filename, ".vsh", MAX_PATH, COPY_ALL_CHARACTERS );
	CUtlInplaceBuffer bffr( 0, 0, CUtlInplaceBuffer::TEXT_BUFFER );
	
	bool bOpenResult = g_pFullFileSystem->ReadFile( filename, NULL, bffr );
	
	if ( bOpenResult )
	{
		NULL;
	}
	else
	{
		// try the fxc dir.
		Q_strncpy( filename, GetShaderSourcePath(), MAX_PATH );
		Q_strncat( filename, "\\", MAX_PATH, COPY_ALL_CHARACTERS );
		Q_strncat( filename, pShaderName, MAX_PATH, COPY_ALL_CHARACTERS );
		Q_strncat( filename, ".fxc", MAX_PATH, COPY_ALL_CHARACTERS );
		bOpenResult = g_pFullFileSystem->ReadFile( filename, NULL, bffr );

		if ( !bOpenResult )
		{
			// Maybe this is a specific version [20 & 20b] -> [2x]
			if ( Q_strlen( pShaderName ) >= 3 )
			{
				char *pszEndFilename = filename + strlen( filename );
				if ( !Q_stricmp( pszEndFilename - 6, "30.fxc" ) )
				{
					// Total hack. Who knows what builds that 30 shader?
					strcpy( pszEndFilename - 6, "20b.fxc" );
					bOpenResult = g_pFullFileSystem->ReadFile( filename, NULL, bffr );
					if ( !bOpenResult )
					{
						strcpy( pszEndFilename - 6, "2x.fxc" );
						bOpenResult = g_pFullFileSystem->ReadFile( filename, NULL, bffr );
					}
					if ( !bOpenResult )
					{
						strcpy( pszEndFilename - 6, "20.fxc" );
						bOpenResult = g_pFullFileSystem->ReadFile( filename, NULL, bffr );
					}
				}
				else
				{
					if ( !stricmp( pszEndFilename - 6, "20.fxc" ) )
					{
						pszEndFilename[ -5 ] = 'x';
					}
					else if ( !stricmp( pszEndFilename - 7, "20b.fxc" ) )
					{
						strcpy( pszEndFilename - 7, "2x.fxc" );
						--pszEndFilename;
					}
					else if ( !stricmp( pszEndFilename - 6, "11.fxc" ) )
					{
						strcpy( pszEndFilename - 6, "xx.fxc" );
					}

					bOpenResult = g_pFullFileSystem->ReadFile( filename, NULL, bffr );
					if ( !bOpenResult )
					{
						if ( !stricmp( pszEndFilename - 6, "2x.fxc" ) )
						{
							pszEndFilename[ -6 ] = 'x';
							bOpenResult = g_pFullFileSystem->ReadFile( filename, NULL, bffr );
						}
					}
				}
			}
		}

		if ( !bOpenResult )
		{
			Assert( 0 );
			return NULL;
		}
	}

	while( char *line = bffr.InplaceGetLinePtr() )
	{
		// dear god perl is better at this kind of shit!
		int begin = 0;
		int end = 0;

		// check if the line starts with '//'
		if( line[0] != '/' || line[1] != '/' )
		{
			continue;
		}

		// Check if line intended for platform lines
		if ( Q_stristr( line, "[360]" ) || Q_stristr( line, "[XBOX]" ) )
				continue;

		// Skip any lines intended for other shader version
		if ( Q_stristr( pShaderName, "_ps20" ) && !Q_stristr( pShaderName, "_ps20b" ) &&
			 Q_stristr( line, "[ps" ) && !Q_stristr( line, "[ps20]" ) )
			 continue;
		if ( Q_stristr( pShaderName, "_ps20b" ) &&
			 Q_stristr( line, "[ps" ) && !Q_stristr( line, "[ps20b]" ) )
			 continue;
		if ( Q_stristr( pShaderName, "_ps30" ) &&
			Q_stristr( line, "[ps" ) &&	 !Q_stristr( line, "[ps30]" ) )
			continue;
		// dimhotepus: Shader model 4, 4.1, 5 support.
		if ( Q_stristr( pShaderName, "_ps40" ) &&
			Q_stristr( line, "[ps" ) &&	 !Q_stristr( line, "[ps40]" ) )
			continue;
		if ( Q_stristr( pShaderName, "_ps41" ) &&
			Q_stristr( line, "[ps" ) &&	 !Q_stristr( line, "[ps41]" ) )
			continue;
		if ( Q_stristr( pShaderName, "_ps50" ) &&
			Q_stristr( line, "[ps" ) &&	 !Q_stristr( line, "[ps50]" ) )
			continue;
		if ( Q_stristr( pShaderName, "_vs20" ) &&
			Q_stristr( line, "[vs" ) &&	 !Q_stristr( line, "[vs20]" ) )
			continue;
		if ( Q_stristr( pShaderName, "_vs30" ) &&
			Q_stristr( line, "[vs" ) &&	 !Q_stristr( line, "[vs30]" ) )
			continue;
		// dimhotepus: Shader model 4, 4.1, 5 support.
		if ( Q_stristr( pShaderName, "_vs40" ) &&
			Q_stristr( line, "[vs" ) &&	 !Q_stristr( line, "[vs40]" ) )
			continue;
		if ( Q_stristr( pShaderName, "_vs41" ) &&
			Q_stristr( line, "[vs" ) &&	 !Q_stristr( line, "[vs41]" ) )
			continue;
		if ( Q_stristr( pShaderName, "_vs50" ) &&
			Q_stristr( line, "[vs" ) &&	 !Q_stristr( line, "[vs50]" ) )
			continue;

		char *pScan = &line[2];
		while( *pScan == ' ' || *pScan == '\t' )
		{
			pScan++;
		}

		bool bDynamic;
		if( Q_strncmp( pScan, "DYNAMIC", 7 ) == 0 )
		{
			bDynamic = true;
			pScan += 7;
		}
		else if( Q_strncmp( pScan, "STATIC", 6 ) == 0 )
		{
			bDynamic = false;
			pScan += 6;
		}
		else
		{
			continue;
		}

		// skip whitespace
		while( *pScan == ' ' || *pScan == '\t' )
		{
			pScan++;
		}

		// check for colon
		if( *pScan != ':' )
		{
			continue;
		}
		pScan++;

		// skip whitespace
		while( *pScan == ' ' || *pScan == '\t' )
		{
			pScan++;
		}

		// check for quote
		if( *pScan != '\"' )
		{
			continue;
		}
		pScan++;

		char *pBeginningOfName = pScan;
		while( 1 )
		{
			if( *pScan == '\0' )
			{
				break;
			}
			if( *pScan == '\"' )
			{
				break;
			}
			pScan++;
		}

		if( *pScan == '\0' )
		{
			continue;
		}

		// must have hit a quote. .done with string.
		// slam a NULL at the end quote of the string so that we have the string at pBeginningOfName.
		*pScan = '\0';
		pScan++;

		// skip whitespace
		while( *pScan == ' ' || *pScan == '\t' )
		{
			pScan++;
		}

		// check for quote
		if( *pScan != '\"' )
		{
			continue;
		}
		pScan++;

		// make sure that we have a number after the quote.
		if( !isdigit( *pScan ) )
		{
			continue;
		}

		while( isdigit( *pScan ) )
		{
			begin = begin * 10 + ( *pScan - '0' );
			pScan++;
		}

		if( pScan[0] != '.' || pScan[1] != '.' )
		{
			continue;
		}
		pScan += 2;

		// make sure that we have a number
		if( !isdigit( *pScan ) )
		{
			continue;
		}

		while( isdigit( *pScan ) )
		{
			end = end * 10 + ( *pScan - '0' );
			pScan++;
		}

		if( pScan[0] != '\"' )
		{
			continue;
		}

		// sweet freaking jesus. .done parsing the line.
//		char buf[1024];
//		V_sprintf_safe( buf, "\"%s\" \"%s\" %d %d\n", bDynamic ? "DYNAMIC" : "STATIC", pBeginningOfName, begin, end );
//		Plat_DebugString( buf );

		Combo_t *pCombo = NULL;
		if( bDynamic )
		{
			pCombo = &combos.m_DynamicCombos[combos.m_DynamicCombos.AddToTail()];
		}
		else
		{
			pCombo = &combos.m_StaticCombos[combos.m_StaticCombos.AddToTail()];
		}

		pCombo->m_ComboName = m_ShaderSymbolTable.AddString( pBeginningOfName );
		pCombo->m_nMin = begin;
		pCombo->m_nMax = end;
	}
	
	return &combos;
}
#endif // DYNAMIC_SHADER_COMPILE

#ifdef DYNAMIC_SHADER_COMPILE
#ifndef DX_TO_GL_ABSTRACTION
//-----------------------------------------------------------------------------
// Used to deal with include files
//-----------------------------------------------------------------------------
class CDxInclude : public ID3DInclude
{
public:
	CDxInclude( const char *pMainFileName );

	STDMETHOD(Open)(THIS_ D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override;

	STDMETHOD(Close)(THIS_ LPCVOID pData) override;

private:
	char m_pBasePath[MAX_PATH];
};

CDxInclude::CDxInclude( const char *pMainFileName )
{
	V_ExtractFilePath( pMainFileName, m_pBasePath );
}


HRESULT CDxInclude::Open( D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID * ppData, UINT * pBytes )
{
	char pTemp[MAX_PATH];
	if ( !Q_IsAbsolutePath( pFileName ) && ( IncludeType == D3D_INCLUDE_LOCAL ) )
	{
		V_ComposeFileName( m_pBasePath, pFileName, pTemp );
		pFileName = pTemp;
	}

	CUtlBuffer buf( (intp)0, (intp)0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( pFileName, NULL, buf ) )
		return E_FAIL;

	*pBytes = buf.TellMaxPut();
	void *pMem = malloc( *pBytes );
	memcpy( pMem, buf.Base(), *pBytes );
	*ppData = pMem;

	return S_OK;
}

HRESULT CDxInclude::Close( LPCVOID pData )
{
	void *pMem = const_cast<void*>( pData );
	free( pMem );
	return S_OK;
}
#endif // not DX_TO_GL_ABSTRACTION

static const char *FileNameToShaderProfile( const char *pShaderName, bool bVertexShader )
{
	// Figure out the shader profile
	const char *pShaderProfile = nullptr;
	if( bVertexShader )
	{
		if( Q_stristr( pShaderName, "vs20" ) )
		{
			pShaderProfile = "vs_2_0";
			bVertexShader = true;
		}
		else if( Q_stristr( pShaderName, "vs11" ) )
		{
			pShaderProfile = "vs_1_1";
			bVertexShader = true;
		}
		else if( Q_stristr( pShaderName, "vs14" ) )
		{
			pShaderProfile = "vs_1_1";
			bVertexShader = true;
		}
		else if( Q_stristr( pShaderName, "vs30" ) )
		{
			pShaderProfile = "vs_3_0";
			bVertexShader = true;
		}
		// dimhotepus: Shader Model 4 support.
		else if( Q_stristr( pShaderName, "vs40" ) )
		{
			pShaderProfile = "vs_4_0";
			bVertexShader = true;
		}
		// dimhotepus: Shader Model 4.1 support.
		else if( Q_stristr( pShaderName, "vs41" ) )
		{
			pShaderProfile = "vs_4_1";
			bVertexShader = true;
		}
		// dimhotepus: Shader Model 5.0 support.
		else if( Q_stristr( pShaderName, "vs50" ) )
		{
			pShaderProfile = "vs_5_0";
			bVertexShader = true;
		}
		else
		{
			Error( "Unable to get shader profile for vertex shader %s.\n", pShaderName );
		}
	}
	else
	{
		if( Q_stristr( pShaderName, "ps20b" ) )
		{
			pShaderProfile = "ps_2_b";
		}
		else if( Q_stristr( pShaderName, "ps20" ) )
		{
			pShaderProfile = "ps_2_0";
		}
		else if( Q_stristr( pShaderName, "ps11" ) )
		{
			pShaderProfile = "ps_1_1";
		}
		else if( Q_stristr( pShaderName, "ps14" ) )
		{
			pShaderProfile = "ps_1_4";
		}
		else if( Q_stristr( pShaderName, "ps30" ) )
		{
			pShaderProfile = "ps_3_0";
		}
		// dimhotepus: Shader Model 4 support.
		else if( Q_stristr( pShaderName, "ps40" ) )
		{
			pShaderProfile = "ps_4_0";
		}
		// dimhotepus: Shader Model 4.1 support.
		else if( Q_stristr( pShaderName, "ps41" ) )
		{
			pShaderProfile = "ps_4_1";
		}
		// dimhotepus: Shader Model 5.0 support.
		else if( Q_stristr( pShaderName, "ps50" ) )
		{
			pShaderProfile = "ps_5_0";
		}
		else
		{
			Error( "Unable to get shader profile for pixel shader %s.\n", pShaderName );
		}
	}
	return pShaderProfile;
}
#endif

#ifdef DYNAMIC_SHADER_COMPILE

HardwareShader_t CShaderManager::CompileShader( const char *pShaderName, 
												int nStaticIndex, int nDynamicIndex, bool bVertexShader )
{
	VPROF_BUDGET( "CompileShader", "CompileShader" );

	Assert( m_ShaderNameToCombos.Defined( pShaderName ) );
	if( !m_ShaderNameToCombos.Defined( pShaderName ) )
	{
		return INVALID_HARDWARE_SHADER;
	}

	const ShaderCombos_t &combos = m_ShaderNameToCombos[pShaderName];
#ifdef _DEBUG
	int numStaticCombos = combos.GetNumStaticCombos();
	int numDynamicCombos = combos.GetNumDynamicCombos();
#endif
	Assert( nStaticIndex % numDynamicCombos == 0 );
	Assert( ( nStaticIndex % numDynamicCombos ) >= 0 &&
			( nStaticIndex % numDynamicCombos ) < numStaticCombos );
	Assert( nDynamicIndex >= 0 && nDynamicIndex < numDynamicCombos );

#	ifdef DYNAMIC_SHADER_COMPILE_VERBOSE

	Warning( "Compiling " );
	if ( bVertexShader )
		ConColorMsg( Color( 0, 255, 0, 255 ), "vsh - %s ", pShaderName );
	else
		ConColorMsg( Color( 0, 255, 255, 255 ), "psh - %s ", pShaderName );
	Warning( "\n\tdynamic:" );

#	endif

	CUtlVector<D3D_SHADER_MACRO> macros;
	// plus 1 for null termination, plus 1 for #define SHADER_MODEL_*
	macros.SetCount( combos.m_DynamicCombos.Count() + combos.m_StaticCombos.Count() + 2 );

	int nCombo = nStaticIndex + nDynamicIndex;
	int macroIndex = 0;
	for( auto &combo : combos.m_DynamicCombos )
	{
		const int countForCombo = combo.m_nMax - combo.m_nMin + 1;
		const int val = nCombo % countForCombo + combo.m_nMin;

		nCombo /= countForCombo;
		macros[macroIndex].Name = m_ShaderSymbolTable.String( combo.m_ComboName );

		char buf[16];
		V_to_chars( buf, val );
		CUtlSymbol valSymbol( buf );
		macros[macroIndex].Definition = valSymbol.String();

#	ifdef DYNAMIC_SHADER_COMPILE_VERBOSE
		Warning( " %s=%s", macros[macroIndex].Name, macros[macroIndex].Definition );
#	endif

		macroIndex++;
	}

#	ifdef DYNAMIC_SHADER_COMPILE_VERBOSE
	Warning( "\n\tstatic:" );
#	endif

	for( auto &combo : combos.m_StaticCombos )
	{
		const int countForCombo = combo.m_nMax - combo.m_nMin + 1;
		const int val = nCombo % countForCombo + combo.m_nMin;

		nCombo /= countForCombo;
		macros[macroIndex].Name = m_ShaderSymbolTable.String( combo.m_ComboName );

		char buf[16];
		V_to_chars( buf, val );
		CUtlSymbol valSymbol( buf );
		macros[macroIndex].Definition = valSymbol.String();

#	ifdef DYNAMIC_SHADER_COMPILE_VERBOSE
		Warning( " %s=%s", macros[macroIndex].Name, macros[macroIndex].Definition );
#	endif

		macroIndex++;
	}

#	ifdef DYNAMIC_SHADER_COMPILE_VERBOSE
	Warning( "\n" );
#	endif

	char filename[MAX_PATH];
	V_strcpy_safe( filename, GetShaderSourcePath() );
	V_strcat_safe( filename, "\\" );
	V_strcat_safe( filename, pShaderName );
	V_strcat_safe( filename, ".fxc" );
	
	const char *pShaderProfile = FileNameToShaderProfile( pShaderName, bVertexShader );
	
	// define the shader model (actually profile, but name for backward compat).
	char shaderModelDefineString[1024];
	V_sprintf_safe( shaderModelDefineString, "SHADER_MODEL_%s", pShaderProfile );
	Q_strupr( shaderModelDefineString );

	macros[macroIndex].Name = shaderModelDefineString;
	macros[macroIndex].Definition = "1";

	macroIndex++;

	// NULL terminate.
	macros[macroIndex].Name = NULL;
	macros[macroIndex].Definition = NULL;

	// Instead of erroring out, infinite-loop on shader compilation
	// (i.e. give developers a chance to fix the shader code w/out restarting the game)
#ifndef _DEBUG
	int retriesLeft = 20;
retry_compile:
#endif

	// Try and open the file to see if it exists
	FileHandle_t fp = g_pFullFileSystem->Open( filename, "r" );
	if ( !fp )
	{
		// Maybe this is a specific version [20 & 20b] -> [2x]
		if ( strlen( pShaderName ) >= 3 )
		{
			char *pszEndFilename = filename + strlen( filename );
			if ( !Q_stricmp( pszEndFilename - 6, "30.fxc" ) )
			{
				strcpy( pszEndFilename - 6, "20b.fxc" );
				fp = g_pFullFileSystem->Open( filename, "r" );
				if ( fp == FILESYSTEM_INVALID_HANDLE )
				{
					strcpy( pszEndFilename - 6, "2x.fxc" );
					fp = g_pFullFileSystem->Open( filename, "r" );
				}

				if ( fp == FILESYSTEM_INVALID_HANDLE )
				{
					strcpy( pszEndFilename - 6, "20.fxc" );
					fp = g_pFullFileSystem->Open( filename, "r" );
				}
			}
			else
			{
				if ( !Q_stricmp( pszEndFilename - 6, "20.fxc" ) )
				{
					pszEndFilename[ -5 ] = 'x';
					fp = g_pFullFileSystem->Open( filename, "r" );
				}
				else if ( !Q_stricmp( pszEndFilename - 7, "20b.fxc" ) )
				{
					strcpy( pszEndFilename - 7, "2x.fxc" );
					fp = g_pFullFileSystem->Open( filename, "r" );
				}
				else if ( !stricmp( pszEndFilename - 6, "11.fxc" ) )
				{
					strcpy( pszEndFilename - 6, "xx.fxc" );
					fp = g_pFullFileSystem->Open( filename, "r" );
				}

				if ( fp == FILESYSTEM_INVALID_HANDLE )
				{
					if ( !stricmp( pszEndFilename - 6, "2x.fxc" ) )
					{
						pszEndFilename[ -6 ] = 'x';
						fp = g_pFullFileSystem->Open( filename, "r" );
					}
				}
			}
		}
	}

	if ( fp )
	{
		g_pFullFileSystem->Close( fp );
	}

#ifdef REMOTE_DYNAMIC_SHADER_COMPILE
	// Remotely-compiled shader code
	uint32 *pRemotelyCompiledShader = NULL;
	uint32 nRemotelyCompiledShaderLength = 0;

	if ( m_RemoteShaderCompileSocket == INVALID_SOCKET )
	{
		InitRemoteShaderCompile();
	}

	// In this case, we're going to use a remote service to do our compiling
	if ( m_RemoteShaderCompileSocket != INVALID_SOCKET )
	{
		// Build up command list for remote shader compiler
		char pFixedFilename[MAX_PATH], buf[MAX_PATH];
		V_FixupPathName( pFixedFilename, filename );
		V_FileBase( pFixedFilename, buf ); // Just find base filename
		V_strcat_safe( buf, ".fxc" );
		
		char pSendbuf[40000];
		V_sprintf_safe( pSendbuf, "%s\n", buf );
		V_strcat_safe( pSendbuf, pShaderProfile );
		V_strcat_safe( pSendbuf, "\n" );
		V_sprintf_safe( buf, "%zd\n", macros.Count() );
		V_strcat_safe( pSendbuf, buf );

		for ( auto &m : macros )
		{
			V_sprintf_safe( buf, "%s\n%s\n", m.Name, m.Definition );
			V_strcat_safe( pSendbuf, buf );
		}

		V_strcat_safe( pSendbuf, "" );

		// Send commands to remote shader compiler
		if ( int rc =
				send( m_RemoteShaderCompileSocket, pSendbuf, V_strlen( pSendbuf ), 0 );
			 rc == SOCKET_ERROR )
		{
			Warning( "send(%s) failed: %s\n",
				pSendbuf,
				std::system_category().message(::WSAGetLastError()).c_str() );
			DeinitRemoteShaderCompile();
		}

		if ( m_RemoteShaderCompileSocket != INVALID_SOCKET )
		{
			alignas(uint32) char pRecvbuf[40000];

			// Block here until we get a result back from the server
			int rc = recv( m_RemoteShaderCompileSocket, pRecvbuf, ssize(pRecvbuf), 0 );
			if ( rc == 0 )
			{
				Warning( "Connection closed.\n" );
				DeinitRemoteShaderCompile();
			}
			else if ( rc < 0 )
			{
				Warning( "recv failed: %s\n",
					std::system_category().message(::WSAGetLastError()).c_str() );
				DeinitRemoteShaderCompile();
			}

			if ( m_RemoteShaderCompileSocket != INVALID_SOCKET )
			{
				// Grab the first 32 bits, which tell us what the rest of the data is
				uint32 compile_rc;
				memcpy( &compile_rc, pRecvbuf, sizeof( compile_rc ) );

				// If is zero, we have an error, so the rest of the data is a text string from the compiler
				if ( compile_rc == 0x00000000u )
				{
					Warning( "Remote shader compile error: %s.\n", pRecvbuf+4 );
				}
				else // we have an actual binary shader blob coming back
				{
					nRemotelyCompiledShaderLength = compile_rc;
					pRemotelyCompiledShader = (uint32 *) pRecvbuf;
					pRemotelyCompiledShader++;
				}
			}
		}
	} // End using remote compile service
#endif // REMOTE_DYNAMIC_SHADER_COMPILE

#if defined( DYNAMIC_SHADER_COMPILE ) && !defined( REMOTE_DYNAMIC_SHADER_COMPILE )
	bool bShadersNeedFlush = false;
#endif

#if defined( DYNAMIC_SHADER_COMPILE )
	se::win::com::com_ptr<ID3DBlob> pShader;
	se::win::com::com_ptr<ID3DBlob> pErrorMessages;

	const bool b30Shader = !Q_stricmp( pShaderProfile, "vs_3_0" ) || !Q_stricmp( pShaderProfile, "ps_3_0" );
	// dimhotepus: Shader model 4, 4.1, 5 support.
	const bool b40Shader = !Q_stricmp( pShaderProfile, "vs_4_0" ) || !Q_stricmp( pShaderProfile, "ps_4_0" );
	const bool b41Shader = !Q_stricmp( pShaderProfile, "vs_4_1" ) || !Q_stricmp( pShaderProfile, "ps_4_1" );
	const bool b50Shader = !Q_stricmp( pShaderProfile, "vs_5_0" ) || !Q_stricmp( pShaderProfile, "ps_5_0" );

	wchar_t wfilename[MAX_PATH];
	V_UTF8ToUnicode( filename, wfilename, sizeof(wfilename) );
	
	HRESULT hr = S_OK;
	if ( b30Shader || b40Shader || b41Shader || b50Shader )
	{
		CDxInclude dxInclude( filename );

		hr = D3DCompileFromFile( wfilename, macros.Base(), &dxInclude,
			"main",	pShaderProfile, 0, 0, &pShader, &pErrorMessages );
	}
	else
	{
		hr = D3DCompileFromFile( wfilename, macros.Base(), NULL,
			"main",	pShaderProfile, 0, 0, &pShader, &pErrorMessages );
	}

#ifdef REMOTE_DYNAMIC_SHADER_COMPILE
	// If we're using the remote compiling service, let's double-check against a local compile
	if ( SUCCEEDED(hr) &&
		 m_RemoteShaderCompileSocket != INVALID_SOCKET &&
		 pRemotelyCompiledShader )
	{
		if ( pShader->GetBufferSize() != nRemotelyCompiledShaderLength ||
				memcmp( pRemotelyCompiledShader, pShader->GetBufferPointer(), pShader->GetBufferSize() ) != 0 )
		{
			Warning( "Remote and local shaders '%S' (profile '%s') don't match!\n",
				wfilename, pShaderProfile );
			return INVALID_HARDWARE_SHADER;
		}
	}
#endif // REMOTE_DYNAMIC_SHADER_COMPILE

	if ( FAILED( hr ) )
	{
		const char *pErrorMessageString = ( const char * )pErrorMessages->GetBufferPointer();

		Plat_DebugString( pErrorMessageString );
		Plat_DebugString( "\n" );

#ifndef _DEBUG
		if ( retriesLeft-- > 0 )
		{
			DevMsg( 0, "Failed dynamic shader '%S' compile. Fix the shader while the debugger is at the breakpoint, then continue\n", wfilename );
			DebuggerBreakIfDebugging();

#if defined( DYNAMIC_SHADER_COMPILE )
			bShadersNeedFlush = true;
#endif

			goto retry_compile;
		}

		Error( "Failed dynamic shader '%S' compile.\n", wfilename );
#else
		Assert( 0 );
#endif // _DEBUG

		return INVALID_HARDWARE_SHADER;
	}
	else
#endif // #if defined( DYNAMIC_SHADER_COMPILE ) && !defined( REMOTE_DYNAMIC_SHADER_COMPILE )
		
	{
#if defined( DYNAMIC_SHADER_COMPILE_WRITE_ASSEMBLY )
		// enable to dump the disassembly for shader validation
		char exampleCommandLine[2048];
		V_strcpy_safe( exampleCommandLine, "// Run from stdshaders\n// ..\\..\\dx9sdk\\utilities\\fxc.exe " );
		for( auto &m : macros )
		{
			V_strcat_safe( exampleCommandLine, "/D" );
			V_strcat_safe( exampleCommandLine, m.Name );
			V_strcat_safe( exampleCommandLine, "=" );
			V_strcat_safe( exampleCommandLine, m.Definition );
			V_strcat_safe( exampleCommandLine, " " );
		}

		V_strcat_safe( exampleCommandLine, "/T" );
		V_strcat_safe( exampleCommandLine, pShaderProfile );
		V_strcat_safe( exampleCommandLine, " " );
		V_strcat_safe( exampleCommandLine, filename );
		V_strcat_safe( exampleCommandLine, "\n" );

		se::win::com::com_ptr<ID3DBlob> d3dblob;

#if defined( REMOTE_DYNAMIC_SHADER_COMPILE )
		hr = D3DDisassemble( pRemotelyCompiledShader, nRemotelyCompiledShaderLength, 0, NULL, &d3dblob );
#elif defined( DYNAMIC_SHADER_COMPILE )
		hr = D3DDisassemble( pShader->GetBufferPointer(), pShader->GetBufferSize(), 0, NULL, &d3dblob );
#endif
		Assert( SUCCEEDED( hr ) );
		CUtlBuffer tempBuffer;
		tempBuffer.SetBufferType( true, false );
		intp exampleCommandLineLength = V_strlen( exampleCommandLine );
		tempBuffer.EnsureCapacity( d3dblob->GetBufferSize() + exampleCommandLineLength );
		memcpy( tempBuffer.Base(), exampleCommandLine, exampleCommandLineLength );
		memcpy( tempBuffer.Base<char>() + exampleCommandLineLength, d3dblob->GetBufferPointer(), d3dblob->GetBufferSize() );
		tempBuffer.SeekPut( CUtlBuffer::SEEK_CURRENT, d3dblob->GetBufferSize() + exampleCommandLineLength );

		char filename_asm[MAX_PATH];
		V_sprintf_safe( filename_asm, "%s_%d_%d.asm", pShaderName, nStaticIndex, nDynamicIndex );
		g_pFullFileSystem->WriteFile( filename_asm, "DEFAULT_WRITE_PATH", tempBuffer );
#endif  // #if defined( DYNAMIC_SHADER_COMPILE_WRITE_ASSEMBLY ) && !defined( REMOTE_DYNAMIC_SHADER_COMPILE )

#if defined( DYNAMIC_SHADER_COMPILE ) && !defined( REMOTE_DYNAMIC_SHADER_COMPILE )
		// We keep up with whether we hit a compile error above.
		// If we did, then we likely need to recompile everything again since we could have changed global code.
		if ( bShadersNeedFlush )
		{
			MatFlushShaders();
		}
#endif

#ifdef REMOTE_DYNAMIC_SHADER_COMPILE
		if ( bVertexShader )
		{
			return CreateD3DVertexShader( ( DWORD * )pRemotelyCompiledShader, nRemotelyCompiledShaderLength, pShaderName );
		}
		else
		{
			return CreateD3DPixelShader( ( DWORD * )pRemotelyCompiledShader, 0, nRemotelyCompiledShaderLength, pShaderName ); // hack hack hack!  need to get centroid info from the source
		}
#else // local compile, not remote
		if ( bVertexShader )
		{
			return CreateD3DVertexShader( ( DWORD * )pShader->GetBufferPointer(), pShader->GetBufferSize(), pShaderName );
		}
		else
		{
			return CreateD3DPixelShader( ( DWORD * )pShader->GetBufferPointer(), 0, pShader->GetBufferSize(), pShaderName ); // hack hack hack!  need to get centroid info from the source
		}
#endif
	}
}
#endif

#ifdef DYNAMIC_SHADER_COMPILE	
bool CShaderManager::LoadAndCreateShaders_Dynamic( ShaderLookup_t &lookup, bool bVertexShader )
{	
	const char *pName = m_ShaderSymbolTable.String( lookup.m_Name );
	const ShaderCombos_t *pCombos = FindOrCreateShaderCombos( pName );
	if ( !pCombos )
	{
		return false;
	}

	int numDynamicCombos = pCombos->GetNumDynamicCombos();
	lookup.m_ShaderStaticCombos.m_pHardwareShaders = new HardwareShader_t[numDynamicCombos];
	lookup.m_ShaderStaticCombos.m_nCount = numDynamicCombos;
	lookup.m_ShaderStaticCombos.m_pCreationData = new ShaderStaticCombos_t::ShaderCreationData_t[numDynamicCombos];

	for( int i = 0; i < numDynamicCombos; i++ )
	{
		lookup.m_ShaderStaticCombos.m_pHardwareShaders[i] = INVALID_HARDWARE_SHADER;
	}
	return true;
}
#endif

//-----------------------------------------------------------------------------
// Open the shader file, optionally gets the header
//-----------------------------------------------------------------------------
FileHandle_t CShaderManager::OpenFileAndLoadHeader( const char *pFileName, ShaderHeader_t *pHeader )
{
	FileHandle_t fp = g_pFullFileSystem->Open( pFileName, "rb", "GAME" );
	if ( !fp )
	{
		return FILESYSTEM_INVALID_HANDLE;
	}

	if ( pHeader )
	{
		// read the header 
		g_pFullFileSystem->Read( pHeader, sizeof( ShaderHeader_t ), fp );

		switch ( pHeader->m_nVersion )
		{
			case 4:
				// version with combos done as diffs vs a reference combo
				// vsh/psh or older fxc
				break;

			case 5:
			case 6:
				// version with optimal dictionary and compressed combo block
				break;

			default:
				Assert( 0 );
				Warning( "Shader %s is the wrong version %d, expecting %d\n", pFileName, pHeader->m_nVersion, SHADER_VCS_VERSION_NUMBER );
				g_pFullFileSystem->Close( fp );
				return FILESYSTEM_INVALID_HANDLE;
		}
	}

	return fp;
}

//---------------------------------------------------------------------------------------------------------
// Writes text files named for looked-up shaders.  Used by GL shader translator to dump code for debugging
//---------------------------------------------------------------------------------------------------------
void CShaderManager::WriteTranslatedFile( ShaderLookup_t *pLookup, int dynamicCombo, char *pFileContents, char *pFileExtension )
{
	const char *pName = m_ShaderSymbolTable.String( pLookup->m_Name );
	intp nNumChars = V_strlen( pFileContents );

	CUtlBuffer tempBuffer;
	tempBuffer.SetBufferType( true, false );
	tempBuffer.EnsureCapacity( nNumChars );
	memcpy( tempBuffer.Base(), pFileContents, nNumChars );
	tempBuffer.SeekPut( CUtlBuffer::SEEK_CURRENT, nNumChars );

	char filename[MAX_PATH];
	V_sprintf_safe( filename, "%s_%d_%d.%s", pName, pLookup->m_nStaticIndex, dynamicCombo, pFileExtension );
	g_pFullFileSystem->WriteFile( filename, "DEFAULT_WRITE_PATH", tempBuffer );
}

//-----------------------------------------------------------------------------
// Disassemble a shader for debugging. Writes .asm files.
//-----------------------------------------------------------------------------
void CShaderManager::DisassembleShader( ShaderLookup_t *pLookup, int dynamicCombo, uint8 *pByteCode, size_t size )
{
#if defined( WRITE_ASSEMBLY )
	const char *pName = m_ShaderSymbolTable.String( pLookup->m_Name );

	se::win::com::com_ptr<ID3DBlob> d3dblob;
	HRESULT hr = D3DDisassemble( pByteCode, size, 0, NULL, &d3dblob );
	Assert( SUCCEEDED( hr ) );

	CUtlBuffer tempBuffer;
	tempBuffer.SetBufferType( true, false );
	tempBuffer.EnsureCapacity( d3dblob->GetBufferSize() );
	memcpy( tempBuffer.Base(), d3dblob->GetBufferPointer(), d3dblob->GetBufferSize() );
	tempBuffer.SeekPut( CUtlBuffer::SEEK_CURRENT, d3dblob->GetBufferSize() );

	char filename[MAX_PATH];
	V_sprintf_safe( filename, "%s_%d_%d.asm", pName, pLookup->m_nStaticIndex, dynamicCombo );
	g_pFullFileSystem->WriteFile( filename, "DEFAULT_WRITE_PATH", tempBuffer );
#endif
}

//-----------------------------------------------------------------------------
// Create dynamic combos
//-----------------------------------------------------------------------------
bool CShaderManager::CreateDynamicCombos_Ver4( void *pContext, uint8 *pComboBuffer )
{
	ShaderLookup_t* pLookup = (ShaderLookup_t *)pContext;

	ShaderFileCache_t *pFileCache = &m_ShaderFileCache[pLookup->m_hShaderFileCache];
	ShaderHeader_t *pHeader = &pFileCache->m_Header;

	int nReferenceComboSizeForDiffs = ((ShaderHeader_t_v4 *)pHeader)->m_nDiffReferenceSize;

	uint8 *pReferenceShader = NULL;
	uint8 *pDiffOutputBuffer = NULL;
	if ( nReferenceComboSizeForDiffs )
	{
		// reference combo is *always* the largest combo, so safe worst case size for uncompression buffer
		pReferenceShader = (uint8 *)pFileCache->m_ReferenceCombo.Base();
		pDiffOutputBuffer = (uint8 *)_alloca( nReferenceComboSizeForDiffs ); 
	}

	// build this shader's dynamic combos
	bool bOK = true;
	int nStartingOffset = 0;
	for ( int i = 0; i < pHeader->m_nDynamicCombos; i++ )
	{
		auto &shaderEntry = pLookup->m_pComboDictionary[i];

		if ( shaderEntry.m_Offset == -1 )
		{
			// skipped
			continue;
		}

		if ( !nStartingOffset )
		{
			nStartingOffset = shaderEntry.m_Offset;
		}

		// offsets better be sequentially ascending
		Assert( nStartingOffset <= shaderEntry.m_Offset );

		if ( shaderEntry.m_Size <= 0 )
		{
			// skipped
			continue;
		}

		// get the right byte code from the monolithic buffer
		uint8 *pByteCode = (uint8 *)pComboBuffer + pLookup->m_nDataOffset + shaderEntry.m_Offset - nStartingOffset;
		intp nByteCodeSize = shaderEntry.m_Size;

		if ( pReferenceShader )
		{
			// reference combo better be the largest combo, otherwise memory corruption
			Assert( nReferenceComboSizeForDiffs >= nByteCodeSize );

			// use the differencing algorithm to recover the full shader
			intp nOriginalSize;
			ApplyDiffs( 
				pReferenceShader, 
				pByteCode,
				nReferenceComboSizeForDiffs,
				nByteCodeSize,
				nOriginalSize,
				pDiffOutputBuffer,
				nReferenceComboSizeForDiffs );

			pByteCode = pDiffOutputBuffer;
			nByteCodeSize = nOriginalSize;
		}

#if defined( WRITE_ASSEMBLY )
		DisassembleShader( pLookup, i, pByteCode, nByteCodeSize );
#endif
		HardwareShader_t hardwareShader = INVALID_HARDWARE_SHADER;

		if ( m_bCreateShadersOnDemand )
		{
			auto &shaderCreationData = pLookup->m_ShaderStaticCombos.m_pCreationData[i];

			// cache the code off for later
			shaderCreationData.ByteCode.SetSize( nByteCodeSize );

			V_memcpy( shaderCreationData.ByteCode.Base(), pByteCode, nByteCodeSize );

			shaderCreationData.iCentroidMask = pFileCache->m_bVertexShader ? 0 : pHeader->m_nCentroidMask;
		}
		else
		{
			const char *pShaderName = m_ShaderSymbolTable.String( pLookup->m_Name );
			if ( pFileCache->m_bVertexShader )
			{
				hardwareShader = CreateD3DVertexShader( reinterpret_cast< DWORD *>( pByteCode ), nByteCodeSize, pShaderName );
			}
			else
			{
				hardwareShader = CreateD3DPixelShader( reinterpret_cast< DWORD *>( pByteCode ), pHeader->m_nCentroidMask, nByteCodeSize, pShaderName );
			}

			if ( hardwareShader == INVALID_HARDWARE_SHADER )
			{
				Assert( 0 );
				bOK = false;
				break;
			}
		}
		pLookup->m_ShaderStaticCombos.m_pHardwareShaders[i] = hardwareShader;
	}

	delete [] pLookup->m_pComboDictionary;
	pLookup->m_pComboDictionary = NULL;

	return bOK;
}

//-----------------------------------------------------------------------------
// Create dynamic combos
//-----------------------------------------------------------------------------
static uint32 NextULONG( uint8 * &pData )
{
	// handle unaligned read
	uint32 nRet;
	memcpy( &nRet, pData, sizeof( nRet ) );
	pData += sizeof( nRet );
	return nRet;
}


bool CShaderManager::CreateDynamicCombos_Ver5( void *pContext, uint8 *pComboBuffer, char *debugLabel )
{
	ShaderLookup_t* pLookup = (ShaderLookup_t *)pContext;
	ShaderFileCache_t *pFileCache = &m_ShaderFileCache[pLookup->m_hShaderFileCache];
	uint8 *pCompressedShaders = pComboBuffer + pLookup->m_nDataOffset;

	std::unique_ptr<uint8[]> pUnpackBuffer = std::make_unique<uint8[]>(MAX_SHADER_UNPACKED_BLOCK_SIZE);

	char *debugLabelPtr = debugLabel;	// can be moved to point at something else if need be
	
	// now, loop through all blocks
	bool bOK = true;
	while ( bOK )
	{
		uint32 nBlockSize = NextULONG( pCompressedShaders );
		if ( nBlockSize == std::numeric_limits<uint32>::max() )	
		{
			// any more blocks?
			break;
		}

		switch( nBlockSize & 0xc0000000U )
		{
			case 0:											// bzip2
			{
				// uncompress
				uint32 nOutsize = MAX_SHADER_UNPACKED_BLOCK_SIZE;
				int nRslt = BZ2_bzBuffToBuffDecompress( 
					reinterpret_cast<char *>( pUnpackBuffer.get() ),
					&nOutsize,
					reinterpret_cast<char *>( pCompressedShaders ),
					nBlockSize, 1, 0 );
				if ( nRslt < 0 )
				{
					// errors are negative for bzip
					Assert( 0 );
					Warning( "BZIP Error (%d) decompressing shader", nRslt );
					bOK = false;
				}
				
				pCompressedShaders += nBlockSize;
				nBlockSize = nOutsize;		// how much data there is
			}
			break;

			case 0x80000000:								// uncompressed
			{
				// not compressed, as is
				nBlockSize &= 0x3fffffff;
				memcpy( pUnpackBuffer.get(), pCompressedShaders, nBlockSize );
				pCompressedShaders += nBlockSize;
			}
			break;

			case 0x40000000:								// lzma compressed
			{
				nBlockSize &= 0x3fffffff;
				
				// dimhotepus: Add out size to prevent overflows.
				size_t nOutsize = CLZMA::Uncompress(
					pCompressedShaders,
					pUnpackBuffer.get(),
					MAX_SHADER_UNPACKED_BLOCK_SIZE );
				pCompressedShaders += nBlockSize;
				nBlockSize = nOutsize;		// how much data there is
			}
			break;
			
			default:
			{
				// dimhotepus: Detailed assert and error when compression type is unknown.
				AssertMsg( 0, "Unrecognized shader %s compression type %u = file corrupt?",
					(debugLabel) ? debugLabel : "none", nBlockSize & 0xc0000000U );
				Error("Unrecognized shader %s compression type %u = file corrupt?",
					(debugLabel) ? debugLabel : "none", nBlockSize & 0xc0000000U);
				bOK = false;
			}
		}
		
		uint8 *pReadPtr = pUnpackBuffer.get();
		while ( pReadPtr < pUnpackBuffer.get()+nBlockSize )
		{
			uint32 nCombo_ID = NextULONG( pReadPtr );
			uint32 nShaderSize = NextULONG( pReadPtr );
			
#if defined( WRITE_ASSEMBLY )
			DisassembleShader( pLookup, nCombo_ID, pReadPtr, nBlockSize );
#endif
			HardwareShader_t hardwareShader = INVALID_HARDWARE_SHADER;

			int iIndex = nCombo_ID;
			if ( iIndex >= pLookup->m_nStaticIndex )
				iIndex -= pLookup->m_nStaticIndex;			// ver5 stores combos as full combo, ver6 as dynamic combo # only
			if ( m_bCreateShadersOnDemand )
			{
				auto &shaderCreationData = pLookup->m_ShaderStaticCombos.m_pCreationData[iIndex];

				// cache the code off for later
				shaderCreationData.ByteCode.SetSize( nShaderSize );

				V_memcpy( shaderCreationData.ByteCode.Base(), pReadPtr, nShaderSize );

				shaderCreationData.iCentroidMask = pFileCache->m_bVertexShader ? 0 : pFileCache->m_Header.m_nCentroidMask;
			}
			else
			{
				const char *pShaderName = m_ShaderSymbolTable.String( pLookup->m_Name );

				if ( pFileCache->m_bVertexShader )
				{
#ifdef DX_TO_GL_ABSTRACTION
					// munge the debug label a bit to aid in decoding... catenate the iIndex on the end
					char temp[1024];
					V_sprintf_safe(temp, "%s vs-combo %d", (debugLabel)?debugLabel:"none", iIndex );
					debugLabelPtr = temp;
#endif
					// pass binary code to d3d interface, on GL it will invoke the translator back to asm
					hardwareShader = CreateD3DVertexShader( reinterpret_cast< DWORD *>( pReadPtr ), nShaderSize, pShaderName, debugLabelPtr );
				}
				else
				{
#ifdef DX_TO_GL_ABSTRACTION
					// munge the debug label a bit to aid in decoding... catenate the iIndex on the end
					char temp[1024];
					V_sprintf_safe(temp, "%s ps-combo %d", (debugLabel)?debugLabel:"", iIndex );
					debugLabelPtr = temp;
#endif

					// pass binary code to d3d interface, on GL it will invoke the translator back to asm
					hardwareShader = CreateD3DPixelShader( reinterpret_cast< DWORD *>( pReadPtr ), pFileCache->m_Header.m_nCentroidMask, nShaderSize, pShaderName, debugLabelPtr );
				}
				if ( hardwareShader == INVALID_HARDWARE_SHADER )
				{
					Warning( "failed to create shader\n" );
					Assert( 0 );
					bOK = false;
					break;
				}
			}
			pLookup->m_ShaderStaticCombos.m_pHardwareShaders[iIndex] = hardwareShader;
			pReadPtr += nShaderSize;
		}
	}

	return bOK;
}

//-----------------------------------------------------------------------------
// Static method, called by thread, don't call anything non-threadsafe from handler!!!
//-----------------------------------------------------------------------------
void CShaderManager::QueuedLoaderCallback( void *pContext, void *pContext2, const void *pData, int nSize, LoaderError_t loaderError )
{
	ShaderLookup_t* pLookup = (ShaderLookup_t *)pContext;

	bool bOK = ( loaderError == LOADERERROR_NONE );
	if ( bOK )
	{
		if ( pContext2 )
		{
			// presence denotes diff version
			bOK = s_ShaderManager.CreateDynamicCombos_Ver4( pContext, (uint8 *)pData );
		}
		else
		{
			bOK = s_ShaderManager.CreateDynamicCombos_Ver5( pContext, (uint8 *)pData );
		}
	}
	if ( !bOK )
	{
		pLookup->m_Flags |= SHADER_FAILED_LOAD;
	}
}

//-----------------------------------------------------------------------------
// Loads all shaders
//-----------------------------------------------------------------------------
bool CShaderManager::LoadAndCreateShaders( ShaderLookup_t &lookup, bool bVertexShader, char *debugLabel )
{
	const char *pName = m_ShaderSymbolTable.String( lookup.m_Name );

	// find it in the cache
	// a cache hit prevents costly i/o for static components, i.e. header, ref combo, etc.
	ShaderFileCache_t fileCacheLookup;
	fileCacheLookup.m_Name = lookup.m_Name;
	fileCacheLookup.m_bVertexShader = bVertexShader;
	intp fileCacheIndex = m_ShaderFileCache.Find( fileCacheLookup );
	if ( fileCacheIndex == m_ShaderFileCache.InvalidIndex() )
	{
		// not found, create a new entry
		fileCacheIndex = m_ShaderFileCache.AddToTail();
	}

	lookup.m_hShaderFileCache = fileCacheIndex;

	// fetch from cache
	ShaderFileCache_t &pFileCache = m_ShaderFileCache[fileCacheIndex];
	ShaderHeader_t &pHeader = pFileCache.m_Header;

	FileHandle_t hFile = FILESYSTEM_INVALID_HANDLE;
	if ( pFileCache.IsValid() )
	{
		const char *shaderFileName = m_ShaderSymbolTable.String( pFileCache.m_Filename );
		// using cached header, just open file, no read of header needed
		hFile = OpenFileAndLoadHeader( shaderFileName, NULL );
		if ( !hFile )
		{
			// shouldn't happen
			AssertMsg( false, "Couldn't load shader '%s'.\n", shaderFileName );
			return false;
		}
	}
	else
	{
		V_memset( &pHeader, 0, sizeof( ShaderHeader_t ) );

		// try the vsh/psh dir first
		char filename[MAX_PATH];
		V_sprintf_safe( filename, "shaders\\%s\\%s" SHADER_FNAME_EXTENSION, bVertexShader ? "vsh" : "psh", pName );
		hFile = OpenFileAndLoadHeader( filename, &pHeader );
		if ( !hFile )
		{
#ifdef DYNAMIC_SHADER_COMPILE
			// Dynamically compile if it's HLSL.
			return LoadAndCreateShaders_Dynamic( lookup, bVertexShader );
#else
			// next, try the fxc dir
			V_sprintf_safe( filename, "shaders\\fxc\\%s" SHADER_FNAME_EXTENSION, pName );
			hFile = OpenFileAndLoadHeader( filename, &pHeader );
			if ( !hFile )
			{
				lookup.m_Flags |= SHADER_FAILED_LOAD;
				Warning( "Couldn't load %s shader %s.\n", bVertexShader ? "vertex" : "pixel", pName );
				return false;
			}
#endif
		}

		lookup.m_Flags = pHeader.m_nFlags;

		pFileCache.m_Name = lookup.m_Name;
		pFileCache.m_Filename = m_ShaderSymbolTable.AddString( filename );
		pFileCache.m_bVertexShader = bVertexShader;

		if ( pFileCache.IsOldVersion() )
		{ 
			int referenceComboSize = ((ShaderHeader_t_v4 &)pHeader).m_nDiffReferenceSize;
			if ( referenceComboSize )
			{
				// cache the reference combo
				pFileCache.m_ReferenceCombo.EnsureCapacity( referenceComboSize );
				g_pFullFileSystem->Read( pFileCache.m_ReferenceCombo.Base(), referenceComboSize, hFile );
			}
		}
		else
		{
			// cache the dictionary
			pFileCache.m_StaticComboRecords.EnsureCount( pHeader.m_nNumStaticCombos );
			g_pFullFileSystem->Read( pFileCache.m_StaticComboRecords.Base(),
				pHeader.m_nNumStaticCombos * sizeof( StaticComboRecord_t ), hFile );
			if ( pFileCache.IsVersion6() )
			{
				// read static combo alias records
				int nNumDups;
				g_pFullFileSystem->Read( &nNumDups, sizeof( nNumDups ), hFile );
				if ( nNumDups )
				{
					pFileCache.m_StaticComboDupRecords.EnsureCount( nNumDups );
					g_pFullFileSystem->Read( pFileCache.m_StaticComboDupRecords.Base(),
						nNumDups * sizeof( StaticComboAliasRecord_t ), hFile );
				}
			}
		}
	}

	RunCodeAtScopeExit(g_pFullFileSystem->Close( hFile ));

	// FIXME: should make lookup and ShaderStaticCombos_t are pool allocated.
	lookup.m_ShaderStaticCombos.m_nCount = pHeader.m_nDynamicCombos;
	lookup.m_ShaderStaticCombos.m_pHardwareShaders = new HardwareShader_t[pHeader.m_nDynamicCombos];
	if ( m_bCreateShadersOnDemand )
	{
		lookup.m_ShaderStaticCombos.m_pCreationData =
			new ShaderStaticCombos_t::ShaderCreationData_t[pHeader.m_nDynamicCombos];
	}
	for ( int i = 0; i < pHeader.m_nDynamicCombos; i++ )
	{
		lookup.m_ShaderStaticCombos.m_pHardwareShaders[i] = INVALID_HARDWARE_SHADER;
	}

	int nStartingOffset = 0;
	int nEndingOffset = 0;

	if ( pFileCache.IsOldVersion() )
	{
		int nDictionaryOffset = sizeof( ShaderHeader_t ) + ((ShaderHeader_t_v4 &)pHeader).m_nDiffReferenceSize;

		// read in shader's dynamic combos directory
		lookup.m_pComboDictionary = new ShaderDictionaryEntry_t[pHeader.m_nDynamicCombos];
		g_pFullFileSystem->Seek( hFile, nDictionaryOffset + lookup.m_nStaticIndex * sizeof( ShaderDictionaryEntry_t ), FILESYSTEM_SEEK_HEAD );
		g_pFullFileSystem->Read( lookup.m_pComboDictionary, pHeader.m_nDynamicCombos * sizeof( ShaderDictionaryEntry_t ), hFile );

		// want single read of all this shader's dynamic combos into a target buffer
		// shaders are written sequentially, determine starting offset and length
		for ( int i = 0; i < pHeader.m_nDynamicCombos; i++ )
		{
			if ( lookup.m_pComboDictionary[i].m_Offset == -1 )
			{
				// skipped
				continue;
			}

			// ensure offsets are in fact sequentially ascending 
			Assert( lookup.m_pComboDictionary[i].m_Offset >= nStartingOffset && lookup.m_pComboDictionary[i].m_Size >= 0 );

			if ( !nStartingOffset )
			{
				nStartingOffset = lookup.m_pComboDictionary[i].m_Offset;
			}
			nEndingOffset = lookup.m_pComboDictionary[i].m_Offset + lookup.m_pComboDictionary[i].m_Size;
		}
		if ( !nStartingOffset )
		{
			Warning( "Shader '%s' - All dynamic combos skipped. This is bad!\n",
				m_ShaderSymbolTable.String( pFileCache.m_Filename ) );
			return false;
		}
	}
	else
	{
		intp nStaticComboIdx = pFileCache.FindCombo( lookup.m_nStaticIndex / pFileCache.m_Header.m_nDynamicCombos );
		if ( nStaticComboIdx == -1 )
		{
			lookup.m_Flags |= SHADER_FAILED_LOAD;
			Warning( "Shader '%s' - Couldn't load combo %d of shader (dyn=%d).\n",
				m_ShaderSymbolTable.String( pFileCache.m_Filename ), lookup.m_nStaticIndex, pFileCache.m_Header.m_nDynamicCombos );
			return false;
		}

		nStartingOffset = pFileCache.m_StaticComboRecords[nStaticComboIdx].m_nFileOffset;
		nEndingOffset = pFileCache.m_StaticComboRecords[nStaticComboIdx+1].m_nFileOffset;
	}

	// align offsets for unbuffered optimal i/o - fastest i/o possible
	unsigned nOffsetAlign, nSizeAlign, nBufferAlign;
	g_pFullFileSystem->GetOptimalIOConstraints( hFile, &nOffsetAlign, &nSizeAlign, &nBufferAlign );
	unsigned nAlignedOffset = AlignValue( ( nStartingOffset - nOffsetAlign ) + 1, nOffsetAlign );
	unsigned nAlignedBytesToRead = AlignValue( nEndingOffset - nAlignedOffset, nSizeAlign );

	// used for adjusting provided buffer to actual data
	lookup.m_nDataOffset = nStartingOffset - nAlignedOffset;

	bool bOK = true;
	{
		// single optimal read of all dynamic combos into monolithic buffer
		uint8 *pOptimalBuffer = (uint8 *)
			g_pFullFileSystem->AllocOptimalReadBuffer( hFile, nAlignedBytesToRead, nAlignedOffset );
		RunCodeAtScopeExit(g_pFullFileSystem->FreeOptimalReadBuffer( pOptimalBuffer ));
		
		g_pFullFileSystem->Seek( hFile, nAlignedOffset, FILESYSTEM_SEEK_HEAD );
		g_pFullFileSystem->Read( pOptimalBuffer, nAlignedBytesToRead, hFile );

		if ( pFileCache.IsOldVersion() )
		{
			bOK = CreateDynamicCombos_Ver4( &lookup, pOptimalBuffer );
		}
		else
		{
			bOK = CreateDynamicCombos_Ver5( &lookup, pOptimalBuffer, debugLabel );
		}
	}

	if ( !bOK )
	{
		lookup.m_Flags |= SHADER_FAILED_LOAD;
	}

	return bOK;
}


#ifdef DX_TO_GL_ABSTRACTION
// if shaders are changed in a way that requires the client-side cache to be invalidated,
// increment this string - such changes include combo changes (skips, adding combos)
const char *k_pszShaderCacheRootKey = "glshadercachev002";
#endif

void	CShaderManager::SaveShaderCache( char *cacheName )
{
#ifdef DX_TO_GL_ABSTRACTION	// must ifdef, it uses calls which don't exist in the real DX9 interface

	KeyValuesAD pProgramCache( k_pszShaderCacheRootKey );
	if ( !pProgramCache )
	{
		Warning( "Could not write to program cache file!\n" );
		return;
	}

	int i=0;
	GLMShaderPairInfo info;

	do
	{
		Dx9Device()->QueryShaderPair( i, &info );
		
		if (info.m_status==1)
		{
			// found one
			// extract values of interest which represent a pair of shaders
			
			if (info.m_vsName[0] && info.m_psName[0] && (info.m_vsDynamicIndex > -1) && (info.m_psDynamicIndex > -1) )
			{
				// make up a key - this thing is really a list of tuples, so need not be keyed by anything particular
				KeyValues *pProgramKey = pProgramCache->CreateNewKey();
				Assert( pProgramKey );

				pProgramKey->SetString	( "vs", info.m_vsName );
				pProgramKey->SetString	( "ps", info.m_psName );

				pProgramKey->SetInt		( "vs_static", info.m_vsStaticIndex );
				pProgramKey->SetInt		( "ps_static", info.m_psStaticIndex );

				pProgramKey->SetInt		( "vs_dynamic", info.m_vsDynamicIndex );
				pProgramKey->SetInt		( "ps_dynamic", info.m_psDynamicIndex );
			}
		}
		i++;
	} while( info.m_status >= 0 );
	
	pProgramCache->SaveToFile( g_pFullFileSystem, cacheName, "MOD" );	
	// done! whew
#endif
}

bool	CShaderManager::LoadShaderCache( char *cacheName )
{
#ifdef DX_TO_GL_ABSTRACTION
	KeyValuesAD pProgramCache( "" );
	bool found = pProgramCache->LoadFromFile( g_pFullFileSystem, cacheName, "MOD" );

	if ( !found ) 
	{
		Warning( "Could not load program cache file %s\n", cacheName );
		return false;
	}

	    if ( Q_stricmp( pProgramCache->GetName(), k_pszShaderCacheRootKey ) ) 
	    {
			Warning( "Ignoring out-of-date shader cache (%s) with root key %s\n", cacheName, pProgramCache->GetName() );
	        return false;
	    }
    	
	int nTotalLinkedShaders = 0;
	int nTotalKeyValues = 0;

	// walk the table..
	FOR_EACH_SUBKEY( pProgramCache, pProgramKey )
	{
		nTotalKeyValues++;

		// extract values decribing the specific active pair
		// then see if either stage needs a compilation done
		// then proceed to link
		
		KeyValues *pValue = pProgramKey->GetFirstValue();
		if (!pValue)
			continue;
		const char *pVertexShaderName = pValue->GetString();

		pValue = pValue->GetNextValue();
		if (!pValue)
			continue;
		const char *pPixelShaderName = pValue->GetString();

		pValue = pValue->GetNextValue();
		if (!pValue)
			continue;
		int nVertexShaderStaticIndex = pValue->GetInt();

		pValue = pValue->GetNextValue();
		if (!pValue)
			continue;
		int nPixelShaderStaticIndex = pValue->GetInt();

		pValue = pValue->GetNextValue();
		if (!pValue)
			continue;
		int nVertexShaderDynamicIndex = pValue->GetInt();

		pValue = pValue->GetNextValue();
		if (!pValue)
			continue;
		int nPixelShaderDynamicIndex = pValue->GetInt();

		ShaderLookup_t vshLookup;
		vshLookup.m_Name = m_ShaderSymbolTable.AddString( pVertexShaderName ); // TODO: use String() here and catch this odd case
		vshLookup.m_nStaticIndex = nVertexShaderStaticIndex;
		VertexShader_t vertexShader = m_VertexShaderDict.Find( vshLookup );

		// if the VS was not found - now is the time to build it
		if( vertexShader == m_VertexShaderDict.InvalidIndex())
		{
			char	temp[1024];
				
			V_snprintf( temp, sizeof(temp), "vs-file %s vs-index %d", pVertexShaderName, nVertexShaderStaticIndex );
			CreateVertexShader( pVertexShaderName, nVertexShaderStaticIndex, temp );
			
			// this one should not fail
			vertexShader = m_VertexShaderDict.Find( vshLookup );
			Assert( vertexShader != m_VertexShaderDict.InvalidIndex());
		}
		
		ShaderLookup_t pshLookup;
		pshLookup.m_Name = m_ShaderSymbolTable.AddString( pPixelShaderName );
		pshLookup.m_nStaticIndex = nPixelShaderStaticIndex;
		PixelShader_t pixelShader = m_PixelShaderDict.Find( pshLookup );

		if( pixelShader == m_PixelShaderDict.InvalidIndex())
		{
			char	temp[1024];
			
			V_snprintf( temp, sizeof(temp), "ps-file %s ps-index %d", pPixelShaderName, nPixelShaderStaticIndex );
			CreatePixelShader( pPixelShaderName, nPixelShaderStaticIndex, temp );
			
			// this one should not fail
			pixelShader = m_PixelShaderDict.Find( pshLookup );
			Assert( pixelShader != m_PixelShaderDict.InvalidIndex());
		}
		
		// If we found both shaders, do the link!
		if ( ( vertexShader != m_VertexShaderDict.InvalidIndex() ) && ( pixelShader != m_PixelShaderDict.InvalidIndex() ) )
		{
			// double check that the hardware shader arrays are actually instantiated.. bail on the attempt if not (odd...)
			if (m_VertexShaderDict[vertexShader].m_ShaderStaticCombos.m_pHardwareShaders && m_PixelShaderDict[pixelShader].m_ShaderStaticCombos.m_pHardwareShaders)
			{
				// and sanity check the indices..
				if ( (nVertexShaderDynamicIndex>=0) && (nPixelShaderDynamicIndex>=0) )
				{
					HardwareShader_t hardwareVertexShader = m_VertexShaderDict[vertexShader].m_ShaderStaticCombos.m_pHardwareShaders[nVertexShaderDynamicIndex];
					HardwareShader_t hardwarePixelShader = m_PixelShaderDict[pixelShader].m_ShaderStaticCombos.m_pHardwareShaders[nPixelShaderDynamicIndex];

					if ( ( hardwareVertexShader != INVALID_HARDWARE_SHADER ) && ( hardwarePixelShader != INVALID_HARDWARE_SHADER ) )
					{
						if ( S_OK != Dx9Device()->LinkShaderPair( (IDirect3DVertexShader9 *)hardwareVertexShader, (IDirect3DPixelShader9 *)hardwarePixelShader ) )
						{
							Warning( "Could not link OpenGL shaders: %s (%d, %d) : %s (%d, %d)\n", pVertexShaderName, nVertexShaderStaticIndex, nVertexShaderDynamicIndex, pPixelShaderName, nPixelShaderStaticIndex, nPixelShaderDynamicIndex );
						}
						else
						{
							nTotalLinkedShaders++;
						}
					}
				}
				else
				{
					Warning( "nVertexShaderDynamicIndex or nPixelShaderDynamicIndex was negative\n" );
				}
			}
			else
			{
				Warning( "m_pHardwareShaders was null\n" );
			}
		}
		else
		{
			Warning( "Invalid shader linkage: %s (%d, %d) : %s (%d, %d)\n", pVertexShaderName, nVertexShaderStaticIndex, nVertexShaderDynamicIndex, pPixelShaderName, nPixelShaderStaticIndex, nPixelShaderDynamicIndex );
		}
	}

	Msg( "Loaded program cache file \"%s\", total keyvalues: %i, total successfully linked: %i\n", cacheName, nTotalKeyValues, nTotalLinkedShaders );

	return true;

#else
	return false;	// have to return a value on Windows build to appease compiler
#endif
}


	
//-----------------------------------------------------------------------------
// Creates and destroys vertex shaders
//-----------------------------------------------------------------------------
VertexShader_t CShaderManager::CreateVertexShader( const char *pFileName, int nStaticVshIndex, char *debugLabel )
{
	MEM_ALLOC_CREDIT();

	if ( !pFileName )
	{
		return INVALID_SHADER;
	}

	ShaderLookup_t lookup;
	lookup.m_Name = m_ShaderSymbolTable.AddString( pFileName );
	lookup.m_nStaticIndex = nStaticVshIndex;
	VertexShader_t shader = m_VertexShaderDict.Find( lookup );
	if ( shader == m_VertexShaderDict.InvalidIndex() )
	{
		shader = m_VertexShaderDict.AddToTail( lookup );
		if ( !LoadAndCreateShaders( m_VertexShaderDict[shader], true, debugLabel ) )
		{
			return INVALID_SHADER;
		}
	}
	m_VertexShaderDict[shader].IncRefCount();
	return shader;
}

//-----------------------------------------------------------------------------
// Create pixel shader
//-----------------------------------------------------------------------------
PixelShader_t CShaderManager::CreatePixelShader( const char *pFileName, int nStaticPshIndex, char *debugLabel )
{
	MEM_ALLOC_CREDIT();

	if ( !pFileName )
	{
		return INVALID_SHADER;
	}

	ShaderLookup_t lookup;
	lookup.m_Name = m_ShaderSymbolTable.AddString( pFileName );
	lookup.m_nStaticIndex = nStaticPshIndex;
	PixelShader_t shader = m_PixelShaderDict.Find( lookup );
	if ( shader == m_PixelShaderDict.InvalidIndex() )
	{
		shader = m_PixelShaderDict.AddToTail( lookup );
		if ( !LoadAndCreateShaders( m_PixelShaderDict[shader], false, debugLabel ) )
		{
			return INVALID_SHADER;
		}
	}
	m_PixelShaderDict[shader].IncRefCount();
	return shader;
}

//-----------------------------------------------------------------------------
// Clear the refCounts to zero
//-----------------------------------------------------------------------------
void CShaderManager::ClearVertexAndPixelShaderRefCounts()
{
	for ( VertexShader_t vshIndex = m_VertexShaderDict.Head(); 
		 vshIndex != m_VertexShaderDict.InvalidIndex(); 
		 vshIndex = m_VertexShaderDict.Next( vshIndex ) )
	{
		m_VertexShaderDict[vshIndex].m_nRefCount = 0;
	}

	for ( PixelShader_t pshIndex = m_PixelShaderDict.Head(); 
		 pshIndex != m_PixelShaderDict.InvalidIndex(); 
		 pshIndex = m_PixelShaderDict.Next( pshIndex ) )
	{
		m_PixelShaderDict[pshIndex].m_nRefCount = 0;
	}
}

//-----------------------------------------------------------------------------
// Destroy all shaders that have no reference
//-----------------------------------------------------------------------------
void CShaderManager::PurgeUnusedVertexAndPixelShaders()
{
	#ifdef DX_TO_GL_ABSTRACTION
		if (mat_autosave_glshaders.GetInt())
		{
			SaveShaderCache("glshaders.cfg");
		}
		return;	// don't purge shaders, it's too costly to put them back
	#endif
	
	// iterate vertex shaders
	for ( VertexShader_t vshIndex = m_VertexShaderDict.Head(); vshIndex != m_VertexShaderDict.InvalidIndex(); )
	{
		Assert( m_VertexShaderDict[vshIndex].m_nRefCount >= 0 );

		// Get the next one before we potentially delete the current one.
		VertexShader_t next = m_VertexShaderDict.Next( vshIndex );
		if ( m_VertexShaderDict[vshIndex].m_nRefCount <= 0 )
		{
			DestroyVertexShader( vshIndex );
		}
		vshIndex = next;
	}

	// iterate pixel shaders
	for ( PixelShader_t pshIndex = m_PixelShaderDict.Head(); pshIndex != m_PixelShaderDict.InvalidIndex(); )
	{
		Assert( m_PixelShaderDict[pshIndex].m_nRefCount >= 0 );

		// Get the next one before we potentially delete the current one.
		PixelShader_t next = m_PixelShaderDict.Next( pshIndex );
		if ( m_PixelShaderDict[pshIndex].m_nRefCount <= 0 )
		{
			DestroyPixelShader( pshIndex );
		}
		pshIndex = next;
	}
}



void* CShaderManager::GetCurrentVertexShader()
{
	return (void*)m_HardwareVertexShader;
}

void* CShaderManager::GetCurrentPixelShader()
{
	return (void*)m_HardwarePixelShader;
}


//-----------------------------------------------------------------------------
// The low-level dx call to set the vertex shader state
//-----------------------------------------------------------------------------
void CShaderManager::SetVertexShaderState( HardwareShader_t shader, DataCacheHandle_t hCachedShader )
{
	if ( m_HardwareVertexShader != shader )
	{
		RECORD_COMMAND( DX8_SET_VERTEX_SHADER, 1 );
		RECORD_PTR( shader ); // hack hack hack

		Dx9Device()->SetVertexShader( (IDirect3DVertexShader9*)shader );
		m_HardwareVertexShader = shader;
	}
}

void CShaderManager::BindVertexShader( VertexShaderHandle_t hVertexShader )
{
	HardwareShader_t hHardwareShader = m_RawVertexShaderDict[ (VertexShaderIndex_t)hVertexShader] ;
	SetVertexShaderState( hHardwareShader );
}


//-----------------------------------------------------------------------------
// Sets a particular vertex shader as the current shader
//-----------------------------------------------------------------------------
void CShaderManager::SetVertexShader( VertexShader_t shader )
{
	// Determine which vertex shader to use...
	if ( shader == INVALID_SHADER )
	{
		SetVertexShaderState( 0 );
		return;
	}

	int vshIndex = m_nVertexShaderIndex;
	Assert( vshIndex >= 0 );
	if( vshIndex < 0 )
	{
		vshIndex = 0;
	}

	ShaderLookup_t &vshLookup = m_VertexShaderDict[shader];
//	Warning( "vsh: %s static: %d dynamic: %d\n", m_ShaderSymbolTable.String( vshLookup.m_Name ),
//		vshLookup.m_nStaticIndex, m_nVertexShaderIndex );

#ifdef DYNAMIC_SHADER_COMPILE
	HardwareShader_t &dxshader = m_VertexShaderDict[shader].m_ShaderStaticCombos.m_pHardwareShaders[vshIndex];
	if ( dxshader == INVALID_HARDWARE_SHADER )
	{
		// compile it since we haven't already!
		dxshader = CompileShader( m_ShaderSymbolTable.String( vshLookup.m_Name ), vshLookup.m_nStaticIndex, vshIndex, true );
		Assert( dxshader != INVALID_HARDWARE_SHADER );
	}
#else
	if ( vshLookup.m_Flags & SHADER_FAILED_LOAD )
	{
		AssertMsg( false, "Can't load vertex shader '%s'.\n", m_ShaderSymbolTable.String( vshLookup.m_Name ) );
		return;
	}
#ifdef _DEBUG
	vshDebugIndex = (vshDebugIndex + 1) % MAX_SHADER_HISTORY;
	V_strcpy_safe( vshDebugName[vshDebugIndex], m_ShaderSymbolTable.String( vshLookup.m_Name ) );
#endif
	// dimhotepus: Check vertex shader in range.
	AssertMsg( vshIndex < vshLookup.m_ShaderStaticCombos.m_nCount,
		"Vertex shader '%s' on index %d is out of pixel shaders range [0, %d).",
		vshDebugName[pshDebugIndex],
		vshIndex,
		vshLookup.m_ShaderStaticCombos.m_nCount );
	HardwareShader_t dxshader = vshLookup.m_ShaderStaticCombos.m_pHardwareShaders[vshIndex];
#endif

	if ( dxshader == INVALID_HARDWARE_SHADER && m_bCreateShadersOnDemand )
	{
#ifdef DYNAMIC_SHADER_COMPILE
		ShaderStaticCombos_t::ShaderCreationData_t *pCreationData = &m_VertexShaderDict[shader].m_ShaderStaticCombos.m_pCreationData[vshIndex];
#else
		ShaderStaticCombos_t::ShaderCreationData_t *pCreationData = &vshLookup.m_ShaderStaticCombos.m_pCreationData[vshIndex];
#endif

		dxshader = CreateD3DVertexShader( ( DWORD * )pCreationData->ByteCode.Base(), pCreationData->ByteCode.Count(), m_ShaderSymbolTable.String( vshLookup.m_Name ) );

#ifdef DYNAMIC_SHADER_COMPILE 
		// copy the compiled shader handle back to wherever it's supposed to be stored
		m_VertexShaderDict[shader].m_ShaderStaticCombos.m_pHardwareShaders[vshIndex] = dxshader;
#else
		vshLookup.m_ShaderStaticCombos.m_pHardwareShaders[vshIndex] = dxshader;
#endif
	}

	AssertMsg( dxshader != INVALID_HARDWARE_SHADER, "Failed to set vertex shader '%s'.\n", m_ShaderSymbolTable.String( vshLookup.m_Name ) );

#ifndef DYNAMIC_SHADER_COMPILE
	if( !dxshader )
	{
		Error( "!!!!!Using invalid shader combo!!!!!  Consult a programmer and tell them to build debug materialsystem.dll and stdshader*.dll.  Run with \"mat_bufferprimitives 0\" and look for CMaterial in the call stack and see what m_pDebugName is.  You are likely using a shader combo that has been skipped.\n" );
	}
#endif

	SetVertexShaderState( dxshader );
}

//-----------------------------------------------------------------------------
// The low-level dx call to set the pixel shader state
//-----------------------------------------------------------------------------
void CShaderManager::SetPixelShaderState( HardwareShader_t shader, DataCacheHandle_t hCachedShader )
{
	if ( m_HardwarePixelShader != shader )
	{		
		Dx9Device()->SetPixelShader( (IDirect3DPixelShader*)shader );		
		m_HardwarePixelShader = shader;
	}
}

void CShaderManager::BindPixelShader( PixelShaderHandle_t hPixelShader )
{
	HardwareShader_t hHardwareShader = m_RawPixelShaderDict[ (PixelShaderIndex_t)hPixelShader ];
	SetPixelShaderState( hHardwareShader );
}


//-----------------------------------------------------------------------------
// Sets a particular pixel shader as the current shader
//-----------------------------------------------------------------------------
void CShaderManager::SetPixelShader( PixelShader_t shader )
{
	if ( shader == INVALID_SHADER )
	{
		SetPixelShaderState( 0 );
		return;
	}

	int pshIndex = m_nPixelShaderIndex;
	Assert( pshIndex >= 0 );
	ShaderLookup_t &pshLookup = m_PixelShaderDict[shader];
//	Warning( "psh: %s static: %d dynamic: %d\n", m_ShaderSymbolTable.String( pshLookup.m_Name ),
//		pshLookup.m_nStaticIndex, m_nPixelShaderIndex );

#ifdef DYNAMIC_SHADER_COMPILE
	HardwareShader_t &dxshader = m_PixelShaderDict[shader].m_ShaderStaticCombos.m_pHardwareShaders[pshIndex];
	if ( dxshader == INVALID_HARDWARE_SHADER )
	{
		// compile it since we haven't already!
		dxshader = CompileShader( m_ShaderSymbolTable.String( pshLookup.m_Name ), pshLookup.m_nStaticIndex, pshIndex, false );
//		Assert( dxshader != INVALID_HARDWARE_SHADER );
	}
#else
	if ( pshLookup.m_Flags & SHADER_FAILED_LOAD )
	{
		AssertMsg( false, "Can't load pixel shader '%s'.\n", m_ShaderSymbolTable.String( pshLookup.m_Name ) );
		return;
	}
#ifdef _DEBUG
	pshDebugIndex = (pshDebugIndex + 1) % MAX_SHADER_HISTORY;
	V_strcpy_safe( pshDebugName[pshDebugIndex], m_ShaderSymbolTable.String( pshLookup.m_Name ) );
#endif
	// dimhotepus: Check pixel shader in range.
	AssertMsg( pshIndex < pshLookup.m_ShaderStaticCombos.m_nCount,
		"Pixel shader '%s' on index %d is out of pixel shaders range [0, %d).",
		pshDebugName[pshDebugIndex],
		pshIndex,
		pshLookup.m_ShaderStaticCombos.m_nCount );
	HardwareShader_t dxshader = pshLookup.m_ShaderStaticCombos.m_pHardwareShaders[pshIndex];
#endif

	if ( dxshader == INVALID_HARDWARE_SHADER && m_bCreateShadersOnDemand )
	{
#ifdef DYNAMIC_SHADER_COMPILE
		ShaderStaticCombos_t::ShaderCreationData_t *pCreationData = &m_PixelShaderDict[shader].m_ShaderStaticCombos.m_pCreationData[pshIndex];
#else
		ShaderStaticCombos_t::ShaderCreationData_t *pCreationData = &pshLookup.m_ShaderStaticCombos.m_pCreationData[pshIndex];
#endif

		const char *pShaderName = m_ShaderSymbolTable.String( pshLookup.m_Name );
		dxshader = CreateD3DPixelShader( ( DWORD * )pCreationData->ByteCode.Base(), pCreationData->iCentroidMask, pCreationData->ByteCode.Count(), pShaderName );

#ifdef DYNAMIC_SHADER_COMPILE 
		// copy the compiled shader handle back to wherever it's supposed to be stored
		m_PixelShaderDict[shader].m_ShaderStaticCombos.m_pHardwareShaders[pshIndex] = dxshader;
#else
		pshLookup.m_ShaderStaticCombos.m_pHardwareShaders[pshIndex] = dxshader;
#endif
	}

	AssertMsg( dxshader != INVALID_HARDWARE_SHADER, "Failed to set pixel shader '%s'.\n", m_ShaderSymbolTable.String( pshLookup.m_Name ) );
	SetPixelShaderState( dxshader );
}

//-----------------------------------------------------------------------------
// Resets the shader state
//-----------------------------------------------------------------------------
void CShaderManager::ResetShaderState()
{
	// This will force the calls to SetVertexShader + SetPixelShader to actually set the state
	m_HardwareVertexShader = (HardwareShader_t)-1;
	m_HardwarePixelShader = (HardwareShader_t)-1;

	SetVertexShader( INVALID_SHADER );
	SetPixelShader( INVALID_SHADER );
}

//-----------------------------------------------------------------------------
// Destroy a particular vertex shader
//-----------------------------------------------------------------------------
void CShaderManager::DestroyVertexShader( VertexShader_t vertexShader )
{
	ShaderStaticCombos_t &combos = m_VertexShaderDict[vertexShader].m_ShaderStaticCombos;
	for ( int i = 0; i < combos.m_nCount; i++ )
	{
		if ( combos.m_pHardwareShaders[i] != INVALID_HARDWARE_SHADER )
		{
			se::win::com::com_ptr<IDirect3DVertexShader9> shader;
			shader.Attach(static_cast<IDirect3DVertexShader9*>(combos.m_pHardwareShaders[i]));

			UnregisterVS( shader );
		}
	}

	delete [] combos.m_pHardwareShaders;
	combos.m_pHardwareShaders = NULL;

	delete [] combos.m_pCreationData;
	combos.m_pCreationData = NULL;

	m_VertexShaderDict.Remove( vertexShader );
}

//-----------------------------------------------------------------------------
// Destroy a particular pixel shader
//-----------------------------------------------------------------------------
void CShaderManager::DestroyPixelShader( PixelShader_t pixelShader )
{
	ShaderStaticCombos_t &combos = m_PixelShaderDict[pixelShader].m_ShaderStaticCombos;
	for ( int i = 0; i < combos.m_nCount; i++ )
	{
		if ( combos.m_pHardwareShaders[i] != INVALID_HARDWARE_SHADER )
		{
			se::win::com::com_ptr<IDirect3DPixelShader> shader;
			shader.Attach(static_cast<IDirect3DPixelShader*>(combos.m_pHardwareShaders[i]));

			UnregisterPS( shader );
		}
	}

	delete [] combos.m_pHardwareShaders;
	combos.m_pHardwareShaders = NULL;

	delete [] combos.m_pCreationData;
	combos.m_pCreationData = NULL;

	m_PixelShaderDict.Remove( pixelShader );
}


//-----------------------------------------------------------------------------
// Destroys all shaders
//-----------------------------------------------------------------------------
void CShaderManager::DestroyAllShaders( void )
{
	for ( VertexShader_t vshIndex = m_VertexShaderDict.Head(); 
		 vshIndex != m_VertexShaderDict.InvalidIndex(); )
	{
		Assert( m_VertexShaderDict[vshIndex].m_nRefCount >= 0 );
		VertexShader_t next = m_VertexShaderDict.Next( vshIndex );
		DestroyVertexShader( vshIndex );
		vshIndex = next;
	}

	for ( PixelShader_t pshIndex = m_PixelShaderDict.Head(); 
		 pshIndex != m_PixelShaderDict.InvalidIndex(); )
	{
		Assert( m_PixelShaderDict[pshIndex].m_nRefCount >= 0 );
		PixelShader_t next = m_PixelShaderDict.Next( pshIndex );
		DestroyPixelShader( pshIndex );
		pshIndex = next;
	}

	// invalidate the file cache
	m_ShaderFileCache.Purge();
}

//-----------------------------------------------------------------------------
// print all vertex and pixel shaders along with refcounts to the console
//-----------------------------------------------------------------------------
void CShaderManager::SpewVertexAndPixelShaders( void )
{
	// only spew a populated shader file cache
	Msg( "\nShader File Cache:\n" );
	for ( auto cacheIndex = m_ShaderFileCache.Head(); 
		 cacheIndex != m_ShaderFileCache.InvalidIndex();
		 cacheIndex = m_ShaderFileCache.Next( cacheIndex ) )
	{
		ShaderFileCache_t *pCache = &m_ShaderFileCache[cacheIndex];
		Msg( "Total Combos:%9d Static:%9d Dynamic:%7d SeekTable:%7d Ver:%d '%s'\n", 
			pCache->m_Header.m_nTotalCombos, 
			pCache->m_Header.m_nTotalCombos/pCache->m_Header.m_nDynamicCombos,
			pCache->m_Header.m_nDynamicCombos,
			pCache->IsOldVersion() ? 0 : pCache->m_Header.m_nNumStaticCombos,
			pCache->m_Header.m_nVersion,
			m_ShaderSymbolTable.String( pCache->m_Filename ) );
	}
	Msg( "\n" );

	// spew vertex shader dictionary
	int totalVertexShaders = 0;
	int totalVertexShaderSets = 0;
	for ( VertexShader_t vshIndex = m_VertexShaderDict.Head();
		 vshIndex != m_VertexShaderDict.InvalidIndex();
		 vshIndex = m_VertexShaderDict.Next( vshIndex ) )
	{
		const ShaderLookup_t &lookup = m_VertexShaderDict[vshIndex];
		const char *pName = m_ShaderSymbolTable.String( lookup.m_Name );
		Msg( "vsh 0x%8.8x: static combo:%9d dynamic combos:%6d refcount:%4d \"%s\"\n", vshIndex,
			lookup.m_nStaticIndex, lookup.m_ShaderStaticCombos.m_nCount,
			lookup.m_nRefCount, pName );
		totalVertexShaders += lookup.m_ShaderStaticCombos.m_nCount;
		totalVertexShaderSets++;
	}

	// spew pixel shader dictionary
	int totalPixelShaders = 0;
	int totalPixelShaderSets = 0;
	for ( PixelShader_t pshIndex = m_PixelShaderDict.Head();
		 pshIndex != m_PixelShaderDict.InvalidIndex();
		 pshIndex = m_PixelShaderDict.Next( pshIndex ) )
	{
		const ShaderLookup_t &lookup = m_PixelShaderDict[pshIndex];
		const char *pName = m_ShaderSymbolTable.String( lookup.m_Name );
		Msg( "psh 0x%8.8x: static combo:%9d dynamic combos:%6d refcount:%4d \"%s\"\n", pshIndex,
			lookup.m_nStaticIndex, lookup.m_ShaderStaticCombos.m_nCount,
			lookup.m_nRefCount, pName );
		totalPixelShaders += lookup.m_ShaderStaticCombos.m_nCount;
		totalPixelShaderSets++;
	}

	Msg( "Total unique vertex shaders: %d\n", totalVertexShaders );
	Msg( "Total vertex shader sets: %d\n", totalVertexShaderSets );
	Msg( "Total unique pixel shaders: %d\n", totalPixelShaders );
	Msg( "Total pixel shader sets: %d\n", totalPixelShaderSets );
}

CON_COMMAND( mat_spewvertexandpixelshaders, "Print all vertex and pixel shaders currently loaded to the console" )
{
	( ( CShaderManager * )ShaderManager() )->SpewVertexAndPixelShaders();
}

#ifdef DYNAMIC_SHADER_COMPILE
void CShaderManager::FlushShaders( void )
{
	for( VertexShader_t shader = m_VertexShaderDict.Head(); 
	     shader != m_VertexShaderDict.InvalidIndex(); 
		 shader = m_VertexShaderDict.Next( shader ) )
	{
		ShaderStaticCombos_t &combos = m_VertexShaderDict[shader].m_ShaderStaticCombos;
		for( int i = 0; i < combos.m_nCount; i++ )
		{
			if( combos.m_pHardwareShaders[i] != INVALID_HARDWARE_SHADER )
			{
#ifdef _DEBUG
				ULONG nRetVal=
#endif
					( ( IDirect3DVertexShader9 * )combos.m_pHardwareShaders[i] )->Release();
				Assert( nRetVal == 0 );
			}
			combos.m_pHardwareShaders[i] = INVALID_HARDWARE_SHADER;
		}
	}

	for( PixelShader_t shader = m_PixelShaderDict.Head(); 
	     shader != m_PixelShaderDict.InvalidIndex(); 
		 shader = m_PixelShaderDict.Next( shader ) )
	{
		ShaderStaticCombos_t &combos = m_PixelShaderDict[shader].m_ShaderStaticCombos;
		for( int i = 0; i < combos.m_nCount; i++ )
		{
			if( combos.m_pHardwareShaders[i] != INVALID_HARDWARE_SHADER )
			{
#ifdef _DEBUG
				ULONG nRetVal =
#endif
					( ( IDirect3DPixelShader * )combos.m_pHardwareShaders[i] )->Release();
				Assert( nRetVal == 0 );
			}
			combos.m_pHardwareShaders[i] = INVALID_HARDWARE_SHADER;
		}
	}

	// invalidate the file cache
	m_ShaderFileCache.Purge();
}
#endif

#ifdef DYNAMIC_SHADER_COMPILE
static void MatFlushShaders( void )
{
	( ( CShaderManager * )ShaderManager() )->FlushShaders();
}
#endif

#ifdef DYNAMIC_SHADER_COMPILE
CON_COMMAND( mat_flushshaders, "flush all hardware shaders when using DYNAMIC_SHADER_COMPILE" )
{
	MatFlushShaders();
}
#endif

CON_COMMAND( mat_shadercount, "display count of all shaders and reset that count" )
{
	Warning( "Num Pixel Shaders = %d Vertex Shaders=%d\n", s_NumPixelShadersCreated, s_NumVertexShadersCreated );
	s_NumVertexShadersCreated = 0;
	s_NumPixelShadersCreated = 0;
}

#if defined( DX_TO_GL_ABSTRACTION )
void	CShaderManager::DoStartupShaderPreloading()
{
	if (mat_autoload_glshaders.GetInt())
	{
		double flStartTime = Plat_FloatTime();

		s_NumVertexShadersCreated = s_NumPixelShadersCreated = 0;

		// try base file
#ifdef OSX		
		if ( !LoadShaderCache("glbaseshaders_osx.cfg") )		// factory cache
#else
		if ( !LoadShaderCache("glbaseshaders.cfg") )		// factory cache
#endif
		{
			Warning( "Could not find base GL shader cache file\n" );
		}

		if ( !LoadShaderCache("glshaders.cfg") )			// user mutable cache
		{
			Warning( "Could not find user GL shader cache file\n" );
		}

		double flEndTime = Plat_FloatTime();
		Msg( "Precache: Took %d ms, Vertex %d, Pixel %d\n", ( int )( ( flEndTime - flStartTime ) * 1000.0 ), s_NumVertexShadersCreated, s_NumPixelShadersCreated );
	}
}
#endif

