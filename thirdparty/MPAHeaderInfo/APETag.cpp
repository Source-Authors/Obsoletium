#include "StdAfx.h"
#include ".\apetag.h"

CAPETag* CAPETag::FindTag(CMPAStream* pStream, bool bAppended, DWORD dwBegin, DWORD dwEnd)
{
	DWORD dwOffset;

	if (!bAppended)
	{
		// stands at the beginning of file (complete header is 32 bytes)
		dwOffset = dwBegin;
	}
	else
	{
		// stands at the end of the file (complete footer is 32 bytes)
		dwOffset = dwEnd - 32;		
	}
	BYTE* pBuffer = pStream->ReadBytes(8, dwOffset, false);

	if (memcmp("APETAGEX", pBuffer, 8) == 0)
		return new CAPETag(pStream, bAppended, dwOffset);
	return NULL;
}

CAPETag::CAPETag(CMPAStream* pStream, bool bAppended, DWORD dwOffset) :
	CTag(pStream, _T("APE"), bAppended, dwOffset)
{
	dwOffset += 8;
	DWORD dwVersion = pStream->ReadLEValue(4, dwOffset);
	
	// first byte is version number
	m_fVersion = dwVersion/1000.0f;
	
	// get size
	m_dwSize = pStream->ReadLEValue(4, dwOffset);

	/*DWORD dwNumItems = */pStream->ReadLEValue(4, dwOffset);

	// only valid for version 2
	DWORD dwFlags = pStream->ReadLEValue(4, dwOffset);
	bool bHeader, bFooter;
	if (m_fVersion > 1.0f)
	{
		bHeader = dwFlags >> 31 & 0x1;
		bFooter = dwFlags >> 30 & 0x1;
	}
	else
	{
		bHeader = false;
		bFooter = true;
	}

	if (bHeader)
		m_dwSize += 32;	// add header

	if (bAppended)
		m_dwOffset -= (m_dwSize - 32);
}

CAPETag::~CAPETag(void)
{
}
