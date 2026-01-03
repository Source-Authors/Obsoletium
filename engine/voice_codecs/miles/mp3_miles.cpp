//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "tier1/interface.h"
#include "milesbase.h"
#include "vaudio/ivaudio.h"
#include "tier0/dbg.h"

#ifndef SE_USE_MILES_MP3
// Strip MP1 / MP2 to reduce attack surface.
#define MINIMP3_ONLY_MP3
// Saves some code bytes, and enforces non-standard but
// logical behaviour of mono-stereo transition (rare case).
#define MINIMP3_NONSTANDARD_BUT_LOGICAL
// No stdio as in engine itself.
#define MINIMP3_NO_STDIO
// Ensure single implementation.
#define MINIMP3_IMPLEMENTATION
#include "minimp3/minimp3.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// dimhotepus: Miles MP3 decoder changed AudioStreamEventCB callback signature around 9.0+ version.
// We have no modern MSS headers, so use open-source minimp3 decoder.

#ifdef SE_USE_MILES_MP3

static S32 AILCALLBACK AudioStreamEventCB( UINTa user, void *dest, S32 bytes_requested, S32 offset )
{
	IAudioStreamEvent *pThis = static_cast<IAudioStreamEvent*>( (void *)user);
	
#if defined( OSX ) && !defined( PLATFORM_64BITS )
	// save of the args to local stack vars before we screw with ESP
	volatile void *newDest = dest;
	volatile S32 newBytes = bytes_requested;
	volatile S32 newOffset = offset;
	volatile void *oldESP;
	
	// now move ESP to be aligned to 16-bytes
	asm volatile(
	 "movl    %%esp,%0\n"
	"subl    $16,%%esp\n"
	"andl    $-0x10,%%esp\n"
	:  "=m" (oldESP) : : "%esp");
	int val = pThis->StreamRequestData( (void *)newDest, newBytes, newOffset );

	// undo the alignment
	asm( "movl    %0,%%esp\n" ::"m" (oldESP) );

	return val;
#else
	return pThis->StreamRequestData( dest, bytes_requested, offset );
#endif	
}

class CMilesMP3 : public IAudioStream
{
public:
	CMilesMP3();
	bool Init( IAudioStreamEvent *pHandler );
	virtual ~CMilesMP3();
	
	// IAudioStream functions
	int	Decode( void *pBuffer, unsigned int bufferSize ) override;
	int GetOutputBits() override;
	int GetOutputRate() override;
	int GetOutputChannels() override;
	unsigned int GetPosition() override;
	void SetPosition( unsigned int position ) override;
private:
	ASISTRUCT		m_decoder;
};

CMilesMP3::CMilesMP3()
{
}

bool CMilesMP3::Init( IAudioStreamEvent *pHandler )
{
	return m_decoder.Init( pHandler, ".MP3", ".RAW", &AudioStreamEventCB );
}


CMilesMP3::~CMilesMP3()
{
	m_decoder.Shutdown();
}
	

// IAudioStream functions
int	CMilesMP3::Decode( void *pBuffer, unsigned int bufferSize )
{
	return m_decoder.Process( pBuffer, bufferSize );
}


int CMilesMP3::GetOutputBits()
{
	return m_decoder.GetProperty( m_decoder.OUTPUT_BITS );
}


int CMilesMP3::GetOutputRate()
{
	return m_decoder.GetProperty( m_decoder.OUTPUT_RATE );
}


int CMilesMP3::GetOutputChannels()
{
	return m_decoder.GetProperty( m_decoder.OUTPUT_CHANNELS );
}


unsigned int CMilesMP3::GetPosition()
{
	return m_decoder.GetProperty( m_decoder.POSITION );
}

// NOTE: Only supports seeking forward right now
void CMilesMP3::SetPosition( unsigned int position )
{
	m_decoder.Seek( position );
}

#else

// Created by Joshua Ashton and Josh Dowell (Slartibarty)
// joshua@froggi.es

// Each chunk is 4096 bytes because that is the size of AUDIOSOURCE_COPYBUF_SIZE
// which is used when streaming in CWaveDataStreamAsync::ReadSourceData
// and going above it's gives us garbage data back.
constexpr int kChunkSize    = 4096;
// 4 chunks of 4KB to make 16KB, or at least 10 frames.
// The buffer we are always called to fill is always 16KB,
// so this will ensure we will always saturate that buffer unless
// we reach the EOF even in the case we need to re-sync if we
// somehow got misaligned, our position got force set and there was garbage
// data in the stream, etc.
constexpr int kChunkCount   = 4;

//-----------------------------------------------------------------------------
// Implementation of IAudioStream
//-----------------------------------------------------------------------------
class CMiniMP3AudioStream final : public IAudioStream
{
public:
	CMiniMP3AudioStream( IAudioStreamEvent *pEventHandler );

	int				Decode( void *pBuffer, unsigned uBufferSize ) override;

	int				GetOutputBits() override;
	int				GetOutputRate() override;
	int				GetOutputChannels() override;
	unsigned		GetPosition() override;
	void			SetPosition( unsigned uPosition ) override;

private:

	void			UpdateStreamInfo();
	// Returns true if it hit EOF
	bool 			StreamChunk( int nChunkIdx );
	// Returns number of samples
	int				DecodeFrame( void *pBuffer );

	unsigned SamplesToBytes( int nSamples ) const;
	unsigned GetTotalChunkSizes() const;

	mp3dec_t				m_Decoder;
	mp3dec_frame_info_t		m_Info;

	// Diagram of how the chunk system below fits into
	// a mp3 data stream.
	// The 'frame' cursor is local to the chunks.
	// The 'data' cursor is how far along we are in picking
	// up chunks.
	//----------------------------------------------------
	//      | chunk 1 | chunk 2 | chunk 3 | chunk 4 |
	//----------------------------------------------------
	//              ^                               ^
	//            frame                           data

	// Position of the 'data' cursor, used to fill
	// m_Frames.
	unsigned 			m_uDataPosition = 0;
	// Position of the 'frame' cursor, inside of
	// m_Frames.
	unsigned			m_uFramePosition = 0;

	IAudioStreamEvent*		m_pEventHandler;

	// Buffers for the current frames.
	// See comments describing the chunk size relationship at
	// the definition of kChunkSize and kChunkCount.
	union
	{
		uint8_t				m_FullFrame	[kChunkSize * kChunkCount];
		uint8_t				m_Chunks	[kChunkCount][kChunkSize];
	} m_Frames;

	int m_nChunkSize[kChunkCount] = { 0 };

	unsigned m_nEOFPosition = std::numeric_limits<unsigned>::max();
};

CMiniMP3AudioStream::CMiniMP3AudioStream( IAudioStreamEvent *pEventHandler )
	: m_pEventHandler( pEventHandler )
{
	mp3dec_init( &m_Decoder );

	memset( &m_Info, 0, sizeof( m_Info ) );
	memset( &m_Frames, 0, sizeof( m_Frames ) );
	
	UpdateStreamInfo();
}

int	CMiniMP3AudioStream::Decode( void *pBuffer, unsigned uBufferSize )
{
	constexpr unsigned kSamplesPerFrameBufferSize = MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof( short );
	if ( uBufferSize < kSamplesPerFrameBufferSize )
	{
		AssertMsg( false, "Decode called with %u < %u!", uBufferSize, kSamplesPerFrameBufferSize );
		return 0;
	}

	unsigned uSampleBytes = 0;
	while ( ( uBufferSize - uSampleBytes ) > kSamplesPerFrameBufferSize )
	{
		// Offset the buffer by the number of samples bytes we've got so far.
		char *pFrameBuffer = static_cast<char*>( pBuffer ) + uSampleBytes;

		int nFrameSamples = DecodeFrame( pFrameBuffer );
		if ( !nFrameSamples )
			break;

		uSampleBytes += SamplesToBytes( nFrameSamples );
	}

	// If we got no samples back and didnt hit EOF,
	// don't return 0 because it still end playback.
	//
	// If this is a streaming MP3, this is just from judder,
	// so just fill with 1152 samples of 0.
	const bool bEOF = m_uDataPosition >= m_nEOFPosition;
	if ( uSampleBytes == 0 && !bEOF )
	{
		const unsigned kZeroSampleBytes = max( SamplesToBytes( 1152 ), uBufferSize );
		memset( pBuffer, 0, kZeroSampleBytes );
		return kZeroSampleBytes;
	}

	return int( uSampleBytes );
}

int CMiniMP3AudioStream::GetOutputBits()
{
	// Unused, who knows what it's supposed to return.
	return m_Info.bitrate_kbps * 100;
}

int CMiniMP3AudioStream::GetOutputRate()
{
	return m_Info.hz;
}

int CMiniMP3AudioStream::GetOutputChannels()
{
	// Must return at least 1 in an error state
	// or the engine will do a nasty div by 0.
	return Max( m_Info.channels, 1 );
}

unsigned CMiniMP3AudioStream::GetPosition()
{
	// Current position is ( our data position - size of our cached chunks ) + position inside of them.
	return ( m_uDataPosition - sizeof( m_Frames ) ) + m_uFramePosition;
}

void CMiniMP3AudioStream::SetPosition( unsigned int uPosition )
{
	m_uDataPosition = uPosition;
	m_uFramePosition = 0;

	UpdateStreamInfo();
}

void CMiniMP3AudioStream::UpdateStreamInfo()
{
	// Pre-fill all frames.
	for ( int i = 0; i < kChunkCount; i++ )
	{
		const bool bEOF = StreamChunk( i );
		if ( bEOF )
		{
			m_nEOFPosition = m_uDataPosition;
			break;
		}
	}

	// Decode a frame to get the latest info, maybe we transitioned from
	// stereo <-> mono, etc.
	mp3dec_decode_frame( &m_Decoder, &m_Frames.m_FullFrame[ m_uFramePosition ], GetTotalChunkSizes() - m_uFramePosition, nullptr, &m_Info );
}

bool CMiniMP3AudioStream::StreamChunk( int nChunkIdx )
{
	m_nChunkSize[nChunkIdx] = m_pEventHandler->StreamRequestData( &m_Frames.m_Chunks[ nChunkIdx ], kChunkSize, m_uDataPosition );

	m_uDataPosition += m_nChunkSize[nChunkIdx];

	// Check if we hit EOF (ie. chunk size != max) and mark the EOF position
	// so we know when to stop playing.
	return m_nChunkSize[nChunkIdx] != kChunkSize;
}

int CMiniMP3AudioStream::DecodeFrame( void *pBuffer )
{
	// If we are past the first two chunks, move those two chunks back and load two new ones.
	//
	// This part of the code assumes the chunk count to be 4, so if you change that,
	// check here. You shouldn't need more than 4 4KB chunks making 16KB though...
	COMPILE_TIME_ASSERT( kChunkCount == 4 );

	while ( m_uFramePosition >= 2 * kChunkSize && m_uDataPosition < m_nEOFPosition )
	{
		// Chunk 0 <- Chunk 2
		// Chunk 1 <- Chunk 3
		memcpy( &m_Frames.m_Chunks[0], &m_Frames.m_Chunks[2], kChunkSize );
		memcpy( &m_Frames.m_Chunks[1], &m_Frames.m_Chunks[3], kChunkSize );

		m_nChunkSize[0] = m_nChunkSize[2];
		m_nChunkSize[1] = m_nChunkSize[3];
		m_nChunkSize[2] = 0;
		m_nChunkSize[3] = 0;

		// Move our frame position back by two chunks
		m_uFramePosition -= 2 * kChunkSize;

		// Grab a new Chunk 2 + 3
		for ( int i = 0; i < 2; i++ )
		{
			const int nChunkIdx = 2 + i;

			// StreamChunk returns if we hit EOF.
			const bool bEOF = StreamChunk( nChunkIdx );

			// If we did hit EOF, break out here, cause we don't need
			// to get the next chunk if there is one left to get.
			// It's okay if it never gets data, as m_nChunkSize[3] is set to 0
			// when we move the chunks back.
			if ( bEOF )
			{
				m_nEOFPosition = m_uDataPosition;
				break;
			}
		}
	}

	int nSamples = mp3dec_decode_frame( &m_Decoder, &m_Frames.m_FullFrame[ m_uFramePosition ], GetTotalChunkSizes() - m_uFramePosition, reinterpret_cast< short* >( pBuffer ), &m_Info );

	m_uFramePosition += m_Info.frame_bytes;

	return nSamples;
}

unsigned CMiniMP3AudioStream::SamplesToBytes( int nSamples ) const
{
	return nSamples * sizeof( short ) * m_Info.channels;
}

unsigned CMiniMP3AudioStream::GetTotalChunkSizes() const
{
	int nTotalSize = 0;
	for ( auto s : m_nChunkSize )
		nTotalSize += s;

	return nTotalSize;
}

#endif


class CVAudio : public IVAudio
{
public:
	CVAudio()
	{
		// Assume the user will be creating multiple miles objects, so
		// keep miles running while this exists
		IncrementRefMiles();
	}

	virtual ~CVAudio() override
	{
		DecrementRefMiles();
	}

	IAudioStream *CreateMP3StreamDecoder( IAudioStreamEvent *pEventHandler ) override
	{
#ifdef SE_USE_MILES_MP3
		CMilesMP3 *pMP3 = new CMilesMP3;
		if ( !pMP3->Init( pEventHandler ) )
		{
			delete pMP3;
			return NULL;
		}
#else
		CMiniMP3AudioStream *pMP3 = new CMiniMP3AudioStream(pEventHandler);
#endif
		
		return pMP3;
	}

	void DestroyMP3StreamDecoder( IAudioStream *pDecoder ) override
	{
		delete pDecoder;
	}

	void *CreateMilesAudioEngine() override
	{
		IncrementRefMiles();
		// dimhotepus: Use system configuration instead of 5:1 as it is now always the case.
		HDIGDRIVER hDriver = AIL_open_digital_driver( 44100, 16, MSS_MC_USE_SYSTEM_CONFIG, 0 );
		if ( !hDriver )
		{
			Warning( "Unable to open Miles Audio driver on 44100 Hz / 16 bits / system speaker config: %s", AIL_last_error() );
		}
		return hDriver;
	}

	void DestroyMilesAudioEngine( void *pEngine ) override
	{
		HDIGDRIVER hDriver = HDIGDRIVER(pEngine);
		AIL_close_digital_driver( hDriver );
		DecrementRefMiles();
	}
};

void* CreateVoiceCodec_CVAudio()
{
	return new CVAudio;
}

EXPOSE_INTERFACE_FN(CreateVoiceCodec_CVAudio, IVAudio, VAUDIO_INTERFACE_VERSION );

