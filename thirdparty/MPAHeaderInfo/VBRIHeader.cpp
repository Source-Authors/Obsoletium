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

#include "VBRIHeader.h"

#include "MPAFrame.h"
#include "MPAHeader.h"

CVBRIHeader* CVBRIHeader::FindHeader(const CMPAFrame* frame) {
  // VBRI header always at fixed offset
  const unsigned offset{frame->m_dwOffset + MPA_HEADER_SIZE + 32U};

  // VBRI ID found?
  if (!CheckID(frame->m_pStream, offset, 'V', 'B', 'R', 'I')) return nullptr;

  return new CVBRIHeader{frame, offset};
}

CVBRIHeader::CVBRIHeader(const CMPAFrame* frame, unsigned offset)
    : CVBRHeader{frame->m_pStream, offset} {
  /* FhG VBRI Header

  size	description
  4		'VBRI' (ID)
  2		version
  2		delay
  2		quality
  4		# bytes
  4		# frames
  2		table size (for TOC)
  2		table scale (for TOC)
  2		size of table entry (max. size = 4 byte (must be stored in an
  integer)) 2		frames per table entry

  ??		dynamic table consisting out of frames with size 1-4
                  whole length in table size! (for TOC)

  */

  // ID is already checked at this point
  offset += 4U;

  // extract all fields from header (all mandatory)
  m_dwVersion = m_pStream->ReadBEValue(2, offset);
  m_fDelay = (float)m_pStream->ReadBEValue(2, offset);
  m_dwQuality = m_pStream->ReadBEValue(2, offset);
  m_dwBytes = m_pStream->ReadBEValue(4, offset);
  m_dwFrames = m_pStream->ReadBEValue(4, offset);
  m_dwTableSize = m_pStream->ReadBEValue(2, offset) + 1;  //!!!
  m_dwTableScale = m_pStream->ReadBEValue(2, offset);
  m_dwBytesPerEntry = m_pStream->ReadBEValue(2, offset);
  m_dwFramesPerEntry = m_pStream->ReadBEValue(2, offset);

  // extract TOC  (for more accurate seeking)
  m_pnToc = new int[m_dwTableSize];
  for (unsigned int i = 0; i < m_dwTableSize; i++) {
    m_pnToc[i] = m_pStream->ReadBEValue(m_dwBytesPerEntry, offset);
  }

  // get length of file (needed for seeking)
  m_dwLengthSec = frame->m_pHeader->GetLengthSecond(m_dwFrames);
}

CVBRIHeader::~CVBRIHeader() { delete[] m_pnToc; }

unsigned CVBRIHeader::SeekPosition(float& percent) const {
  return SeekPositionByTime(percent / 100.0f * m_dwLengthSec * 1000.0f);
}

unsigned CVBRIHeader::SeekPositionByTime(float entry_time_ms) const {
  unsigned int i = 0;

  float accumulated_time_ms{0.0f};
  const float length_ms = static_cast<float>(m_dwLengthSec) * 1000.0f;
  const float length_ms_per_toc_entry =
      length_ms / static_cast<float>(m_dwTableSize);

  if (entry_time_ms > length_ms) entry_time_ms = length_ms;

  unsigned seek_point = 0;
  while (accumulated_time_ms <= entry_time_ms) {
    seek_point += m_pnToc[i++];
    accumulated_time_ms += length_ms_per_toc_entry;
  }

  // Searched too far; correct result
  const unsigned fraction = static_cast<unsigned>(
      (((accumulated_time_ms - entry_time_ms) / length_ms_per_toc_entry) +
       (1.0f / (2.0f * static_cast<float>(m_dwFramesPerEntry)))) *
      static_cast<float>(m_dwFramesPerEntry));

  seek_point -= static_cast<unsigned>(static_cast<float>(m_pnToc[i - 1]) *
                                      static_cast<float>(fraction) /
                                      static_cast<float>(m_dwFramesPerEntry));

  return seek_point;
}
