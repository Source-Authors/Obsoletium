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

#ifndef MPA_HEADER_INFO_XING_HEADER_H_
#define MPA_HEADER_INFO_XING_HEADER_H_

#include "VBRHeader.h"

class CMPAFrame;
class CLAMETag;

class CXINGHeader : public CVBRHeader {
 public:
  [[nodiscard]] static CXINGHeader* FindHeader(const CMPAFrame* frame);

  CXINGHeader(const CMPAFrame* frame, unsigned offset);
  virtual ~CXINGHeader();

  unsigned SeekPosition(float& percent) const override;

  CLAMETag* m_pLAMETag;
};

#endif  // !MPA_HEADER_INFO_XING_HEADER_H_