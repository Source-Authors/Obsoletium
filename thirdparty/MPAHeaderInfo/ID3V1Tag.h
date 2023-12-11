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

#ifndef MPA_HEADER_INFO_ID3V1_TAG_H_
#define MPA_HEADER_INFO_ID3V1_TAG_H_

#include "Tag.h"

class CID3V1Tag : public CTag {
 public:
  [[nodiscard]] static CID3V1Tag* FindTag(CMPAStream* stream, bool is_appended,
                                          unsigned begin_offset,
                                          unsigned end_offset);
  ~CID3V1Tag();

 private:
  CID3V1Tag(CMPAStream* stream, unsigned offset);
};

#endif  // !MPA_HEADER_INFO_ID3V1_TAG_H_
