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

#include "XINGHeader.h"

#include "LameTag.h"
#include "MPAFrame.h"

// for XING VBR Header flags
constexpr unsigned FRAMES_FLAG{0x0001U};
constexpr unsigned BYTES_FLAG{0x0002U};
constexpr unsigned TOC_FLAG{0x0004U};
constexpr unsigned VBR_SCALE_FLAG{0x0008U};

// offset of XING header: after side information in Layer III

CXINGHeader* CXINGHeader::FindHeader(const CMPAFrame* frame) {
  assert(frame);

  // where does VBR header begin (XING)
  const unsigned offset{frame->m_dwOffset + MPA_HEADER_SIZE +
                        frame->m_pHeader->GetSideInfoSize()};
  // + (frame->m_pHeader->m_bCRC?2:0);

  // check for XING header first
  if (!CheckID(frame->m_pStream, offset, 'X', 'i', 'n', 'g') &&
      !CheckID(frame->m_pStream, offset, 'I', 'n', 'f', 'o'))
    return nullptr;

  return new CXINGHeader{frame, offset};
}

CXINGHeader::CXINGHeader(const CMPAFrame* frame, unsigned offset)
    : CVBRHeader{frame->m_pStream, offset} {
  /* XING VBR-Header

   size	description
   4		'Xing' or 'Info'
   4		flags (indicates which fields are used)
   4		frames (optional)
   4		bytes (optional)
   100	toc (optional)
   4		a VBR quality indicator: 0=best 100=worst (optional)

  */

  // XING ID already checked at this point
  offset += 4U;

  // get flags (mandatory in XING header)
  const unsigned flags{m_pStream->ReadBEValue(4U, offset)};

  // extract total number of frames in file
  if (flags & FRAMES_FLAG) m_dwFrames = m_pStream->ReadBEValue(4U, offset);

  // extract total number of bytes in file
  if (flags & BYTES_FLAG) m_dwBytes = m_pStream->ReadBEValue(4U, offset);

  // extract TOC (for more accurate seeking)
  if (flags & TOC_FLAG) {
    m_dwTableSize = 100U;
    m_pnToc = new int[m_dwTableSize];

    for (unsigned i = 0; i < m_dwTableSize; i++)
      m_pnToc[i] = *m_pStream->ReadBytes(1, offset);
  }

  if (flags & VBR_SCALE_FLAG) m_dwQuality = m_pStream->ReadBEValue(4U, offset);

  m_pLAMETag = CLAMETag::FindTag(m_pStream, true, m_dwOffset, 0U);
}

CXINGHeader::~CXINGHeader() { delete m_pLAMETag; }

unsigned CXINGHeader::SeekPosition(float& percent) const {
  // interpolate in TOC to get file seek point in bytes
  const int a = static_cast<int>(percent);

  float fb;
  if (a < 99) {
    fb = static_cast<float>(m_pnToc[a + 1]);
  } else {
    fb = 256.0f;
  }

  float fa = static_cast<float>(m_pnToc[a]);
  const float fx{fa + (fb - fa) * (percent - a)};

  return static_cast<int>(1.0f / 256.0f * fx * m_dwBytes);
}
