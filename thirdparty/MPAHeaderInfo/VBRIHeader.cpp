// dimhotepus: Add missed header.
#include "audio_pch.h"

#include "StdAfx.h"
#include ".\vbriheader.h"

CVBRIHeader* CVBRIHeader::FindHeader(const CMPAFrame* pFrame)
{
	// VBRI header always at fixed offset
	DWORD dwOffset = pFrame->m_dwOffset + MPA_HEADER_SIZE + 32;
	
	// VBRI ID found?
	if (!CheckID(pFrame->m_pStream, dwOffset, 'V', 'B', 'R', 'I'))
		return NULL;
	
	return new CVBRIHeader(pFrame, dwOffset);
}


CVBRIHeader::CVBRIHeader(const CMPAFrame* pFrame, DWORD dwOffset) :
	CVBRHeader(pFrame->m_pStream, dwOffset)
{
	/* FhG VBRI Header

	size	description
	4		'VBRI' (ID)
	2		version
	2		delay
	2		quality
	4		# bytes
	4		# frames
	2		table size (for TOC)
	2		table scale (for TOC)
	2		size of table entry (max. size = 4 byte (must be stored in an integer))
	2		frames per table entry
	
	??		dynamic table consisting out of frames with size 1-4
			whole length in table size! (for TOC)

	*/

    // ID is already checked at this point
	dwOffset += 4;

	// extract all fields from header (all mandatory)
	m_dwVersion = m_pStream->ReadBEValue(2, dwOffset);
	m_fDelay = (float)m_pStream->ReadBEValue(2, dwOffset);
	m_dwQuality = m_pStream->ReadBEValue(2, dwOffset);
	m_dwBytes = m_pStream->ReadBEValue(4, dwOffset);
	m_dwFrames = m_pStream->ReadBEValue(4, dwOffset);
	m_dwTableSize = m_pStream->ReadBEValue(2, dwOffset) + 1;	//!!!
	m_dwTableScale = m_pStream->ReadBEValue(2, dwOffset);
	m_dwBytesPerEntry = m_pStream->ReadBEValue(2, dwOffset);
	m_dwFramesPerEntry = m_pStream->ReadBEValue(2, dwOffset);

	// extract TOC  (for more accurate seeking)
	m_pnToc = new int[m_dwTableSize];
	if (m_pnToc)
	{
		for (unsigned int i = 0 ; i < m_dwTableSize ; i++)
		{
			m_pnToc[i] = m_pStream->ReadBEValue(m_dwBytesPerEntry, dwOffset);
		}
	}

	// get length of file (needed for seeking)
	m_dwLengthSec = pFrame->m_pHeader->GetLengthSecond(m_dwFrames);
}

CVBRIHeader::~CVBRIHeader(void)
{
}

DWORD CVBRIHeader::SeekPosition(float& fPercent) const
{
	return SeekPositionByTime((fPercent/100.0f) * m_dwLengthSec * 1000.0f);
}

DWORD CVBRIHeader::SeekPositionByTime(float fEntryTimeMS) const
{
	unsigned int i=0,  fraction = 0;
	DWORD dwSeekPoint = 0;

	float fLengthMS;
	float fLengthMSPerTOCEntry;
	float fAccumulatedTimeMS = 0.0f ;
	 
	fLengthMS = (float)m_dwLengthSec * 1000.0f ;
	fLengthMSPerTOCEntry = fLengthMS / (float)m_dwTableSize;
	 
	if (fEntryTimeMS > fLengthMS) 
		fEntryTimeMS = fLengthMS; 
	 
	while (fAccumulatedTimeMS <= fEntryTimeMS)
	{
		dwSeekPoint += m_pnToc[i++];
		fAccumulatedTimeMS += fLengthMSPerTOCEntry;
	}
	  
	// Searched too far; correct result
	fraction = ( (int)((((fAccumulatedTimeMS - fEntryTimeMS) / fLengthMSPerTOCEntry) 
				+ (1.0f/(2.0f*(float)m_dwFramesPerEntry))) * (float)m_dwFramesPerEntry));

	dwSeekPoint -= (DWORD)((float)m_pnToc[i-1] * (float)(fraction) 
					/ (float)m_dwFramesPerEntry);

	return dwSeekPoint;
}
