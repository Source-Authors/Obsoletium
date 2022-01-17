#pragma once
#include "mpaframe.h"
#include "vbrheader.h"


class CVBRIHeader :
	public CVBRHeader
{
public:
	static CVBRIHeader* FindHeader(const CMPAFrame* pFrame);

	CVBRIHeader(const CMPAFrame* pFrame, DWORD dwOffset);
	virtual ~CVBRIHeader(void);

	virtual DWORD SeekPosition(float& fPercent) const;
	DWORD SeekPositionByTime(float fEntryTimeMS) const;

	// these values exist only in VBRI headers
	float m_fDelay;	
	DWORD m_dwTableScale;	// for seeking
	DWORD m_dwBytesPerEntry;
    DWORD m_dwFramesPerEntry;
	DWORD m_dwVersion;
private:
	DWORD m_dwLengthSec;
};
