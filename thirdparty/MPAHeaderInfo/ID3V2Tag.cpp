#include "StdAfx.h"
#include ".\id3v2tag.h"


CID3V2Tag* CID3V2Tag::FindTag(CMPAStream* pStream, bool bAppended, DWORD dwBegin, DWORD dwEnd)
{
	char* szID;
	DWORD dwOffset;

	if (!bAppended)
	{
		// stands at the beginning of file (complete header is 10 bytes)
		dwOffset = dwBegin;
		szID = "ID3";
	}
	else
	{
		// stands at the end of the file (complete footer is 10 bytes)
		dwOffset = dwEnd - 10;
		szID = "3DI";
		
	}
	BYTE* pBuffer = pStream->ReadBytes(10, dwOffset, false);

	if (memcmp(szID, pBuffer, 3) == 0)
		return new CID3V2Tag(pStream, bAppended, dwOffset);
	return NULL;
}


CID3V2Tag::CID3V2Tag(CMPAStream* pStream, bool bAppended, DWORD dwOffset) :
	CTag(pStream, _T("ID3"), false, dwOffset)
{
	dwOffset += 3;

	// read out version info
	BYTE* pBuffer = m_pStream->ReadBytes(3, dwOffset);
	SetVersion(2, pBuffer[0], pBuffer[1]);
	BYTE bFlags = pBuffer[3];
	/*m_bUnsynchronization = (bFlags & 0x80)?true:false;	// bit 7
	m_bExtHeader = (bFlags & 0x40)?true:false;			// bit 6
	m_bExperimental = (bFlags & 0x20)?true:false;		// bit 5*/
	bool bFooter = (bFlags & 0x10)?true:false;				// bit 4

	// convert synchsafe integer
	m_dwSize = GetSynchsafeInteger(m_pStream->ReadBEValue(4, dwOffset)) + (bFooter?20:10);

	// for appended tag: calculate new offset
	if (bAppended)
		m_dwOffset -= m_dwSize - 10;
}

// return for each byte only lowest 7 bits (highest bit always zero)
DWORD CID3V2Tag::GetSynchsafeInteger(DWORD dwValue)
{
	// masks first 7 bits
	const DWORD dwMask = 0x7F000000;
	DWORD dwReturn = 0;
	for (int n=0; n < sizeof(DWORD); n++)
	{
		dwReturn = (dwReturn << 7) | ((dwValue & dwMask) >> 24);
		dwValue <<= 8;
	}
	return dwReturn;
}

CID3V2Tag::~CID3V2Tag(void)
{
}
