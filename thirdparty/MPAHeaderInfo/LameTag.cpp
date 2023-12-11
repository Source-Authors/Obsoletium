// GNU LESSER GENERAL PUBLIC LICENSE
// Version 3, 29 June 2007
//
// Copyright (C) 2007 Free Software Foundation, Inc. <http://fsf.org/>
//
// Everyone is permitted to copy and distribute verbatim copies of this license
// document, but changing it is not allowed.
//
// This version of the GNU Lesser General Public License incorporates the terms
// and conditions of version 3 of the GNU General Public License, supplemented
// by the additional permissions listed below.

#include "stdafx.h"

#include "LameTag.h"

LPCTSTR CLAMETag::m_szVBRInfo[10] = {
    _T("Unknown"), _T("CBR"),  _T("ABR"),      _T("VBR1"),     _T("VBR2"),
    _T("VBR3"),    _T("VBR4"), _T("Reserved"), _T("CBR2Pass"), _T("ABR2Pass")};

CLAMETag* CLAMETag::FindTag(CMPAStream* stream, bool is_appended,
                            unsigned begin, unsigned) {
  assert(stream);

  // check for LAME Tag extension (always 120 bytes after XING ID)
  unsigned offset{begin + 120U};
  const unsigned char* buffer{stream->ReadBytes(9, offset, false)};

  if (memcmp(buffer, "LAME", 4U) == 0)
    return new CLAMETag{stream, is_appended, offset};

  return nullptr;
}

CLAMETag::CLAMETag(CMPAStream* stream, bool is_appended, unsigned offset)
    : CTag{stream, _T("LAME"), is_appended, offset} {
  const unsigned char* buffer{stream->ReadBytes(20, offset, false)};

  CString strVersion = CString{reinterpret_cast<const char*>(buffer) + 4, 4};
#ifndef MPA_USE_STRING_FOR_CSTRING
  m_fVersion = (float)_tstof(strVersion);
#else
  m_fVersion = (float)_tstof(strVersion.c_str());
#endif

  // LAME prior to 3.90 writes only a 20 byte encoder string
  if (m_fVersion < 3.90f) {
    m_bSimpleTag = true;
    m_strEncoder = CString{reinterpret_cast<const char*>(buffer), 20U};

    m_bRevision = 0;
    m_bVBRInfo = 0;
    m_dwLowpassFilterHz = 0;
    m_bBitrate = 0;
  } else {
    m_bSimpleTag = false;
    m_strEncoder = CString{reinterpret_cast<const char*>(buffer), 9U};
    offset += 9U;

    // cut off last period
    if (m_strEncoder[8] == '.')
#ifndef MPA_USE_STRING_FOR_CSTRING
      m_strEncoder.Delete(8);
#else
      m_strEncoder.erase(8);
#endif

    // version information
    const unsigned char info_and_vbr{*(stream->ReadBytes(1U, offset))};

    // revision info in 4 MSB
    m_bRevision = info_and_vbr & 0xF0;
    // invalid value
    if (m_bRevision == 15U) {
      throw std::exception("Incorrect revision (15)");
    }

    // VBR info in 4 LSB
    m_bVBRInfo = info_and_vbr & 0x0F;

    // lowpass information
    m_dwLowpassFilterHz = *(stream->ReadBytes(1, offset)) * 100U;

    // skip replay gain values
    offset += 8U;

    // skip encoding flags
    offset += 1U;

    // average bitrate for ABR, bitrate for CBR and minimal bitrate for VBR [in
    // kbps] 255 means 255 kbps or more
    m_bBitrate = *(stream->ReadBytes(1, offset));
  }
}

CLAMETag::~CLAMETag() {}

bool CLAMETag::IsVBR() const { return (m_bVBRInfo >= 3U && m_bVBRInfo <= 6U); }

bool CLAMETag::IsABR() const { return (m_bVBRInfo == 2U || m_bVBRInfo == 9U); }

bool CLAMETag::IsCBR() const { return (m_bVBRInfo == 1U || m_bVBRInfo == 8U); }

LPCTSTR CLAMETag::GetVBRInfo() const {
  if (m_bVBRInfo >= std::size(m_szVBRInfo)) return m_szVBRInfo[0];

  return m_szVBRInfo[m_bVBRInfo];
}