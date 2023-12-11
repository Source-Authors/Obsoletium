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

#include "MpaFile.h"

#include "MPAException.h"

#include <Windows.h>

CMPAFile::CMPAFile(LPCTSTR file) {
  m_pStream = new CMPAFileStream(file);
  m_pTags = new CTags(m_pStream);

  unsigned offset{0};
  // find first valid MPEG frame
  m_pFirstFrame = new CMPAFrame(m_pStream, offset, true, false, false, nullptr);

  // check for VBR header
  m_pVBRHeader = m_pFirstFrame->FindVBRHeader();

  CalcBytesPerSec();
}

// destructor
CMPAFile::~CMPAFile() {
  delete m_pVBRHeader;

  delete m_pFirstFrame;
  delete m_pTags;
  delete m_pStream;
}

constexpr unsigned MAX_EMPTY_FRAMES{25U};

// try to guess the bitrate of the whole file as good as possible
void CMPAFile::CalcBytesPerSec() {
  // in case of a VBR header we know the bitrate (if at least the number of
  // frames is known)
  if (m_pVBRHeader && m_pVBRHeader->m_dwFrames) {
    const unsigned bytes{m_pVBRHeader->m_dwBytes};

    // number of bytes can be guessed
    if (!bytes) m_pVBRHeader->m_dwBytes = GetFileSize();

    m_dwBytesPerSec = m_pFirstFrame->m_pHeader->GetBytesPerSecond(
        m_pVBRHeader->m_dwFrames, bytes);

    return;
  }

  // otherwise we have to guess it

  // go through all frames that have a lower bitrate than 48kbit
  CMPAFrame* frame{m_pFirstFrame};
  unsigned frames_num{0};
  bool should_delete_frame{false};

  while (frame && frame->m_pHeader->m_dwBitrate <= 48000U) {
    frame = GetFrame(GetType::Next, frame, should_delete_frame);

    if (frames_num++ > MAX_EMPTY_FRAMES) break;

    should_delete_frame = true;
  };

  if (!frame) {
    frame = m_pFirstFrame;
    should_delete_frame = false;
  }

  m_dwBytesPerSec = frame->m_pHeader->GetBytesPerSecond();

  if (should_delete_frame) delete frame;
}

// MPEG2, LayerIII, 8kbps, 24kHz => Framesize = 24 Bytes
constexpr unsigned MIN_FRAME_SIZE{24U};

CMPAFrame* CMPAFile::GetFrame(CMPAFile::GetType Type, CMPAFrame* frame,
                              bool should_delete_old_frame, unsigned offset) {
  CMPAFrame* new_frame;
  CMPAHeader* comparison_header{nullptr};
  bool is_subsequent_frame{true};
  bool should_reverse{false};
  bool is_exact_offset{true};

  switch (Type) {
    case GetType::First:
      offset = GetBegin();
      is_subsequent_frame = true;
      is_exact_offset = false;
      break;
    case GetType::Last:
      offset = GetEnd() - MIN_FRAME_SIZE;
      should_reverse = true;
      is_exact_offset = false;
      comparison_header = m_pFirstFrame->m_pHeader;
      break;
    case GetType::Next:
      if (!frame) return nullptr;

      comparison_header = m_pFirstFrame->m_pHeader;
      offset = frame->GetSubsequentHeaderOffset();
      break;
    case GetType::Prev:
      if (!frame) return nullptr;

      offset = frame->m_dwOffset - MIN_FRAME_SIZE;
      should_reverse = true;
      is_exact_offset = false;
      comparison_header = m_pFirstFrame->m_pHeader;
      break;
    case GetType::Resync:
      is_subsequent_frame = true;
      is_exact_offset = false;
      // pCompHeader = m_pFrame->m_pHeader;
      break;
    default:
      return nullptr;
  }

  try {
    new_frame =
        new CMPAFrame(m_pStream, offset, is_subsequent_frame, is_exact_offset,
                      should_reverse, comparison_header);

  } catch (CMPAException& e) {
    // try a complete resync from position offset
    if (Type == GetType::Next) {
      return GetFrame(GetType::Resync, frame, should_delete_old_frame, offset);
    }

    OutputDebugString(e.GetErrorDescription());
    new_frame = nullptr;
  }

  if (frame && should_delete_old_frame) delete frame;

  return new_frame;
}
