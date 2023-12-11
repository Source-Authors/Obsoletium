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

#include "ID3V1Tag.h"

CID3V1Tag* CID3V1Tag::FindTag(CMPAStream* stream, bool is_appended, unsigned,
                              unsigned end) {
  assert(stream);

  if (is_appended) {
    // stands 128 byte before file end
    unsigned offset{end - 128U};
    const unsigned char* buffer{stream->ReadBytes(3U, offset, false)};

    if (memcmp("TAG", buffer, 3U) == 0) return new CID3V1Tag{stream, offset};
  }

  return nullptr;
}

CID3V1Tag::CID3V1Tag(CMPAStream* stream, unsigned offset)
    : CTag{stream, _T("ID3"), true, offset, 128U} {
  offset += 125U;

  const unsigned char* buffer{stream->ReadBytes(2U, offset, false)};
  // check if V1.1 tag (byte 125 = 0 and byte 126 != 0)
  const bool bIsV11 = buffer[0] == '\0' && buffer[1] != '\0';

  SetVersion(1U, bIsV11 ? 1U : 0U);
}

CID3V1Tag::~CID3V1Tag() {}
