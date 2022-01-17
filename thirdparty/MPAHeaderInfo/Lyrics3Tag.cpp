#include "StdAfx.h"
#include ".\lyrics3tag.h"
#include "mpaexception.h"

CLyrics3Tag* CLyrics3Tag::FindTag(CMPAStream* pStream, bool bAppended, DWORD dwBegin, DWORD dwEnd)
{
	// stands at the end of file
	DWORD dwOffset = dwEnd - 9;
	BYTE* pBuffer = pStream->ReadBytes(9, dwOffset, false, true);

	// is it Lyrics 2 Tag
	if (memcmp("LYRICS200", pBuffer, 9) == 0)
		return new CLyrics3Tag(pStream, dwOffset, true);
	else if (memcmp("LYRICSEND", pBuffer, 9) == 0)
		return new CLyrics3Tag(pStream, dwOffset, false);
	
	return NULL;
}

CLyrics3Tag::CLyrics3Tag(CMPAStream* pStream, DWORD dwOffset, bool bVersion2) :
	CTag(pStream, _T("Lyrics3"), true, dwOffset)
{
	BYTE* pBuffer;
	if (bVersion2)
	{
		SetVersion(2);
		
		// look for size of tag (stands before dwOffset)
		dwOffset -= 6;
		pBuffer = pStream->ReadBytes(6, dwOffset, false); 

		// add null termination
		char szSize[7];
		memcpy(szSize, pBuffer, 6);
		szSize[6] = '\0';

		// convert string to integer
		m_dwSize = atoi(szSize); 
		m_dwOffset = dwOffset - m_dwSize;
		m_dwSize += 6 + 9;	// size must include size info and end string
	}
	else
	{
		SetVersion(1);

		// seek back 5100 bytes and look for LYRICSBEGIN
		m_dwOffset -= 5100;
		pBuffer = pStream->ReadBytes(11, m_dwOffset, false);

		while (memcmp("LYRICSBEGIN", pBuffer, 11) != 0)
		{
			if (dwOffset >= m_dwOffset)
				throw CMPAException(CMPAException::CorruptLyricsTag);
		}
		m_dwSize = (dwOffset - m_dwOffset) + 9;
	}
}


CLyrics3Tag::~CLyrics3Tag(void)
{
}
