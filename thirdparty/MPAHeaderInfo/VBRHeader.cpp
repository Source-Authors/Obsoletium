// dimhotepus: Add missed header.
#include "audio_pch.h"

#include "stdafx.h"
#include "mpaframe.h"	// also includes vbrheader.h

#include "xingheader.h"
#include "vbriheader.h"

// first test with this static method, if it does exist
CVBRHeader* CVBRHeader::FindHeader(const CMPAFrame* pFrame)
{
	_ASSERTE(pFrame);
	CVBRHeader* pVBRHeader = NULL;

	pVBRHeader = CXINGHeader::FindHeader(pFrame);
	if (!pVBRHeader)
		pVBRHeader = CVBRIHeader::FindHeader(pFrame);
	
	return pVBRHeader;
}

CVBRHeader::CVBRHeader(CMPAStream* pStream, DWORD dwOffset) :
	m_pStream(pStream), m_pnToc(NULL), m_dwOffset(dwOffset), m_dwFrames(0), m_dwBytes(0), m_dwQuality(0), m_dwTableSize(0)
{
}

bool CVBRHeader::CheckID(CMPAStream* pStream, DWORD dwOffset, char ch0, char ch1, char ch2, char ch3)
{
	BYTE* pBuffer = pStream->ReadBytes(4, dwOffset, false);
	if (pBuffer[0] == ch0 && pBuffer[1] == ch1 && pBuffer[2] == ch2 && pBuffer[3] == ch3)
		return true;
	return false;
}

/*
// currently not used
bool CVBRHeader::ExtractLAMETag( DWORD dwOffset )
{
	// LAME ID found?
	if( !CheckID( m_pMPAFile, dwOffset, 'L', 'A', 'M', 'E' ) && !CheckID( m_pMPAFile, dwOffset, 'G', 'O', 'G', 'O' ) )
		return false;

	return true;
}*/



CVBRHeader::~CVBRHeader(void)
{
	delete[] m_pnToc;
}

// get byte position for percentage value (fPercent) of file
bool CVBRHeader::SeekPosition(float& fPercent, DWORD& dwSeekPoint) const
{
	if (!m_pnToc || m_dwBytes == 0)
		return false;

	// check range of fPercent
	if (fPercent < 0.0f)   
		fPercent = 0.0f;
	if (fPercent > 99.0f) 
		fPercent = 99.0f;

	dwSeekPoint = SeekPosition(fPercent);
	return true;
}
