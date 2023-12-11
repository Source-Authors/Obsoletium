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
#include "MPAException.h"
#include "MPAEndOfFileException.h"
#include "MPAStream.h"

#include <Windows.h>

// static variables
LPCTSTR CMPAHeader::m_szLayers[] = {_T("Layer I"), _T("Layer II"),
                                    _T("Layer III")};
LPCTSTR CMPAHeader::m_szMPEGVersions[] = {_T("MPEG 2.5"), _T(""), _T("MPEG 2"),
                                          _T("MPEG 1")};
LPCTSTR CMPAHeader::m_szChannelModes[] = {
    _T("Stereo"), _T("Joint Stereo"), _T("Dual Channel"), _T("Single Channel")};
LPCTSTR CMPAHeader::m_szEmphasis[] = {_T("None"), _T("50/15ms"), _T(""),
                                      _T("CCIT J.17")};

// sampling rates in hertz: 1. index = MPEG Version ID, 2. index = sampling rate
// index
const unsigned CMPAHeader::m_dwSamplingRates[4][3] = {
    {
        11025,
        12000,
        8000,
    },  // MPEG 2.5
    {
        0,
        0,
        0,
    },  // reserved
    {
        22050,
        24000,
        16000,
    },                     // MPEG 2
    {44100, 48000, 32000}  // MPEG 1
};

// bitrates: 1. index = LSF, 2. index = Layer, 3. index = bitrate index
const unsigned CMPAHeader::m_dwBitrates[2][3][15] = {{
                                                         // MPEG 1
                                                         {
                                                             0,
                                                             32,
                                                             64,
                                                             96,
                                                             128,
                                                             160,
                                                             192,
                                                             224,
                                                             256,
                                                             288,
                                                             320,
                                                             352,
                                                             384,
                                                             416,
                                                             448,
                                                         },  // Layer1
                                                         {
                                                             0,
                                                             32,
                                                             48,
                                                             56,
                                                             64,
                                                             80,
                                                             96,
                                                             112,
                                                             128,
                                                             160,
                                                             192,
                                                             224,
                                                             256,
                                                             320,
                                                             384,
                                                         },  // Layer2
                                                         {
                                                             0,
                                                             32,
                                                             40,
                                                             48,
                                                             56,
                                                             64,
                                                             80,
                                                             96,
                                                             112,
                                                             128,
                                                             160,
                                                             192,
                                                             224,
                                                             256,
                                                             320,
                                                         }  // Layer3
                                                     },
                                                     {
                                                         // MPEG 2, 2.5
                                                         {
                                                             0,
                                                             32,
                                                             48,
                                                             56,
                                                             64,
                                                             80,
                                                             96,
                                                             112,
                                                             128,
                                                             144,
                                                             160,
                                                             176,
                                                             192,
                                                             224,
                                                             256,
                                                         },  // Layer1
                                                         {
                                                             0,
                                                             8,
                                                             16,
                                                             24,
                                                             32,
                                                             40,
                                                             48,
                                                             56,
                                                             64,
                                                             80,
                                                             96,
                                                             112,
                                                             128,
                                                             144,
                                                             160,
                                                         },  // Layer2
                                                         {
                                                             0,
                                                             8,
                                                             16,
                                                             24,
                                                             32,
                                                             40,
                                                             48,
                                                             56,
                                                             64,
                                                             80,
                                                             96,
                                                             112,
                                                             128,
                                                             144,
                                                             160,
                                                         }  // Layer3
                                                     }};

// allowed combination of bitrate (1.index) and mono (2.index)
const bool CMPAHeader::m_bAllowedModes[15][2] = {
    // {stereo, intensity stereo, dual channel allowed,single channel allowed}
    {true, true},   // free mode
    {false, true},  // 32
    {false, true},  // 48
    {false, true},  // 56
    {true, true},   // 64
    {false, true},  // 80
    {true, true},   // 96
    {true, true},   // 112
    {true, true},   // 128
    {true, true},   // 160
    {true, true},   // 192
    {true, false},  // 224
    {true, false},  // 256
    {true, false},  // 320
    {true, false}   // 384
};

// Samples per Frame: 1. index = LSF, 2. index = Layer
const unsigned CMPAHeader::m_dwSamplesPerFrames[2][3] = {{
                                                             // MPEG 1
                                                             384,   // Layer1
                                                             1152,  // Layer2
                                                             1152   // Layer3
                                                         },
                                                         {
                                                             // MPEG 2, 2.5
                                                             384,   // Layer1
                                                             1152,  // Layer2
                                                             576    // Layer3
                                                         }};

// Samples per Frame / 8
const unsigned CMPAHeader::m_dwCoefficients[2][3] = {
    {
        // MPEG 1
        12,   // Layer1	(must be multiplied with 4, because of slot size)
        144,  // Layer2
        144   // Layer3
    },
    {
        // MPEG 2, 2.5
        12,   // Layer1	(must be multiplied with 4, because of slot size)
        144,  // Layer2
        72    // Layer3
    }};

// slot size per layer
const unsigned CMPAHeader::m_dwSlotSizes[3] = {
    4,  // Layer1
    1,  // Layer2
    1   // Layer3
};

// size of side information (only for Layer III)
// 1. index = LSF, 2. index = mono
const unsigned CMPAHeader::m_dwSideInfoSizes[2][2] = {
    // MPEG 1
    {32, 17},
    // MPEG 2/2.5
    {17, 9}};

// tolerance range, look at expected offset +/- m_dwTolerance/2 for beginning of
// a frame
const unsigned CMPAHeader::m_dwTolerance = 6U;  // +/-3 bytes

// max. range where to look for frame sync
const unsigned CMPAHeader::m_dwMaxRange = 16384U;

// constructor (throws exception if header is invalid)
// if bExactOffset = true then look for frame at offset +/- tolerance, otherwise
// start looking at offset and go through file
CMPAHeader::CMPAHeader(CMPAStream* stream, unsigned& offset,
                       bool is_exact_offset, bool should_reverse,
                       CMPAHeader* compare_header)
    : m_wAllocationTableIndex(0), m_wBound(32) {
  assert(stream);

  // look for synchronisation
  unsigned step = 1U;

  // is new offset within valid range?
  bool is_header_found = false;
  while (unsigned char* header =
             stream->ReadBytes(4U, offset, false, should_reverse)) {
    // sync bytes found?
    // for performance reasons check already that it is not data within an empty
    // frame (all bits set) therefore check wether the bits for bitrate are all
    // set -> means that this is no header!
    if (header[0] == 0xFF && ((header[1] & 0xE0) == 0xE0) &&
        ((header[2] & 0xF0) != 0xF0))  // first 11 bits should be 1

    {
      try {
        Init(header, stream->GetFilename());

        if (compare_header) {
          // is this header compatible (which means that it resembles the
          // previous header
          if (!(*this == *compare_header))
            throw CMPAException(CMPAException::ErrorIDs::IncompatibleHeader,
                                stream->GetFilename());
        }

        is_header_found = true;
        break;
      }
      /*
      An Exception either means, that a corrupt header was found or
      that there is no header at the position offset (but there were
      incidentally the sync bytes found). The distinction between these to
      errors is made upon the value of bExactOffset
      */
      catch (const CMPAException&) {
        OutputDebugString(_T("Exception at construction of MPAHeader."));

        if (is_exact_offset) throw;
      }
    }

    // find the frame on a position greater or smaller than the current offset
    if (!is_exact_offset) {
      // just go forward or backward to find sync
      offset += (should_reverse ? -1 : +1);

      // check only within dwMaxRange
      if (step > m_dwMaxRange)
        // out of tolerance range
        throw CMPAException(CMPAException::ErrorIDs::NoFrameInTolerance,
                            stream->GetFilename());

      step++;
    }
    // find the frame at the exact position (or within the tolerance around the
    // position)
    else {
      if (step > m_dwTolerance) {
        // out of tolerance range
        throw CMPAException(CMPAException::ErrorIDs::NoFrameInTolerance,
                            stream->GetFilename());
      }

      // look around dwExpectedOffset with increasing steps (+1,-2,+3,-4,...)
      if (step % 2 == 1)
        offset += step;
      else
        offset -= step;
      step++;
    }
  }

  if (!is_header_found) {
    // out of tolerance range
    throw CMPAEndOfFileException(stream->GetFilename());
  }
}

// the bit information refers to bit 0 as the most significant bit (MSB) of Byte
// 0 decodes the header in pHeader
void CMPAHeader::Init(unsigned char* header, LPCTSTR file_name) {
  assert(header);
  assert(file_name);

  // get MPEG version [bit 11,12]
  m_Version =
      (MPAVersion)((header[1] >> 3) & 0x03U);  // mask only the rightmost 2 bits

  if (m_Version == MPAVersion::MPEGReserved)
    throw CMPAException(CMPAException::ErrorIDs::HeaderCorrupt, file_name);

  m_bLSF = m_Version != MPAVersion::MPEG1;

  // get layer (0 = layer1, 2 = layer2, ...)  [bit 13,14]
  m_Layer = (MPALayer)(3 - ((header[1] >> 1) & 0x03U));
  if (m_Layer == MPALayer::LayerReserved)
    throw CMPAException(CMPAException::ErrorIDs::HeaderCorrupt, file_name);

  // protection bit (inverted) [bit 15]
  m_bCRC = !((header[1]) & 0x01);

  // bitrate [bit 16..19]
  unsigned char bBitrateIndex = (unsigned char)((header[2] >> 4) & 0x0F);
  if (bBitrateIndex == 0x0F)  // all bits set is reserved
    throw CMPAException(CMPAException::ErrorIDs::HeaderCorrupt);

  m_dwBitrate = m_dwBitrates[m_bLSF][static_cast<int>(m_Layer)][bBitrateIndex] *
                1000U;  // convert from kbit to bit

  if (m_dwBitrate == 0)  // means free bitrate (is unsupported yet)
    throw CMPAException(CMPAException::ErrorIDs::FreeBitrate, file_name);

  // sampling rate [bit 20,21]
  unsigned char bIndex = (unsigned char)((header[2] >> 2U) & 0x03U);

  if (bIndex == 0x03U)  // all bits set is reserved
    throw CMPAException(CMPAException::ErrorIDs::HeaderCorrupt, file_name);

  m_dwSamplesPerSec = m_dwSamplingRates[static_cast<int>(m_Version)][bIndex];

  // padding bit [bit 22]
  m_dwPaddingSize = 1 * ((header[2] >> 1) & 0x01U);  // in Slots (always 1)

  m_dwSamplesPerFrame = m_dwSamplesPerFrames[m_bLSF][static_cast<int>(m_Layer)];

  // private bit [bit 23]
  m_bPrivate = (header[2]) & 0x01U;

  // channel mode [bit 24,25]
  m_ChannelMode = static_cast<ChannelMode>((header[3] >> 6) & 0x03U);

  // mode extension [bit 26,27]
  m_ModeExt = (unsigned char)((header[3] >> 4) & 0x03U);

  // determine the bound for intensity stereo
  if (m_ChannelMode == ChannelMode::JointStereo) m_wBound = 4U + m_ModeExt * 4U;

  // copyright bit [bit 28]
  m_bCopyright = (header[3] >> 3) & 0x01U;

  // original bit [bit 29]
  m_bOriginal = (header[3] >> 2) & 0x01U;

  // emphasis [bit 30,31]
  m_Emphasis = (Emphasis)((header[3]) & 0x03U);
  if (m_Emphasis == Emphasis::EmphReserved)
    throw CMPAException(CMPAException::ErrorIDs::HeaderCorrupt, file_name);

  // extended check for Layer II
  if (m_Layer == MPALayer::Layer2) {
    // MPEG 1
    if (m_Version == MPAVersion::MPEG1) {
      if (!m_bAllowedModes[bBitrateIndex][IsMono()])
        throw CMPAException(CMPAException::ErrorIDs::HeaderCorrupt, file_name);

      // which allocation table is used
      switch (m_dwBitrate / 1000 / (IsMono() ? 1 : 2)) {
        case 32:
        case 48:
          if (m_dwSamplesPerSec == 32000U)
            m_wAllocationTableIndex = 3;  // table d
          else
            m_wAllocationTableIndex = 2;  // table c
          break;
        case 56:
        case 64:
        case 80:
          if (m_dwSamplesPerSec != 48000U) {
            m_wAllocationTableIndex = 0;  // table a
            break;
          }
        case 96:
        case 112:
        case 128:
        case 160:
        case 192:
          if (m_dwSamplesPerSec != 48000U) {
            m_wAllocationTableIndex = 1;  // table b
            break;
          } else
            m_wAllocationTableIndex = 0;  // table a
          break;
      }
    } else  // MPEG 2/2.5
      m_wAllocationTableIndex = 4U;
  }
}

// compare headers
// return true if identical or related
// return false if no similarities

bool CMPAHeader::operator==(CMPAHeader& DestHeader) const {
  // version change never possible
  if (DestHeader.m_Version != m_Version) return false;

  // layer change never possible
  if (DestHeader.m_Layer != m_Layer) return false;

  // sampling rate change never possible
  if (DestHeader.m_dwSamplesPerSec != m_dwSamplesPerSec) return false;

  // from mono to stereo never possible
  if (DestHeader.IsMono() != IsMono()) return false;

  if (DestHeader.m_Emphasis != m_Emphasis) return false;

  return true;
}