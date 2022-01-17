#pragma once
#include "tag.h"

class CLAMETag :
	public CTag
{
public:
	static CLAMETag* CLAMETag::FindTag(CMPAStream* pStream, bool bAppended, DWORD dwBegin, DWORD dwEnd);
	~CLAMETag(void);

	CString m_strEncoder;
	DWORD m_dwLowpassFilterHz;
	BYTE m_bBitrate;	// in kbps
	BYTE m_bRevision;

	bool IsVBR() const;
	bool IsABR() const;
	bool IsCBR() const;
	LPCTSTR GetVBRInfo() const;
	bool IsSimpleTag() const { return m_bSimpleTag; };

private:
	CLAMETag(CMPAStream* pStream, bool bAppended, DWORD dwOffset);
	
	BYTE m_bVBRInfo;
	bool m_bSimpleTag;
	static LPCTSTR m_szVBRInfo[10];
};
