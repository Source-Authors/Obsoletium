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

#include "Lyrics3Tag.h"

#include "MPAException.h"

CLyrics3Tag* CLyrics3Tag::FindTag(CMPAStream* stream, bool, unsigned,
                                  unsigned end) {
  assert(stream);

  // stands at the end of file
  unsigned offset{end - 9U};
  const unsigned char* buffer{stream->ReadBytes(9U, offset, false, true)};

  // is it Lyrics 2 Tag
  if (memcmp("LYRICS200", buffer, 9U) == 0)
    return new CLyrics3Tag{stream, offset, true};

  if (memcmp("LYRICSEND", buffer, 9U) == 0)
    return new CLyrics3Tag{stream, offset, false};

  return nullptr;
}

CLyrics3Tag::CLyrics3Tag(CMPAStream* stream, unsigned offset, bool version_2)
    : CTag{stream, _T("Lyrics3"), true, offset} {
  if (version_2) {
    SetVersion(2);

    // look for size of tag (stands before offset)
    offset -= 6U;
    const unsigned char* buffer{stream->ReadBytes(6U, offset, false)};

    // add null termination
    char size[7];
    memcpy(size, buffer, 6U);
    size[6] = '\0';

    // convert string to integer
    m_dwSize = atoi(size);
    m_dwOffset = offset - m_dwSize;
    m_dwSize += 6U + 9U;  // size must include size info and end string
  } else {
    SetVersion(1);

    // seek back 5100 bytes and look for LYRICSBEGIN
    m_dwOffset -= 5100U;

    const unsigned char* buffer{stream->ReadBytes(11U, m_dwOffset, false)};

    while (memcmp("LYRICSBEGIN", buffer, 11U) != 0) {
      if (offset >= m_dwOffset)
        throw CMPAException{CMPAException::ErrorIDs::CorruptLyricsTag};
    }

    m_dwSize = offset - m_dwOffset + 9U;
  }
}

CLyrics3Tag::~CLyrics3Tag() {}
