//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef SND_WAVE_SOURCE_H
#define SND_WAVE_SOURCE_H
#pragma once

#include "snd_audio_source.h"
class IterateRIFF;
#include "sentence.h"
#include "snd_sfx.h"

//=============================================================================
// Functions to create audio sources from wave files or from wave data.
//=============================================================================
extern CAudioSource* Audio_CreateMemoryWave( CSfxTable *pSfx );
extern CAudioSource* Audio_CreateStreamedWave( CSfxTable *pSfx );

class CAudioSourceWave : public CAudioSource
{
public:
	CAudioSourceWave( CSfxTable *pSfx );
	CAudioSourceWave( CSfxTable *pSfx, CAudioSourceCachedInfo *info );
	virtual ~CAudioSourceWave();

	int				GetType( void ) override;
	void			GetCacheData( CAudioSourceCachedInfo *info ) override;

	void			Setup( const char *pFormat, int formatSize, IterateRIFF &walk );
	int				SampleRate( void ) override;
	int				SampleSize( void ) override;
	int				SampleCount( void ) override;

	int				Format( void ) override;
	int				DataSize( void ) override;

	void					*GetHeader( void );
	bool			IsVoiceSource() override;

	virtual	void			ParseChunk( IterateRIFF &walk, int chunkName );
	virtual void			ParseSentence( IterateRIFF &walk );

	void					ConvertSamples( char *pData, int sampleCount );
	bool					IsLooped( void ) override;
	bool					IsStereoWav( void ) override;
	bool					IsStreaming( void ) override;
	int						GetCacheStatus( void ) override;
	int						ConvertLoopedPosition( int samplePosition );
	void					CacheLoad( void ) override;
	void					CacheUnload( void ) override;
	int				ZeroCrossingBefore( int sample ) override;
	int				ZeroCrossingAfter( int sample ) override;
	void			ReferenceAdd( CAudioMixer *pMixer ) override;
	void			ReferenceRemove( CAudioMixer *pMixer ) override;
	bool			CanDelete( void ) override;
	CSentence		*GetSentence( void ) override;
	const char				*GetName();

	bool			IsAsyncLoad() override;

	void			CheckAudioSourceCache() override;

	char const		*GetFileName() override;

	// 360 uses alternate play once semantics
	void			SetPlayOnce( bool bIsPlayOnce ) override { m_bIsPlayOnce = IsPC() ? bIsPlayOnce : false; }
	bool			IsPlayOnce() override { return IsPC() ? m_bIsPlayOnce : false; }

	void			SetSentenceWord( bool bIsWord ) override { m_bIsSentenceWord = bIsWord; }
	bool			IsSentenceWord() override { return m_bIsSentenceWord; }

	int				GetLoopingInfo( int *pLoopBlock, int *pNumLeadingSamples, int *pNumTrailingSamples );

	int				SampleToStreamPosition( int ) override { return 0; }
	int				StreamToSamplePosition( int ) override { return 0; }

protected:
	void					ParseCueChunk( IterateRIFF &walk );
	void					ParseSamplerChunk( IterateRIFF &walk );

	void					Init( const char *pHeaderBuffer, int headerSize );
	bool					GetStartupData( void *dest, int destsize, int& bytesCopied );
	bool					GetXboxAudioStartupData();

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Output : byte
	//-----------------------------------------------------------------------------
	inline byte *GetCachedDataPointer()
	{
		VPROF("CAudioSourceWave::GetCachedDataPointer");

		CAudioSourceCachedInfo *info = m_AudioCacheHandle.Get( CAudioSource::AUDIO_SOURCE_WAV, m_pSfx->IsPrecachedSound(), m_pSfx, &m_nCachedDataSize );
		if ( !info )
		{
			AssertMsg( false, "CAudioSourceWave::GetCachedDataPointer info == NULL" );
			return NULL;
		}

		return (byte *)info->CachedData();
	}

	int				m_bits;
	int				m_rate;
	int				m_channels;
	int				m_format;
	int				m_sampleSize;
	int				m_loopStart;
	int				m_sampleCount;			// can be "samples" or "bytes", depends on format

	CSfxTable		*m_pSfx;
	CSentence		*m_pTempSentence;

	int				m_dataStart;			// offset of sample data
	int				m_dataSize;				// size of sample data

	char			*m_pHeader;
	int				m_nHeaderSize;

	CAudioSourceCachedInfoHandle_t m_AudioCacheHandle;

	int				m_nCachedDataSize;

	// number of actual samples (regardless of format)
	// compressed formats alter definition of m_sampleCount 
	// used to spare expensive calcs by decoders
	int				m_numDecodedSamples;

	// additional data needed by xma decoder to for looping
	unsigned short	m_loopBlock;			// the block the loop occurs in
	unsigned short	m_numLeadingSamples;	// number of leader samples in the loop block to discard
	unsigned short	m_numTrailingSamples;	// number of trailing samples in the final block to discard
	unsigned short	unused;

	unsigned int	m_bNoSentence : 1;
	unsigned int	m_bIsPlayOnce : 1;
	unsigned int	m_bIsSentenceWord : 1;

private:
	CAudioSourceWave( const CAudioSourceWave & ) = delete; // not implemented, not allowed

	int				m_refCount;
	
#ifdef _DEBUG
	// Only set in debug mode so you can see the name.
	const char		*m_pDebugName;
#endif
};


#endif // SND_WAVE_SOURCE_H
