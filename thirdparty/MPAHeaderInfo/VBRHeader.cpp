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

#include "MPAHeader.h"

#include <algorithm>

#include "XINGHeader.h"
#include "VBRIHeader.h"

// first test with this static method, if it does exist
CVBRHeader* CVBRHeader::FindHeader(const CMPAFrame* frame) {
  assert(frame);

  CVBRHeader* header = CXINGHeader::FindHeader(frame);
  if (!header) header = CVBRIHeader::FindHeader(frame);

  return header;
}

CVBRHeader::CVBRHeader(CMPAStream* stream, unsigned offset)
    : m_pStream(stream),
      m_pnToc(nullptr),
      m_dwOffset(offset),
      m_dwFrames(0),
      m_dwBytes(0),
      m_dwQuality(0),
      m_dwTableSize(0) {}

bool CVBRHeader::CheckID(CMPAStream* stream, unsigned offset, char ch0,
                         char ch1, char ch2, char ch3) {
  const unsigned char* buffer{stream->ReadBytes(4U, offset, false)};

  if (buffer[0] == ch0 && buffer[1] == ch1 && buffer[2] == ch2 &&
      buffer[3] == ch3)
    return true;

  return false;
}

/*
// currently not used
bool CVBRHeader::ExtractLAMETag( unsigned offset )
{
        // LAME ID found?
        if( !CheckID( m_pMPAFile, offset, 'L', 'A', 'M', 'E' ) && !CheckID(
m_pMPAFile, offset, 'G', 'O', 'G', 'O' ) ) return false;

        return true;
}*/

CVBRHeader::~CVBRHeader() { delete[] m_pnToc; }

// get byte position for percentage value (percent) of file
bool CVBRHeader::SeekPosition(float& percent, unsigned& seek_point) const {
  if (!m_pnToc || m_dwBytes == 0) return false;

  // check range of percent
  if (percent < 0.0f) percent = 0.0f;
  if (percent > 99.0f) percent = 99.0f;

  seek_point = SeekPosition(percent);
  return true;
}
