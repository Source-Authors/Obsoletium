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

#ifndef MPA_HEADER_INFO_LAME_TAG_H_
#define MPA_HEADER_INFO_LAME_TAG_H_

#include "Tag.h"

class CLAMETag : public CTag {
 public:
  [[nodiscard]] static CLAMETag* FindTag(CMPAStream* stream, bool is_appended,
                                         unsigned begin_offset,
                                         unsigned end_offset);
  ~CLAMETag();

  CString m_strEncoder;
  unsigned m_dwLowpassFilterHz;
  unsigned char m_bBitrate;  // in kbps
  unsigned char m_bRevision;

  bool IsVBR() const;
  bool IsABR() const;
  bool IsCBR() const;
  LPCTSTR GetVBRInfo() const;
  bool IsSimpleTag() const { return m_bSimpleTag; };

 private:
  CLAMETag(CMPAStream* stream, bool is_appended, unsigned offset);

  unsigned char m_bVBRInfo;
  bool m_bSimpleTag;
  static LPCTSTR m_szVBRInfo[10];
};

#endif  // !MPA_HEADER_INFO_LAME_TAG_H_
