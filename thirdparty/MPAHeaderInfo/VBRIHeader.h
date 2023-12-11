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

#ifndef MPA_HEADER_INFO_VBRI_HEADER_H_
#define MPA_HEADER_INFO_VBRI_HEADER_H_

#include "VBRHeader.h"

class CMPAFrame;

class CVBRIHeader : public CVBRHeader {
 public:
  static CVBRIHeader* FindHeader(const CMPAFrame* frame);

  CVBRIHeader(const CMPAFrame* frame, unsigned offset);
  virtual ~CVBRIHeader();

  unsigned SeekPosition(float& percent) const override;
  unsigned SeekPositionByTime(float entry_time_ms) const;

  // these values exist only in VBRI headers
  float m_fDelay;
  unsigned m_dwTableScale;  // for seeking
  unsigned m_dwBytesPerEntry;
  unsigned m_dwFramesPerEntry;
  unsigned m_dwVersion;

 private:
  unsigned m_dwLengthSec;
};

#endif  // !MPA_HEADER_INFO_VBRI_HEADER_H_
