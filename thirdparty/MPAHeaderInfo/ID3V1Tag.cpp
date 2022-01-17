#include "StdAfx.h"
#include ".\id3v1tag.h"



CID3V1Tag* CID3V1Tag::FindTag(CMPAStream* pStream, bool bAppended, DWORD dwBegin, DWORD dwEnd)
{
	if (bAppended)
	{
		// stands 128 byte before file end
		DWORD dwOffset = dwEnd - 128;
		BYTE* pBuffer = pStream->ReadBytes(3, dwOffset, false);
		if (memcmp("TAG", pBuffer, 3)==0)
			return new CID3V1Tag(pStream, dwOffset);
	}
	return NULL;
}


CID3V1Tag::CID3V1Tag(CMPAStream* pStream, DWORD dwOffset) :
	CTag(pStream, _T("ID3"), true, dwOffset, 128)
{
	dwOffset += 125;
	BYTE* pBuffer = pStream->ReadBytes(2, dwOffset, false);

	bool bIsV11 = false;
	// check if V1.1 tag (byte 125 = 0 and byte 126 != 0)
	if (pBuffer[0] == '\0' && pBuffer[1] != '\0')
		bIsV11 = true;
	SetVersion(1,(bIsV11?1:0));
}

CID3V1Tag::~CID3V1Tag(void)
{
}
