#pragma once
#include "tag.h"

class CLyrics3Tag :
	public CTag
{
public:
	static CLyrics3Tag* FindTag(CMPAStream* pStream, bool bAppended, DWORD dwBegin, DWORD dwEnd);
	~CLyrics3Tag(void);

private:
	CLyrics3Tag(CMPAStream* pStream, DWORD dwOffset, bool bVersion2);
};
