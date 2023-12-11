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

#ifndef MPA_HEADER_INFO_VBR_HEADER_H_
#define MPA_HEADER_INFO_VBR_HEADER_H_

class CMPAFrame;
class CMPAStream;

class CVBRHeader {
 public:
  [[nodiscard]] static CVBRHeader* FindHeader(const CMPAFrame* frame);

  virtual ~CVBRHeader();

  [[nodiscard]] bool SeekPosition(float& percent, unsigned& seek_point) const;

  unsigned m_dwBytes;   // total number of bytes
  unsigned m_dwFrames;  // total number of frames

 protected:
  CVBRHeader(CMPAStream* stream, unsigned offset);

  [[nodiscard]] static bool CheckID(CMPAStream* stream, unsigned offset,
                                    char ch0, char ch1, char ch2, char ch3);

  virtual unsigned SeekPosition(float& percent) const = 0;

  CMPAStream* m_pStream;

 public:
  unsigned m_dwOffset;
  unsigned m_dwQuality;    // quality (0..100)
  int* m_pnToc;            // TOC points for seeking (must be freed)
  unsigned m_dwTableSize;  // size of table (number of entries)
};

#endif  // !MPA_HEADER_INFO_VBR_HEADER_H_
