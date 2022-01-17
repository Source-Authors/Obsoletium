#pragma once
#include "tag.h"

class CAPETag :
	public CTag
{
public:
	static CAPETag* CAPETag::FindTag(CMPAStream* pStream, bool bAppended, DWORD dwBegin, DWORD dwEnd);
	~CAPETag(void);

	

private:
	CAPETag(CMPAStream* pStream, bool bAppended, DWORD dwOffset);
};
