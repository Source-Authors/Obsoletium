//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "milesbase.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static int s_MilesRefCount = 0;
static AILMEMALLOCCB s_OldMemAlloc = nullptr;
static AILMEMFREECB s_OldMemFree = nullptr;

static void* AILCALL MssAlloc( UINTa size )
{
	return MemAlloc_AllocAligned( size, 16 );
}

static void AILCALL MssFree( void* mem )
{
	MemAlloc_FreeAligned( mem );
}

void IncrementRefMiles()
{
	if(s_MilesRefCount == 0)
	{
#ifdef WIN32
		AIL_set_redist_directory( "." );
#elif defined( OSX )
		#ifdef PLATFORM_64BITS
		AIL_set_redist_directory( "osx32" );
#else
		AIL_set_redist_directory( "osx64" );
#endif
#elif defined( LINUX )
#ifdef PLATFORM_64BITS
		AIL_set_redist_directory( "bin/linux64" );
#else
		AIL_set_redist_directory( "bin/linux32" );
#endif
#else
		AssertMsg( false, "Using default MSS_REDIST_DIR_NAME - this will most likely fail." );
		AIL_set_redist_directory( MSS_REDIST_DIR_NAME );
#endif

		if ( AIL_startup() == 0 )
		{
			Warning( "Miles Sound System startup failed. Audio may not be available. %s\n", AIL_last_error());
		}
		else
		{
			Msg( "Start up Miles Sound System %s (%u.%u.%u)\n",
				MSS_VERSION, MSS_MAJOR_VERSION, MSS_MINOR_VERSION, MSS_SUB_VERSION );
		}

		// dimhotepus: Use our allocators (ASAN happier).
		s_OldMemAlloc = AIL_mem_use_malloc( MssAlloc );
		s_OldMemFree = AIL_mem_use_free( MssFree );
	}
	
	++s_MilesRefCount;
}

void DecrementRefMiles()
{
	Assert(s_MilesRefCount > 0);
	--s_MilesRefCount;
	if(s_MilesRefCount == 0)
	{
        // dimhotepus: Reset our allocators.
		AIL_mem_use_free( s_OldMemFree );
		AIL_mem_use_malloc( s_OldMemAlloc );

		CProvider::FreeAllProviders();
		AIL_shutdown();

		Msg( "Shut down Miles Sound System %s (%u.%u.%u)\n",
			MSS_VERSION, MSS_MAJOR_VERSION, MSS_MINOR_VERSION, MSS_SUB_VERSION );
	}
}


// ------------------------------------------------------------------------ //
// CProvider functions.
// ------------------------------------------------------------------------ //

CProvider *CProvider::s_pHead = NULL;


CProvider::CProvider( HPROVIDER hProvider )
{
	m_hProvider = hProvider;

	// Add to the global list of CProviders.
	m_pNext = s_pHead;
	s_pHead = this;
}


CProvider::~CProvider()
{
	RIB_free_provider_library( m_hProvider );

	// Remove from the global list.
	CProvider **ppPrev = &s_pHead;
	for ( CProvider *pCur=s_pHead; pCur; pCur=pCur->m_pNext )
	{
		if ( pCur == this )
		{
			*ppPrev = m_pNext;
			return;
		}

		ppPrev = &pCur->m_pNext;
	}
}


CProvider* CProvider::FindProvider( HPROVIDER hProvider )
{
	for ( CProvider *pCur=s_pHead; pCur; pCur=pCur->m_pNext )
	{
		if ( pCur->GetProviderHandle() == hProvider )
		{
			return pCur;
		}
	}
	return NULL;
}


void CProvider::FreeAllProviders()
{
	CProvider *pNext;
	for ( CProvider *pCur=s_pHead; pCur; pCur=pNext )
	{
		pNext = pCur->m_pNext;
		delete pCur;
	}
}


HPROVIDER CProvider::GetProviderHandle()
{
	return m_hProvider;
}


// ------------------------------------------------------------------------ //
// ASISTRUCT functions.
// ------------------------------------------------------------------------ //
ASISTRUCT::ASISTRUCT()
{
	Clear();
	IncrementRefMiles();
}

ASISTRUCT::~ASISTRUCT()
{
	Shutdown();
	DecrementRefMiles();
}


void ASISTRUCT::Clear()
{
	m_pProvider = NULL;
	ASI_stream_open = NULL;
	ASI_stream_process = NULL;
	ASI_stream_close = NULL;
	ASI_stream_property = NULL;
	ASI_stream_seek = NULL;
	OUTPUT_BITS = NULL;
	OUTPUT_CHANNELS = NULL;
	OUTPUT_RATE = NULL;
	INPUT_BITS = NULL;
	INPUT_CHANNELS = NULL;
	INPUT_RATE = NULL;
	INPUT_BLOCK_SIZE = NULL;
	POSITION = NULL;
	m_stream = NULL;
}


bool ASISTRUCT::Init( void *pCallbackObject, const char *pInputFileType, const char *pOutputFileType, AILASIFETCHCB cb )
{
	static char lastInputFileType[128] = {'\0'}, lastOutputFileType[128] = {'\0'};

	// Get the provider.
	HPROVIDER hProvider = RIB_find_files_provider( "ASI codec", 
		"Output file types", pOutputFileType, "Input file types", pInputFileType );
	if ( !hProvider )
	{
		// dimhotepus: Do not spam console when no MSS codec.
		if ( V_strcmp( lastInputFileType, pInputFileType ) || strcmp( lastOutputFileType, pOutputFileType ) )
		{
			DevWarning( "Can't find provider 'ASI codec' for input %s, output %s\n",
				pInputFileType, pOutputFileType );

			V_strcpy_safe( lastInputFileType, pInputFileType );
			V_strcpy_safe( lastOutputFileType, pOutputFileType );
		}

		return false;
	}

	m_pProvider = CProvider::FindProvider( hProvider );
	if ( !m_pProvider )
	{
		m_pProvider = new CProvider( hProvider );
	}

	if ( !m_pProvider )
	{
		DevWarning( "Can't create provider 'ASI codec'\n" );
		return false;
	}

	RIB_INTERFACE_ENTRY ASISTR[] =
	{
		FN( ASI_stream_property ),
		FN( ASI_stream_open ),
		FN( ASI_stream_close ),
		FN( ASI_stream_process ),

		PR( "Output sample rate",       OUTPUT_RATE ),
		PR( "Output sample width",      OUTPUT_BITS ),
		PR( "Output channels",          OUTPUT_CHANNELS ),

		PR( "Input sample rate",        INPUT_RATE ),
		PR( "Input sample width",       INPUT_BITS ),
		PR( "Input channels",           INPUT_CHANNELS ),

		PR( "Minimum input block size", INPUT_BLOCK_SIZE ),
		PR( "Position",                 POSITION ),
	};

	RIBRESULT result = RIB_request( m_pProvider->GetProviderHandle(), "ASI stream", ASISTR );
	if ( result != RIB_NOERR )
	{
		DevWarning( "Can't find interface 'ASI stream' for provider 'ASI codec'\n" );
		return false;
	}
	
	// This function doesn't exist for the voice DLLs, but it's not fatal in that case.
	RIB_INTERFACE_ENTRY seekFn[]{ FN( ASI_stream_seek ), };
	result = RIB_request( m_pProvider->GetProviderHandle(), "ASI stream", seekFn );
	if ( result != RIB_NOERR )
		ASI_stream_seek = NULL;

	m_stream = ASI_stream_open( (UINTa)pCallbackObject, cb, 0 );
	if( !m_stream )
	{
		DevWarning("Can't open ASI stream for conversion input %s -> output %s\n",
			pInputFileType, pOutputFileType );
		return false;
	}

	return true;
}

void ASISTRUCT::Shutdown()
{
	if ( m_pProvider )
	{
		if (m_stream && ASI_stream_close)
		{
			ASI_stream_close(m_stream);
			m_stream = NULL;
		}

		//m_pProvider->Release();
		m_pProvider = NULL;
	}

	Clear();
}


int ASISTRUCT::Process( void *pBuffer, unsigned int bufferSize )
{
	return ASI_stream_process( m_stream, pBuffer, bufferSize );
}


bool ASISTRUCT::IsActive() const
{
	return m_stream != NULL ? true : false;
}


unsigned int ASISTRUCT::GetProperty( HPROPERTY hProperty )
{
	uint32 nValue = 0;
	return ASI_stream_property( m_stream, hProperty, &nValue, NULL, NULL )
		? nValue : 0;
}

void ASISTRUCT::Seek( int position )
{
	if ( !ASI_stream_seek )
		Error( "ASI_stream_seek called, but it doesn't exist." );

	ASI_stream_seek( m_stream, (S32)position );
}
