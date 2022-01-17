#pragma once
#include "tag.h"

class CID3V2Tag :
	public CTag
{
public:
	static CID3V2Tag* FindTag(CMPAStream* pStream, bool bAppended, DWORD dwBegin, DWORD dwEnd);	
	~CID3V2Tag(void);

private:
	CID3V2Tag(CMPAStream* pStream, bool bAppended, DWORD dwOffset);
	static DWORD GetSynchsafeInteger(DWORD dwValue);

};
