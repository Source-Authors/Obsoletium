#include "stdafx.h"

#include "mpaexception.h"
#include ".\mpafile.h"


/// CMPAFile
//////////////////////////////////////////



CMPAFile::CMPAFile(LPCTSTR szFile)
{
	DWORD dwOffset = 0;
	m_pStream = new CMPAFileStream(szFile);
	m_pTags = new CTags(m_pStream);
	
	// find first valid MPEG frame
	m_pFirstFrame = new CMPAFrame(m_pStream, dwOffset, true, false, false, NULL);

	// check for VBR header
	m_pVBRHeader = m_pFirstFrame->FindVBRHeader();
	
	CalcBytesPerSec();
}

// destructor
CMPAFile::~CMPAFile(void)
{
	if (m_pVBRHeader)
		delete m_pVBRHeader;

	delete m_pFirstFrame;
	delete m_pTags;
	delete m_pStream;

}
#define MAX_EMPTY_FRAMES 25

// try to guess the bitrate of the whole file as good as possible
void CMPAFile::CalcBytesPerSec()
{
	// in case of a VBR header we know the bitrate (if at least the number of frames is known)
	if (m_pVBRHeader && m_pVBRHeader->m_dwFrames)
	{
		DWORD dwBytes = m_pVBRHeader->m_dwBytes;
		// number of bytes can be guessed
		if (!dwBytes)
			m_pVBRHeader->m_dwBytes = GetFileSize();
		
		m_dwBytesPerSec = m_pFirstFrame->m_pHeader->GetBytesPerSecond(m_pVBRHeader->m_dwFrames, dwBytes);
		return;
	}
	
	// otherwise we have to guess it
	
	// go through all frames that have a lower bitrate than 48kbit
	CMPAFrame* pFrame = m_pFirstFrame;
	DWORD dwFrames = 0;
	bool bDeleteFrame = false;
	
	while (pFrame && pFrame->m_pHeader->m_dwBitrate <= 48000)
	{
		pFrame = GetFrame(Next, pFrame, bDeleteFrame);
		if (dwFrames++ > MAX_EMPTY_FRAMES)	
			break;
		bDeleteFrame = true;
	};

	if (!pFrame) {
		pFrame = m_pFirstFrame;
		bDeleteFrame = false;
	}
	m_dwBytesPerSec = pFrame->m_pHeader->GetBytesPerSecond();
	
	if (bDeleteFrame)
		delete pFrame;
	//return true;
}

#define MIN_FRAME_SIZE 24	// MPEG2, LayerIII, 8kbps, 24kHz => Framesize = 24 Bytes

CMPAFrame* CMPAFile::GetFrame(CMPAFile::GetType Type, CMPAFrame* pFrame, bool bDeleteOldFrame, DWORD dwOffset)
{
	CMPAFrame* pNewMPAFrame;
	CMPAHeader* pCompHeader = NULL;
	bool bSubsequentFrame = true;
	bool bReverse = false;
	bool bExactOffset = true;
	
	switch(Type)
	{
		case First:
			dwOffset = GetBegin();
			bSubsequentFrame = true;
			bExactOffset = false;
			break;
		case Last:
			dwOffset = GetEnd() - MIN_FRAME_SIZE;
			bReverse = true;
			bExactOffset = false;
			pCompHeader = m_pFirstFrame->m_pHeader;
			break;
		case Next:
			if (!pFrame)
				return NULL;
			pCompHeader = m_pFirstFrame->m_pHeader;
			dwOffset = pFrame->GetSubsequentHeaderOffset();
			break;
		case Prev:
			if (!pFrame)
				return NULL;
			dwOffset = pFrame->m_dwOffset-MIN_FRAME_SIZE;
			bReverse = true;
			bExactOffset = false;
			pCompHeader = m_pFirstFrame->m_pHeader;
			break;
		case Resync:
			bSubsequentFrame = true;
			bExactOffset = false;
			//pCompHeader = m_pFrame->m_pHeader;
			break;
		default:
			return NULL;
	}

	try
	{
		pNewMPAFrame = new CMPAFrame(m_pStream, dwOffset, bSubsequentFrame, bExactOffset, bReverse, pCompHeader);
		
		
	}
	catch(CMPAException& e)
	{
		// try a complete resync from position dwOffset
		if (Type == Next)
		{
			return GetFrame(Resync, pFrame, bDeleteOldFrame, dwOffset);
		}
		OutputDebugString(e.GetErrorDescription());
		pNewMPAFrame = NULL;
	}
	if (pFrame && bDeleteOldFrame)
		delete pFrame;
	return pNewMPAFrame;
}

