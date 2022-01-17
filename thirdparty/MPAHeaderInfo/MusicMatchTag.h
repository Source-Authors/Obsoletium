#pragma once
#include "tag.h"

class CMusicMatchTag :
	public CTag
{
public:
	static CMusicMatchTag* FindTag(CMPAStream* pStream, bool bAppended, DWORD dwBegin, DWORD dwEnd);
	~CMusicMatchTag(void);

private:
	CMusicMatchTag(CMPAStream* pStream, DWORD dwOffset);
	
};
