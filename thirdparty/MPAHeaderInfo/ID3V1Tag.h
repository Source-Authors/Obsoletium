#pragma once
#include "tag.h"

class CID3V1Tag :
	public CTag
{
public:
	static CID3V1Tag* FindTag(CMPAStream* pStream, bool bAppended, DWORD dwBegin, DWORD dwEnd);
	~CID3V1Tag(void);

private:
	CID3V1Tag(CMPAStream* pStream, DWORD dwOffset);
};
