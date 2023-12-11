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

#include "MPAFrame.h"

#include <cmath>  // for ceil

#include "MPAEndOfFileException.h"

#include <Windows.h>

// number of bits that are used for CRC check in MPEG 1 Layer 2
// (this table is invalid for Joint Stereo/Intensity Stereo)
// 1. index = allocation table index, 2. index = mono
const unsigned CMPAFrame::m_dwProtectedBitsLayer2[5][2] = {
    {284, 142},  // table a
    {308, 154},  // table b
    {42, 84},    // table c
    {62, 114},   // table d
    {270, 135}   // table for MPEG 2/2.5
};

CMPAFrame::CMPAFrame(CMPAStream* stream, unsigned& offset,
                     bool should_find_subsequent_frame, bool is_exact_offset,
                     bool should_reverse, CMPAHeader* compare_header)
    : m_pStream(stream), m_dwOffset(offset), m_bIsLast(false) {
  // decode header of frame at position offset
  m_pHeader = new CMPAHeader(stream, m_dwOffset, is_exact_offset,
                             should_reverse, compare_header);
  m_dwFrameSize = m_pHeader->CalcFrameSize();

  // do extended check?
  if (should_find_subsequent_frame) {
    // calc offset for next frame header
    unsigned new_offset{GetSubsequentHeaderOffset()};

    /* if no subsequent frame found
            - end of file reached (last frame)
            - corruption in file
                    - subsequent frame is at incorrect position (out of
       detection range)
                    - frame was incorrectly detected (very improbable)
    */
    try {
      CMPAFrame subsequentFrame(stream, new_offset, false, true, false,
                                m_pHeader);
    } catch (CMPAEndOfFileException&) {
      m_bIsLast = true;
    } catch (CMPAException&) {
      OutputDebugString(_T("Didn't find subsequent frame"));
      // if (e->GetErrorID() == CMPAException::NoFrameInTolerance
    }
  }
}

CVBRHeader* CMPAFrame::FindVBRHeader() const {
  // read out VBR header (if available), must be freed with delete
  return CVBRHeader::FindHeader(this);
}

CMPAFrame::~CMPAFrame() { delete m_pHeader; }

// returns true if CRC is correct otherwise false
// do only call this function, if header contains a CRC checksum
bool CMPAFrame::CheckCRC() const {
  if (!m_pHeader->m_bCRC) return false;

  unsigned protected_bits;

  // which bits should be considered for CRC calculation
  switch (m_pHeader->m_Layer) {
    case CMPAHeader::MPALayer::Layer1:
      protected_bits = 4U * (m_pHeader->GetNumChannels() * m_pHeader->m_wBound +
                             (32U - m_pHeader->m_wBound));
      break;
    case CMPAHeader::MPALayer::Layer2:
      // no check for Layer II files (not easy to compute number protected bits,
      // need to get allocation bits first)
      return true;
      break;
    // for Layer III the protected bits are the side information
    case CMPAHeader::MPALayer::Layer3:
      protected_bits = m_pHeader->GetSideInfoSize() * 8U;
    default:
      return true;
  }

  // CRC is also calculated from the last 2 bytes of the header
  protected_bits += MPA_HEADER_SIZE * 8U + 16U;  // +16bit for CRC itself

  const unsigned byte_size{static_cast<unsigned>(ceil(protected_bits / 8.0))};

  // the first 2 bytes and the CRC itself are automatically skipped
  unsigned offset{m_dwOffset};
  const unsigned short crc16{CalcCRC16(
      m_pStream->ReadBytes(byte_size, offset, false), protected_bits)};

  // read out crc from frame (it follows frame header)
  offset += MPA_HEADER_SIZE;

  return crc16 == m_pStream->ReadBEValue(2, offset, false);
}

// CRC16 check
unsigned short CMPAFrame::CalcCRC16(unsigned char* buffer, unsigned bit_size) {
  assert(buffer);

  unsigned short tmp_char{0}, crc_mask{0};
  unsigned short crc{0xFFFFU};  // start with inverted value of 0

  // start with byte 2 of header
  for (unsigned n = 16U; n < bit_size; n++) {
    // skip the 2 bytes of the crc itself
    if (n < 32U || n >= 48U) {
      if (n % 8U == 0) {
        crc_mask = 1U << 8U;
        tmp_char = buffer[n / 8U];
      }

      crc_mask >>= 1;

      unsigned short tmpi{crc & 0x8000U};

      crc <<= 1;

      if (!tmpi ^ !(tmp_char & crc_mask)) crc ^= 0x8005U;
    }
  }

  crc &= 0xFFFFU;  // invert the result
  return crc;
}