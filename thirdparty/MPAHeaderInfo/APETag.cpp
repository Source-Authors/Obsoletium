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

#include "APETag.h"

CAPETag* CAPETag::FindTag(CMPAStream* stream, bool is_appended,
                          unsigned begin_offset, unsigned end_offset) {
  assert(stream);

  unsigned offset;

  if (!is_appended) {
    // stands at the beginning of file (complete header is 32 bytes)
    offset = begin_offset;
  } else {
    // stands at the end of the file (complete footer is 32 bytes)
    offset = end_offset - 32U;
  }

  const unsigned char* buffer{stream->ReadBytes(8U, offset, false)};

  if (memcmp("APETAGEX", buffer, 8U) == 0)
    return new CAPETag{stream, is_appended, offset};

  return nullptr;
}

CAPETag::CAPETag(CMPAStream* stream, bool is_appended, unsigned offset)
    : CTag{stream, _T("APE"), is_appended, offset} {
  offset += 8U;

  const unsigned version{stream->ReadLEValue(4U, offset)};

  // first byte is version number
  m_fVersion = version / 1000.0f;

  // get size
  m_dwSize = stream->ReadLEValue(4, offset);

  /*unsigned dwNumItems = */ stream->ReadLEValue(4U, offset);

  // only valid for version 2
  const unsigned flag{stream->ReadLEValue(4U, offset)};

  bool is_header, is_footer;

  if (m_fVersion > 1.0f) {
    is_header = flag >> 31U & 0x1U;
    is_footer = flag >> 30U & 0x1U;
  } else {
    is_header = false;
    is_footer = true;
  }

  if (is_header) m_dwSize += 32U;  // add header
  if (is_appended) m_dwOffset -= (m_dwSize - 32);
}

CAPETag::~CAPETag() {}
