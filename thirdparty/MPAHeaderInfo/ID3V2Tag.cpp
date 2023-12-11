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

#include "ID3V2Tag.h"

CID3V2Tag* CID3V2Tag::FindTag(CMPAStream* stream, bool is_appended,
                              unsigned begin, unsigned end) {
  assert(stream);

  const char* id;
  unsigned offset;

  if (!is_appended) {
    // stands at the beginning of file (complete header is 10 bytes)
    offset = begin;
    id = "ID3";
  } else {
    // stands at the end of the file (complete footer is 10 bytes)
    offset = end - 10U;
    id = "3DI";
  }

  const unsigned char* buffer{stream->ReadBytes(10U, offset, false)};

  if (memcmp(id, buffer, 3U) == 0)
    return new CID3V2Tag{stream, is_appended, offset};

  return nullptr;
}

CID3V2Tag::CID3V2Tag(CMPAStream* stream, bool is_appended, unsigned offset)
    : CTag{stream, _T("ID3"), false, offset} {
  offset += 3U;

  // read out version info
  const unsigned char* buffer{m_pStream->ReadBytes(3, offset)};
  SetVersion(2, buffer[0], buffer[1]);

  const unsigned char flags{buffer[3]};
  /*m_bUnsynchronization = (bFlags & 0x80)?true:false;	// bit 7
  m_bExtHeader = (bFlags & 0x40)?true:false;			// bit 6
  m_bExperimental = (bFlags & 0x20)?true:false;		// bit 5*/
  const bool is_footer{(flags & 0x10) ? true : false};  // bit 4

  // convert synchsafe integer
  m_dwSize = GetSynchsafeInteger(m_pStream->ReadBEValue(4U, offset)) +
             (is_footer ? 20U : 10U);

  // for appended tag: calculate new offset
  if (is_appended) m_dwOffset -= m_dwSize - 10U;
}

// return for each byte only lowest 7 bits (highest bit always zero)
unsigned CID3V2Tag::GetSynchsafeInteger(unsigned value) {
  // masks first 7 bits
  constexpr unsigned mask{0x7F000000U};

  unsigned result{0U};

  for (int n = 0; n < sizeof(unsigned); n++) {
    result = (result << 7U) | ((value & mask) >> 24U);
    value <<= 8U;
  }

  return result;
}

CID3V2Tag::~CID3V2Tag() {}
