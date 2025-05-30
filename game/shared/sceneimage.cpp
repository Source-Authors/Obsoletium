//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "sceneimage.h"
#include "choreoscene.h"
#include "iscenetokenprocessor.h"
#include "scenefilecache/SceneImageFile.h"

#include "lzma/lzma.h"

#include "tier1/utlbuffer.h"
#include "tier1/UtlStringMap.h"
#include "tier1/utlvector.h"
#include "tier1/UtlSortVector.h"

#include "scriplib.h"
#include "cmdlib.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CSceneImage : public ISceneImage
{
public:
	bool CreateSceneImageFile( CUtlBuffer &targetBuffer, char const *pchModPath, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *Status ) override;
};

static CSceneImage g_SceneImage;
ISceneImage *g_pSceneImage = &g_SceneImage;

struct SceneFile_t
{
	SceneFile_t()
	{
		msecs = 0;
	}

	CUtlString	fileName;
	CUtlBuffer	compiledBuffer;

	unsigned int		msecs;
	CUtlVector< short >	soundList;
};
CUtlVector< SceneFile_t > g_SceneFiles;

//-----------------------------------------------------------------------------
// Helper for parsing scene data file
//-----------------------------------------------------------------------------
class CSceneTokenProcessor final : public ISceneTokenProcessor
{
public:
	const char *CurrentToken( void ) override
	{
		return token;
	}

	bool GetToken( bool crossline ) override
	{
		return ::GetToken( crossline ) ? true : false;
	}

	bool TokenAvailable( void ) override
	{
		return ::TokenAvailable() ? true : false;
	}

	void Error( PRINTF_FORMAT_STRING const char *fmt, ... ) override
	{
		char string[2048];
		va_list argptr;
		va_start( argptr, fmt );
		V_vsprintf_safe( string, fmt, argptr );
		va_end( argptr );

		Warning( "%s", string );
		Assert( 0 );
	}
};
static CSceneTokenProcessor g_SceneTokenProcessor;
ISceneTokenProcessor *tokenprocessor = &g_SceneTokenProcessor;

// a simple case insensitive string pool
// the final pool contains all the unique strings seperated by a null
class CChoreoStringPool final : public IChoreoStringPool
{
public:
	CChoreoStringPool() : m_StringMap( true )
	{
		m_nOffset = 0;
	}

	// Returns a valid id into the string table
	short FindOrAddString( const char *pString ) override
	{
		UtlSymId_t stringId = m_StringMap.Find( pString );
		if ( stringId != m_StringMap.InvalidIndex() )
		{
			// found in pool
			return stringId;
		}

		intp &nOffset = m_StringMap[pString];
		nOffset = m_nOffset;
		// advance by string and null
		m_nOffset += strlen( pString ) + 1;

		stringId = m_StringMap.Find( pString );
		Assert( stringId >= 0 && stringId <= SHRT_MAX );

		return stringId;
	}

	bool GetString( short stringId, char *buff, intp buffSize ) override
	{
		if ( stringId < 0 || stringId >= m_StringMap.GetNumStrings() )
		{
			V_strncpy( buff, "", buffSize );
			return false;
		}
		V_strncpy( buff, m_StringMap.String( stringId ), buffSize );
		return true;
	}

	unsigned short GetNumStrings()
	{
		return m_StringMap.GetNumStrings();
	}

	size_t GetPoolSize() const
	{
		return m_nOffset;
	}

	// build the final pool
	void GetTableAndPool( CUtlVector< size_t > &offsets, CUtlBuffer &buffer )
	{
		offsets.Purge();
		buffer.Purge();

		offsets.EnsureCapacity( m_StringMap.GetNumStrings() );
		buffer.EnsureCapacity( m_nOffset );

		size_t currentOffset = 0;
		for ( unsigned short i = 0; i < m_StringMap.GetNumStrings(); i++ )
		{
			offsets.AddToTail( currentOffset );

			const char *pString = m_StringMap.String( i );
			buffer.Put( pString, strlen( pString ) + 1 ); 

			currentOffset += strlen( pString ) + 1;
		}
		Assert( currentOffset == m_nOffset );

		// align string pool to end on dword boundary
		while ( buffer.TellMaxPut() & 0x03 )
		{
			buffer.PutChar( '\0' );
			m_nOffset++;
		}
	}

	void DumpPool()
	{
		for ( unsigned short i = 0; i < m_StringMap.GetNumStrings(); i++ )
		{
			const char *pString = m_StringMap.String( i );
			Msg( "%s\n", pString );
		}
	}

	void Reset()
	{
		m_StringMap.Purge();
		m_nOffset = 0;
	}

private:
	CUtlStringMap< intp >	m_StringMap;
	size_t			m_nOffset;
};
CChoreoStringPool g_ChoreoStringPool;

//-----------------------------------------------------------------------------
// Helper for crawling events to determine sounds
//-----------------------------------------------------------------------------
void FindSoundsInEvent( CChoreoEvent *pEvent, CUtlVector< short >& soundList )
{
	if ( !pEvent || pEvent->GetType() != CChoreoEvent::SPEAK )
		return;

	unsigned short stringId = g_ChoreoStringPool.FindOrAddString( pEvent->GetParameters() );
	if ( soundList.Find( stringId ) == soundList.InvalidIndex() )
	{
		soundList.AddToTail( stringId );
	}

	if ( pEvent->GetCloseCaptionType() == CChoreoEvent::CC_MASTER )
	{
		char tok[ CChoreoEvent::MAX_CCTOKEN_STRING ];
		if ( pEvent->GetPlaybackCloseCaptionToken( tok, sizeof( tok ) ) )
		{
			stringId = g_ChoreoStringPool.FindOrAddString( tok );
			if ( soundList.Find( stringId ) == soundList.InvalidIndex() )
			{
				soundList.AddToTail( stringId );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Create binary compiled version of VCD. Stores to a dictionary for later
// post processing
//-----------------------------------------------------------------------------
bool CreateTargetFile_VCD( const char *pSourceName, [[maybe_unused]] const char *pTargetName, [[maybe_unused]] bool bWriteToZip, bool bLittleEndian )
{
	CUtlBuffer sourceBuf;
	if ( !scriptlib->ReadFileToBuffer( pSourceName, sourceBuf ) )
	{
		return false;
	}

	CRC32_t crcSource;
	CRC32_Init( &crcSource );
	CRC32_ProcessBuffer( &crcSource, sourceBuf.Base(), sourceBuf.TellMaxPut() );
	CRC32_Final( &crcSource );

	ParseFromMemory( sourceBuf.Base<char>(), sourceBuf.TellMaxPut() );

	CChoreoScene *pChoreoScene = ChoreoLoadScene( pSourceName, NULL, &g_SceneTokenProcessor, Msg );
	if ( !pChoreoScene )
	{
		return false;
	}

	SceneFile_t &sceneFile = g_SceneFiles[g_SceneFiles.AddToTail()];
	sceneFile.fileName.Set( pSourceName );

	// Walk all events looking for SPEAK events
	CChoreoEvent *pEvent;
	for ( intp i = 0; i < pChoreoScene->GetNumEvents(); ++i )
	{
		pEvent = pChoreoScene->GetEvent( i );
		FindSoundsInEvent( pEvent, sceneFile.soundList );
	}

	// calc duration
	sceneFile.msecs = (unsigned int)( pChoreoScene->FindStopTime() * 1000.0f + 0.5f );

	// compile to binary buffer
	sceneFile.compiledBuffer.SetBigEndian( !bLittleEndian );
	pChoreoScene->SaveToBinaryBuffer( sceneFile.compiledBuffer, crcSource, &g_ChoreoStringPool );

	unsigned int compressedSize;
	unsigned char *pCompressedBuffer = LZMA_OpportunisticCompress( sceneFile.compiledBuffer.Base<unsigned char>(),
	                                                               sceneFile.compiledBuffer.TellMaxPut(),
	                                                               &compressedSize );
	if ( pCompressedBuffer )
	{
		// replace the compiled buffer with the compressed version
		sceneFile.compiledBuffer.Purge();
		sceneFile.compiledBuffer.EnsureCapacity( compressedSize );
		sceneFile.compiledBuffer.Put( pCompressedBuffer, compressedSize );
		free( pCompressedBuffer );
	}

	delete pChoreoScene;

	return true;
}

class CSceneImageEntryLessFunc
{
public:
	bool Less( const SceneImageEntry_t &entryLHS, const SceneImageEntry_t &entryRHS, void * )
	{
		return entryLHS.crcFilename < entryRHS.crcFilename;
	}
};



//-----------------------------------------------------------------------------
// A Scene image file contains all the compiled .XCD
//-----------------------------------------------------------------------------
bool CSceneImage::CreateSceneImageFile( CUtlBuffer &targetBuffer, char const *pchModPath, bool bLittleEndian, bool bQuiet, ISceneCompileStatus *pStatus )
{
	CUtlVector<fileList_t>	vcdFileList;
	CUtlSymbolTable			vcdSymbolTable( 0, 32, true );

	Msg( "\n" );

	// get all the VCD files according to the seacrh paths
	char searchPaths[512];
	g_pFullFileSystem->GetSearchPath_safe( "GAME", false, searchPaths );
	char *pPath = strtok( searchPaths, ";" );
	while ( pPath )
	{
		intp currentCount = vcdFileList.Count();

		char szPath[MAX_PATH];
		V_ComposeFileName( pPath, "scenes/*.vcd", szPath );

		scriptlib->FindFiles( szPath, true, vcdFileList );

		Msg( "Scenes: Searching '%s' - Found %zd scenes.\n", szPath, vcdFileList.Count() - currentCount );

		pPath = strtok( NULL, ";" );
	}

	if ( !vcdFileList.Count() )
	{
		Msg( "Scenes: No Scene Files found!\n" );
		return false;
	}

	// iterate and convert all the VCD files
	bool bGameIsTF = V_stristr( pchModPath, "\\tf" ) != NULL;
	for ( intp i=0; i<vcdFileList.Count(); i++ )
	{
		const char *pFilename = vcdFileList[i].fileName.String();
		const char *pSceneName = V_stristr( pFilename, "scenes\\" );
		if ( !pSceneName )
		{
			continue;
		}

		if ( !bLittleEndian && bGameIsTF && V_stristr( pSceneName, "high\\" ) )
		{
			continue;
		}

		// process files in order they would be found in search paths
		// i.e. skipping later processed files that match an earlier conversion
		UtlSymId_t symbol = vcdSymbolTable.Find( pSceneName );
		if ( symbol == UTL_INVAL_SYMBOL )
		{
			vcdSymbolTable.AddString( pSceneName );

			pStatus->UpdateStatus( pFilename, bQuiet, i, vcdFileList.Count() );

			if ( !CreateTargetFile_VCD( pFilename, "", false, bLittleEndian ) )
			{
				Error( "CreateSceneImageFile: Failed on '%s' conversion!\n", pFilename );
			}
		}
	}

	if ( !g_SceneFiles.Count() )
	{
		// nothing to do
		return true;
	}

	Msg( "Scenes: Finalizing %zd unique scenes.\n", g_SceneFiles.Count() );


	// get the string pool
	CUtlVector< size_t > stringOffsets;
	CUtlBuffer stringPool;
	g_ChoreoStringPool.GetTableAndPool( stringOffsets, stringPool );

	if ( !bQuiet )
	{
		Msg( "Scenes: String Table: %zu bytes\n", stringOffsets.Count() * sizeof( int ) );
		Msg( "Scenes: String Pool: %zd bytes\n", stringPool.TellMaxPut() );
	}

	// first header, then lookup table, then string pool blob
	intp stringPoolStart = sizeof( SceneImageHeader_t ) + stringOffsets.Count() * sizeof( int );
	// then directory
	intp sceneEntryStart = stringPoolStart + stringPool.TellMaxPut();
	// then variable sized summaries
	intp sceneSummaryStart = sceneEntryStart + g_SceneFiles.Count() * sizeof( SceneImageEntry_t );
	// then variable sized compiled binary scene data
	intp sceneDataStart = 0;

	// construct header
	SceneImageHeader_t imageHeader = {};
	imageHeader.nId = SCENE_IMAGE_ID;
	imageHeader.nVersion = SCENE_IMAGE_VERSION;
	imageHeader.nNumScenes = g_SceneFiles.Count();
	imageHeader.nNumStrings = stringOffsets.Count();
	imageHeader.nSceneEntryOffset = sceneEntryStart;
	if ( !bLittleEndian )
	{
		imageHeader.nId = BigLong( imageHeader.nId );
		imageHeader.nVersion = BigLong( imageHeader.nVersion );
		imageHeader.nNumScenes = BigLong( imageHeader.nNumScenes );
		imageHeader.nNumStrings = BigLong( imageHeader.nNumStrings );
		imageHeader.nSceneEntryOffset = BigLong( imageHeader.nSceneEntryOffset );
	}
	targetBuffer.Put( &imageHeader, sizeof( imageHeader ) );

	// header is immediately followed by string table and pool
	for ( intp i = 0; i < stringOffsets.Count(); i++ )
	{
		size_t offset = stringPoolStart + stringOffsets[i];
		if ( !bLittleEndian )
		{
			offset = BigLong( offset );
		}
		targetBuffer.PutInt( offset );
	}
	Assert( stringPoolStart == targetBuffer.TellMaxPut() );
	targetBuffer.Put( stringPool.Base(), stringPool.TellMaxPut() );

	// construct directory
	CUtlSortVector< SceneImageEntry_t, CSceneImageEntryLessFunc > imageDirectory;
	imageDirectory.EnsureCapacity( g_SceneFiles.Count() );

	// build directory
	// directory is linear sorted by filename checksum for later binary search
	for ( intp i = 0; i < g_SceneFiles.Count(); i++ )
	{
		SceneImageEntry_t imageEntry = {};

		// name needs to be normalized for determinstic later CRC name calc
		// calc crc based on scenes\anydir\anyscene.vcd
		char szCleanName[MAX_PATH];
		V_strcpy_safe( szCleanName, g_SceneFiles[i].fileName.String() );
		V_strlower( szCleanName );
		V_FixSlashes( szCleanName );
		char *pName = V_stristr( szCleanName, "scenes\\" );
		if ( !pName )
		{
			// must have scenes\ in filename
			Error( "CreateSceneImageFile: Unexpected lack of scenes prefix on %s\n", g_SceneFiles[i].fileName.String() );
		}

		CRC32_t crcFilename = CRC32_ProcessSingleBuffer( pName, strlen( pName ) );
		imageEntry.crcFilename = crcFilename;

		// temp store an index to its file, fixup later, necessary to access post sort
		imageEntry.nDataOffset = i;
		if ( imageDirectory.Find( imageEntry ) != imageDirectory.InvalidIndex() )
		{
			// filename checksums must be unique or runtime binary search would be bogus
			Error( "CreateSceneImageFile: Unexpected filename checksum collision!\n" );
		}

		imageDirectory.Insert( imageEntry );
	}

	// determine sort order and start of data after dynamic summaries
	CUtlVector< int > writeOrder;
	writeOrder.EnsureCapacity( g_SceneFiles.Count() );
	sceneDataStart = sceneSummaryStart;
	for ( intp i = 0; i < imageDirectory.Count(); i++ )
	{
		// reclaim offset, indicates write order of scene file
		int iScene = imageDirectory[i].nDataOffset;
		writeOrder.AddToTail( iScene );

		// march past each variable sized summary to determine start of scene data
		intp numSounds = g_SceneFiles[iScene].soundList.Count();
		sceneDataStart += sizeof( SceneImageSummary_t ) + ( numSounds - 1 ) * sizeof( int );
	}

	// finalize and write directory
	Assert( sceneEntryStart == targetBuffer.TellMaxPut() );
	intp nSummaryOffset = sceneSummaryStart;
	intp nDataOffset = sceneDataStart;
	for ( intp i = 0; i < imageDirectory.Count(); i++ )
	{
		int iScene = writeOrder[i];

		auto &dir = imageDirectory[i];
		const auto &sceneFile = g_SceneFiles[iScene];

		dir.nDataOffset = nDataOffset;
		dir.nDataLength = sceneFile.compiledBuffer.TellMaxPut();
		dir.nSceneSummaryOffset = nSummaryOffset;
		if ( !bLittleEndian )
		{
			dir.crcFilename = BigLong( dir.crcFilename );
			dir.nDataOffset = BigLong( dir.nDataOffset );
			dir.nDataLength = BigLong( dir.nDataLength );
			dir.nSceneSummaryOffset = BigLong( dir.nSceneSummaryOffset );
		}
		targetBuffer.Put( &dir, sizeof( SceneImageEntry_t ) );

		const intp numSounds = sceneFile.soundList.Count();

		nSummaryOffset += sizeof( SceneImageSummary_t ) + (numSounds - 1) * sizeof( int );
		nDataOffset += sceneFile.compiledBuffer.TellMaxPut();
	}

	// finalize and write summaries
	Assert( sceneSummaryStart == targetBuffer.TellMaxPut() );
	for ( intp i = 0; i < imageDirectory.Count(); i++ )
	{
		int iScene = writeOrder[i];
		const auto &sceneFile = g_SceneFiles[iScene];

		int msecs = sceneFile.msecs;
		intp soundCount = sceneFile.soundList.Count();
		if ( !bLittleEndian )
		{
			msecs = BigLong( msecs );
			soundCount = BigLong( soundCount );
		}
		targetBuffer.PutInt( msecs );
		targetBuffer.PutInt( soundCount );
		for ( intp j = 0; j < sceneFile.soundList.Count(); j++ )
		{
			int soundId = sceneFile.soundList[j];
			if ( !bLittleEndian )
			{
				soundId = BigLong( soundId );
			}
			targetBuffer.PutInt( soundId );
		}
	}

	// finalize and write data
	Assert( sceneDataStart == targetBuffer.TellMaxPut() );
	for ( intp i = 0; i < imageDirectory.Count(); i++ )
	{	
		int iScene = writeOrder[i];
		const auto &sceneFile = g_SceneFiles[iScene];

		targetBuffer.Put( sceneFile.compiledBuffer.Base(), sceneFile.compiledBuffer.TellMaxPut() );
	}

	if ( !bQuiet )
	{
		Msg( "Scenes: Final size: %.2f MiB\n", targetBuffer.TellMaxPut() / (1024.0f * 1024.0f) );
	}

	// cleanup
	g_SceneFiles.Purge();

	return true;
}
