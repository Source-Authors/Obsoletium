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

#ifndef MPA_HEADER_INFO_MPA_HEADER_H_
#define MPA_HEADER_INFO_MPA_HEADER_H_

#include "MPAStream.h"

constexpr unsigned MPA_HEADER_SIZE{4};  // MPEG-Audio Header Size 32bit
constexpr unsigned MAXTIMESREAD{5};

class CMPAHeader {
 public:
  CMPAHeader(CMPAStream* stream, unsigned& offset, bool bExactOffset,
             bool bReverse, CMPAHeader* pCompareHeader);
  ~CMPAHeader() = default;

  [[nodiscard]] bool operator==(CMPAHeader& DestHeader) const;
  [[nodiscard]] bool operator!=(CMPAHeader& DestHeader) const {
    return !(*this == DestHeader);
  }

  [[nodiscard]] unsigned CalcFrameSize() const {
    return int(((m_dwCoefficients[m_bLSF][static_cast<int>(m_Layer)] * m_dwBitrate /
                 m_dwSamplesPerSec) +
                m_dwPaddingSize)) *
           m_dwSlotSizes[static_cast<int>(m_Layer)];
  };

  // bitrate is in bit per second, to calculate in bytes => (/ 8)
  [[nodiscard]] unsigned GetBytesPerSecond() const { return m_dwBitrate / 8; };
  // calc number of seconds from number of frames
  [[nodiscard]] unsigned GetLengthSecond(unsigned dwNumFrames) const {
    return dwNumFrames * m_dwSamplesPerFrame / m_dwSamplesPerSec;
  };
  [[nodiscard]] unsigned GetBytesPerSecond(unsigned dwNumFrames,
                                           unsigned dwNumBytes) const {
    return dwNumBytes / GetLengthSecond(dwNumFrames);
  };
  [[nodiscard]] bool IsMono() const { return m_ChannelMode == ChannelMode::SingleChannel; };
  // true if MPEG2/2.5 otherwise false
  [[nodiscard]] bool IsLSF() const { return m_bLSF; };
  [[nodiscard]] unsigned GetSideInfoSize() const {
    return m_dwSideInfoSizes[IsLSF()][IsMono()];
  };
  [[nodiscard]] unsigned GetNumChannels() const { return IsMono() ? 1 : 2; };

 private:
  static const unsigned m_dwMaxRange;
  static const unsigned m_dwTolerance;

  static const unsigned m_dwSamplingRates[4][3];
  static const unsigned m_dwPaddingSizes[3];
  static const unsigned m_dwBitrates[2][3][15];
  static const bool m_bAllowedModes[15][2];
  static const unsigned m_dwSamplesPerFrames[2][3];
  static const unsigned m_dwCoefficients[2][3];
  static const unsigned m_dwSlotSizes[3];
  static const unsigned m_dwSideInfoSizes[2][2];

  bool m_bLSF;  // true means lower sampling frequencies (=MPEG2/MPEG2.5)

  void Init(unsigned char* pHeader, LPCTSTR file_name);

 public:
  static LPCTSTR m_szLayers[];
  static LPCTSTR m_szMPEGVersions[];
  static LPCTSTR m_szChannelModes[];
  static LPCTSTR m_szEmphasis[];

  enum class MPAVersion { MPEG25 = 0, MPEGReserved, MPEG2, MPEG1 };
  MPAVersion m_Version;

  enum class MPALayer { Layer1, Layer2, Layer3, LayerReserved };
  MPALayer m_Layer;

  enum class Emphasis { EmphNone = 0, Emph5015, EmphReserved, EmphCCITJ17 };
  Emphasis m_Emphasis;

  enum class ChannelMode { Stereo, JointStereo, DualChannel, SingleChannel };
  ChannelMode m_ChannelMode;

  unsigned m_dwSamplesPerSec;
  unsigned m_dwSamplesPerFrame;
  unsigned m_dwBitrate;  // in bit per second (1 kb = 1000 bit, not 1024)
  unsigned m_dwPaddingSize;
  unsigned short m_wBound;                 // only valid for intensity stereo
  unsigned short m_wAllocationTableIndex;  // only valid for MPEG 1 Layer II
                                           // (0=table a, 1=table b,...)

  // flags
  bool m_bCopyright, m_bPrivate, m_bOriginal;
  bool m_bCRC;
  unsigned char m_ModeExt;
};

#endif  // !MPA_HEADER_INFO_MPA_HEADER_H_
