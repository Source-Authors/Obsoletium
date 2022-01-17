#pragma once
#include "mpaframe.h"
#include "vbrheader.h"
#include "lametag.h"


class CXINGHeader :
	public CVBRHeader
{
public:
	static CXINGHeader* FindHeader(const CMPAFrame* pFrame);

	CXINGHeader(const CMPAFrame* pFrame, DWORD dwOffset);
	virtual ~CXINGHeader(void);

	virtual DWORD SeekPosition(float& fPercent) const;

	CLAMETag* m_pLAMETag;
};
