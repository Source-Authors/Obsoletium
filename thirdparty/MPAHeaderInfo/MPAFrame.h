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

#ifndef MPA_HEADER_INFO_MPA_FRAME_H_
#define MPA_HEADER_INFO_MPA_FRAME_H_

#include "MPAStream.h"
#include "MPAHeader.h"
#include "VBRHeader.h"
#include "VBRIHeader.h"
#include "XINGHeader.h"

class CMPAFrame {
 public:
  CMPAFrame(CMPAStream* stream, unsigned& offset, bool bFindSubsequentFrame,
            bool bExactOffset, bool bReverse, CMPAHeader* pCompareHeader);
  ~CMPAFrame();

  [[nodiscard]] CVBRHeader* FindVBRHeader() const;

  [[nodiscard]] unsigned GetSubsequentHeaderOffset() const {
    return m_dwOffset + m_dwFrameSize;
  };
  [[nodiscard]] bool CheckCRC() const;
  [[nodiscard]] bool IsLast() const { return m_bIsLast; };

 public:
  CMPAHeader* m_pHeader;
  CMPAStream* m_pStream;

  unsigned m_dwOffset;     // offset in bytes where frame begins
  unsigned m_dwFrameSize;  // calculated frame size

 private:
  static const unsigned m_dwProtectedBitsLayer2[5][2];
  static unsigned short CalcCRC16(unsigned char* pBuffer, unsigned dwSize);

  bool m_bIsLast;  // true, if it is last frame
};

#endif  // !MPA_HEADER_INFO_MPA_FRAME_H_
