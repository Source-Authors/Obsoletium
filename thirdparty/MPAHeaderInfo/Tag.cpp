#include "StdAfx.h"
#include ".\tag.h"

CTag::CTag(CMPAStream* pStream, LPCTSTR szName, bool bAppended, DWORD dwOffset, DWORD dwSize):
	m_pStream(pStream), m_bAppended(bAppended), m_dwOffset(dwOffset), m_dwSize(dwSize)
{
	m_szName = _tcsdup(szName);
}

CTag::~CTag(void)
{
	free(m_szName);
}

void CTag::SetVersion(BYTE bVersion1, BYTE bVersion2, BYTE bVersion3)
{
	// only values between 0 and 9 are displayed correctly
	m_fVersion = bVersion1;
	m_fVersion += bVersion2 * 0.1f;
	m_fVersion += bVersion3 * 0.01f;
}