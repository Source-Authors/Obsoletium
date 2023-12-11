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

#ifndef MPA_HEADER_INFO_TAG_H_
#define MPA_HEADER_INFO_TAG_H_

#include "mpastream.h"

class CTag {
 public:
  [[nodiscard]] unsigned GetOffset() const { return m_dwOffset; };
  [[nodiscard]] unsigned GetEnd() const { return m_dwOffset + m_dwSize; };
  [[nodiscard]] LPCTSTR GetName() const { return m_szName; };
  [[nodiscard]] unsigned GetSize() const { return m_dwSize; };
  [[nodiscard]] float GetVersion() const { return m_fVersion; };

  virtual ~CTag();

 protected:
  CMPAStream* m_pStream;

  CTag(CMPAStream* stream, LPCTSTR szName, bool is_appended,
       unsigned offset = 0, unsigned dwSize = 0);

  unsigned m_dwOffset;  // beginning of tag
  unsigned m_dwSize;    // size of tag
  bool m_bAppended;     // true if at the end of file
  float m_fVersion;     // format x.yz
  LPTSTR m_szName;      // name of tag

  void SetVersion(unsigned char bVersion1, unsigned char bVersion2 = 0, unsigned char bVersion3 = 0);
};

#endif  // !MPA_HEADER_INFO_TAG_H_