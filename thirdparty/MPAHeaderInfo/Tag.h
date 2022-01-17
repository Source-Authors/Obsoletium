#pragma once
#include "mpastream.h"

class CTag
{
public:
	DWORD GetOffset() const { return m_dwOffset; };
	DWORD GetEnd() const { return m_dwOffset + m_dwSize; };
	LPCTSTR GetName() const { return m_szName; };
	DWORD GetSize() const { return m_dwSize; };
	float GetVersion() const { return m_fVersion; };
	
	virtual ~CTag(void);
protected:
	CMPAStream* m_pStream;

	CTag(CMPAStream* pStream, LPCTSTR szName, bool bAppended, DWORD dwOffset = 0, DWORD dwSize = 0);

	DWORD m_dwOffset;	// beginning of tag
	DWORD m_dwSize;		// size of tag
	bool m_bAppended;	// true if at the end of file
	float m_fVersion;	// format x.yz
	LPTSTR m_szName;	// name of tag

	void SetVersion(BYTE bVersion1, BYTE bVersion2=0, BYTE bVersion3=0);
};
