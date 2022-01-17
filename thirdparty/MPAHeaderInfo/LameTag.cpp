#include "StdAfx.h"
#include ".\lametag.h"


LPCTSTR CLAMETag::m_szVBRInfo[10] = 
{
	_T("Unknown"),
	_T("CBR"),
	_T("ABR"),
	_T("VBR1"),
	_T("VBR2"),
	_T("VBR3"),
	_T("VBR4"),
	_T("Reserved"),
	_T("CBR2Pass"),
	_T("ABR2Pass")
};


CLAMETag* CLAMETag::FindTag(CMPAStream* pStream, bool bAppended, DWORD dwBegin, DWORD dwEnd)
{
	// check for LAME Tag extension (always 120 bytes after XING ID)
	DWORD dwOffset = dwBegin + 120;

	BYTE* pBuffer = pStream->ReadBytes(9, dwOffset, false);
	if (memcmp(pBuffer, "LAME", 4) == 0)
		return new CLAMETag(pStream, bAppended, dwOffset);

	return NULL;
}

CLAMETag::CLAMETag(CMPAStream* pStream, bool bAppended, DWORD dwOffset) :
	CTag(pStream, _T("LAME"), bAppended, dwOffset)
{
	BYTE* pBuffer = pStream->ReadBytes(20, dwOffset, false);

	CString strVersion = CString((char*)pBuffer+4, 4);
#ifndef VALVE_CUSTOM_MPA_SOURCE
	m_fVersion = (float)_tstof(strVersion);
#else
	m_fVersion = (float)_tstof(strVersion.c_str());
#endif
	
	// LAME prior to 3.90 writes only a 20 byte encoder string
	if (m_fVersion < 3.90)
	{
		m_bSimpleTag = true;
		m_strEncoder = CString((char*)pBuffer, 20);
	}
	else
	{
		m_bSimpleTag = false;
		m_strEncoder = CString((char*)pBuffer, 9);
		dwOffset += 9;

		// cut off last period
		if (m_strEncoder[8] == '.')
#ifndef VALVE_CUSTOM_MPA_SOURCE
			m_strEncoder.Delete(8);
#else
			m_strEncoder.erase(8);
#endif

		// version information
		BYTE bInfoAndVBR = *(pStream->ReadBytes(1, dwOffset));

		// revision info in 4 MSB
		m_bRevision = bInfoAndVBR & 0xF0;
		// invalid value
		if (m_bRevision == 15)
			throw NULL;

		// VBR info in 4 LSB
		m_bVBRInfo = bInfoAndVBR & 0x0F;

		// lowpass information
		m_dwLowpassFilterHz = *(pStream->ReadBytes(1, dwOffset)) * 100;

		// skip replay gain values
		dwOffset += 8;

		// skip encoding flags
		dwOffset += 1;

		// average bitrate for ABR, bitrate for CBR and minimal bitrat for VBR [in kbps]
		// 255 means 255 kbps or more
		m_bBitrate = *(pStream->ReadBytes(1, dwOffset)); 
	}
}

CLAMETag::~CLAMETag(void)
{
}

bool CLAMETag::IsVBR() const
{
	if (m_bVBRInfo >= 3 && m_bVBRInfo <= 6)
		return true;
	return false;
}

bool CLAMETag::IsABR() const
{
	if (m_bVBRInfo == 2 || m_bVBRInfo == 9)
		return true;
	return false;
}

bool CLAMETag::IsCBR() const
{
	if (m_bVBRInfo == 1 || m_bVBRInfo == 8)
		return true;
	return false;
}

LPCTSTR CLAMETag::GetVBRInfo() const
{
	if (m_bVBRInfo > 9)
		return m_szVBRInfo[0];

	return m_szVBRInfo[m_bVBRInfo];
}