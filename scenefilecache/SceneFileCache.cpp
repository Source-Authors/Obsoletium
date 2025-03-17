//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Cache for VCDs. PC async loads and uses the datacache to manage.
// 360 uses a baked resident image of aggregated compiled VCDs.
//
//=============================================================================

#include "scenefilecache/ISceneFileCache.h"
#include "filesystem.h"
#include "tier1/utldict.h"
#include "tier1/utlbuffer.h"
#include "tier1/lzmaDecoder.h"
#include "scenefilecache/SceneImageFile.h"
#include "choreoscene.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IFileSystem	*filesystem = nullptr;

class CSceneFileCache : public CBaseAppSystem< ISceneFileCache >
{
public:
	// IAppSystem

	bool			Connect( CreateInterfaceFn factory ) override;
	void			Disconnect() override;
	InitReturnVal_t Init() override;
	void			Shutdown() override;

	// ISceneFileCache

	size_t			GetSceneBufferSize( char const *pFilename ) override;
	bool			GetSceneData( char const *pFilename, byte *buf, size_t bufsize ) override;

	// alternate resident image implementation
	bool			GetSceneCachedData( char const *pFilename, SceneCachedData_t *pData ) override;
	short			GetSceneCachedSound( int iScene, int iSound ) override;
	const char		*GetSceneString( short stringId ) override;

	// Physically reloads image from disk
	void			Reload() override;

private:
	// alternate implementation - uses a resident baked image of the file cache, contains all the compiled VCDs
	// single i/o read at startup to mount the image
	int						FindSceneInImage( const char *pSceneName );
	bool					GetSceneDataFromImage( const char *pSceneName, int iIndex, byte *pData, size_t *pLength );

private:
	CUtlBuffer						m_SceneImageFile;
};

bool CSceneFileCache::Connect( CreateInterfaceFn factory )
{
	if ( (filesystem = (IFileSystem *)factory( FILESYSTEM_INTERFACE_VERSION, nullptr)) == nullptr )
	{
		return false;
	}
	
	return true;
}

void CSceneFileCache::Disconnect()
{
}

InitReturnVal_t CSceneFileCache::Init()
{
	constexpr char pSceneImageName[]{"scenes/scenes.image"};

	if ( m_SceneImageFile.TellMaxPut() == 0 )
	{
		MEM_ALLOC_CREDIT();

		if ( filesystem->ReadFile( pSceneImageName, "GAME", m_SceneImageFile ) )
		{
			auto *pHeader = m_SceneImageFile.Base<SceneImageHeader_t>();
			if ( pHeader->nId != SCENE_IMAGE_ID || 
				 pHeader->nVersion != SCENE_IMAGE_VERSION )
			{
				Error( "CSceneFileCache: Bad scene image file %s. Expected id %d and version 0x%x, got %d and 0x%x.\n",
					pSceneImageName, SCENE_IMAGE_ID, SCENE_IMAGE_VERSION, pHeader->nId, pHeader->nVersion );
			}
		}
		else
		{
			m_SceneImageFile.Purge();
		}
	}

	return INIT_OK;
}

void CSceneFileCache::Shutdown()
{
	m_SceneImageFile.Purge();
}

// Physically reloads image from disk
void CSceneFileCache::Reload()
{
	Shutdown();
	Init();
}

size_t CSceneFileCache::GetSceneBufferSize( char const *pFilename )
{
	char fn[MAX_PATH];
	V_strcpy_safe( fn, pFilename );
	Q_FixSlashes( fn );
	Q_strlower( fn );
	
	size_t returnSize = 0;
	GetSceneDataFromImage( pFilename, FindSceneInImage( fn ), nullptr, &returnSize );
	return returnSize;
}

bool CSceneFileCache::GetSceneData( char const *pFilename, byte *buf, size_t bufsize )
{
	Assert( pFilename );
	Assert( buf );
	Assert( bufsize > 0 );

	char fn[MAX_PATH];
	V_strcpy_safe( fn, pFilename );
	Q_FixSlashes( fn );
	Q_strlower( fn );

	size_t nLength = bufsize;
	return GetSceneDataFromImage( pFilename, FindSceneInImage( fn ), buf, &nLength );
}

bool CSceneFileCache::GetSceneCachedData( char const *pFilename, SceneCachedData_t *pData )
{
	int iScene = FindSceneInImage( pFilename );
	auto *pHeader = m_SceneImageFile.Base<SceneImageHeader_t>();
	if ( !pHeader || iScene < 0 || iScene >= pHeader->nNumScenes )
	{
		// not available
		pData->sceneId = -1;
		pData->msecs = 0;
		pData->numSounds = 0;
		return false;
	}

	// get scene summary
	auto *pEntries = (SceneImageEntry_t *)( (byte *)pHeader + pHeader->nSceneEntryOffset );
	auto *pSummary = (SceneImageSummary_t *)( (byte *)pHeader + pEntries[iScene].nSceneSummaryOffset );
	
	pData->sceneId = iScene;
	pData->msecs = pSummary->msecs;
	pData->numSounds = pSummary->numSounds;

	return true;
}

short CSceneFileCache::GetSceneCachedSound( int iScene, int iSound )
{
	auto *pHeader = m_SceneImageFile.Base<SceneImageHeader_t>();
	if ( !pHeader || iScene < 0 || iScene >= pHeader->nNumScenes )
	{
		// huh?, image file not present or bad index
		return -1;
	}

	auto *pEntries = (SceneImageEntry_t *)( (byte *)pHeader + pHeader->nSceneEntryOffset );
	auto *pSummary = (SceneImageSummary_t *)( (byte *)pHeader + pEntries[iScene].nSceneSummaryOffset );
	if ( iSound < 0 || iSound >= pSummary->numSounds )
	{
		// bad index
		Assert( 0 );
		return -1;
	}

	// dimhotepus: Check string in range.
	int string = pSummary->soundStrings[iSound];
		Assert(string >= std::numeric_limits<short>::min() &&
			   string <= std::numeric_limits<short>::max());
	return static_cast<short>(string);
}

const char *CSceneFileCache::GetSceneString( short stringId )
{
	auto *pHeader = m_SceneImageFile.Base<SceneImageHeader_t>();
	if ( !pHeader || stringId < 0 || stringId >= pHeader->nNumStrings )
	{
		// huh?, image file not present, or index bad
		return nullptr;
	}

	return pHeader->String( stringId );
}

//-----------------------------------------------------------------------------
//  Returns -1 if not found, otherwise [0..n] index.
//-----------------------------------------------------------------------------
int CSceneFileCache::FindSceneInImage( const char *pSceneName )
{
	auto *pHeader = m_SceneImageFile.Base<SceneImageHeader_t>();
	if ( !pHeader )
	{
		return -1;
	}
	auto *pEntries = (SceneImageEntry_t *)( (byte *)pHeader + pHeader->nSceneEntryOffset );

	char szCleanName[MAX_PATH];

	V_strncpy( szCleanName, pSceneName, sizeof( szCleanName ) );
	V_strlower( szCleanName );
#ifdef POSIX
	V_FixSlashes( szCleanName, '\\' );
#else
	V_FixSlashes( szCleanName );
#endif
	V_SetExtension( szCleanName, ".vcd", sizeof( szCleanName ) );

	CRC32_t crcFilename = CRC32_ProcessSingleBuffer( szCleanName, V_strlen( szCleanName ) );

	// use binary search, entries are sorted by ascending crc
	int nLowerIdx = 1;
	int nUpperIdx = pHeader->nNumScenes;
	for ( ;; )
	{
		if ( nUpperIdx < nLowerIdx )
		{
			return -1;
		}

		{
			int nMiddleIndex = ( nLowerIdx + nUpperIdx )/2;
			CRC32_t nProbe = pEntries[nMiddleIndex-1].crcFilename;
			if ( crcFilename < nProbe )
			{
				nUpperIdx = nMiddleIndex - 1;
			}
			else
			{
				if ( crcFilename > nProbe )
				{
					nLowerIdx = nMiddleIndex + 1;
				}
				else
				{
					return nMiddleIndex - 1;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//  Returns true if success, false otherwise. Caller must free ouput scene data
//-----------------------------------------------------------------------------
bool CSceneFileCache::GetSceneDataFromImage( const char *pFileName, int iScene, byte *pSceneData, size_t *pSceneLength )
{
	auto *pHeader = m_SceneImageFile.Base<SceneImageHeader_t>();
	if ( !pHeader || iScene < 0 || iScene >= pHeader->nNumScenes )
	{
		if ( pSceneData )
		{
			*pSceneData = 0;
		}
		if ( pSceneLength )
		{
			*pSceneLength = 0;
		}
		return false;
	}

	auto *pEntries = (SceneImageEntry_t *)( (byte *)pHeader + pHeader->nSceneEntryOffset );
	auto *pData = (unsigned char *)pHeader + pEntries[iScene].nDataOffset;
	bool bIsCompressed = CLZMA::IsCompressed( pData );
	if ( bIsCompressed )
	{
		size_t originalSize = CLZMA::GetActualSize( pData );
		if ( pSceneData )
		{
			size_t nMaxLen = *pSceneLength;
			if ( originalSize <= nMaxLen )
			{
				// dimhotepus: Add out size to prevent overflows.
				CLZMA::Uncompress( pData, pSceneData, originalSize );
			}
			else
			{
				auto *pOutputData = (unsigned char *)malloc( originalSize );
				CLZMA::Uncompress( pData, pOutputData );
				V_memcpy( pSceneData, pOutputData, nMaxLen );
				free( pOutputData );
			}
		}
		if ( pSceneLength )
		{
			*pSceneLength = originalSize;
		}
	}
	else
	{
		if ( pSceneData )
		{
			size_t nCountToCopy = min(*pSceneLength, (size_t)pEntries[iScene].nDataLength );
			V_memcpy( pSceneData, pData, nCountToCopy );
		}
		if ( pSceneLength )
		{
			*pSceneLength = (size_t)pEntries[iScene].nDataLength;
		}
	}
	return true;
}

static CSceneFileCache g_SceneFileCache;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CSceneFileCache, ISceneFileCache, SCENE_FILE_CACHE_INTERFACE_VERSION, g_SceneFileCache );
