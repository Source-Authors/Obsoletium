#pragma once
#include "mpastream.h"

#define MPA_HEADER_SIZE 4	// MPEG-Audio Header Size 32bit
#define MAXTIMESREAD 5

class CMPAHeader
{
public:
	
	CMPAHeader(CMPAStream* pStream, DWORD& dwOffset, bool bExactOffset, bool bReverse, CMPAHeader* pCompareHeader);
	~CMPAHeader();
	
	bool operator==(CMPAHeader& DestHeader) const;
	
	DWORD CalcFrameSize() const {	return int(((m_dwCoefficients[m_bLSF][m_Layer] * m_dwBitrate / m_dwSamplesPerSec) + m_dwPaddingSize)) * m_dwSlotSizes[m_Layer]; };

	// bitrate is in bit per second, to calculate in bytes => (/ 8)
	DWORD GetBytesPerSecond() const { return m_dwBitrate / 8; };
	// calc number of seconds from number of frames
	DWORD GetLengthSecond(DWORD dwNumFrames) const { return dwNumFrames * m_dwSamplesPerFrame / m_dwSamplesPerSec; };
	DWORD GetBytesPerSecond(DWORD dwNumFrames, DWORD dwNumBytes) const { return dwNumBytes / GetLengthSecond( dwNumFrames ); };
	bool IsMono() const { return (m_ChannelMode == SingleChannel)?true:false; };
	// true if MPEG2/2.5 otherwise false
	bool IsLSF() const { return m_bLSF;	};
	DWORD GetSideInfoSize() const { return m_dwSideInfoSizes[IsLSF()][IsMono()]; };
	DWORD GetNumChannels() const { return IsMono()?1:2; };

private:
	static const DWORD m_dwMaxRange;
	static const DWORD m_dwTolerance;

	static const DWORD m_dwSamplingRates[4][3];
	static const DWORD m_dwPaddingSizes[3];
	static const DWORD m_dwBitrates[2][3][15];
	static const bool m_bAllowedModes[15][2];
	static const DWORD m_dwSamplesPerFrames[2][3];
	static const DWORD m_dwCoefficients[2][3];
	static const DWORD m_dwSlotSizes[3];
	static const DWORD m_dwSideInfoSizes[2][2];

	bool m_bLSF;		// true means lower sampling frequencies (=MPEG2/MPEG2.5)

	void Init(BYTE* pHeader, LPCTSTR szFilename);

public:
	static LPCTSTR m_szLayers[];
	static LPCTSTR m_szMPEGVersions[];
	static LPCTSTR m_szChannelModes[];
	static LPCTSTR m_szEmphasis[];

	enum MPAVersion
	{
		MPEG25 = 0,
		MPEGReserved,
		MPEG2,
		MPEG1		
	}m_Version;

	enum MPALayer
	{
		Layer1,
		Layer2,
		Layer3,
		LayerReserved
	}m_Layer;

	enum Emphasis
	{
		EmphNone = 0,
		Emph5015,
		EmphReserved,
		EmphCCITJ17
	}m_Emphasis;

	enum ChannelMode
	{
		Stereo,
		JointStereo,
		DualChannel,
		SingleChannel
	}m_ChannelMode;
	
	DWORD m_dwSamplesPerSec;
	DWORD m_dwSamplesPerFrame;
	DWORD m_dwBitrate;	// in bit per second (1 kb = 1000 bit, not 1024)
	DWORD m_dwPaddingSize;
	WORD m_wBound;		// only valid for intensity stereo
	WORD m_wAllocationTableIndex;	// only valid for MPEG 1 Layer II (0=table a, 1=table b,...)
	
	// flags
	bool m_bCopyright, m_bPrivate, m_bOriginal;
	bool m_bCRC; 
	BYTE m_ModeExt;
};
