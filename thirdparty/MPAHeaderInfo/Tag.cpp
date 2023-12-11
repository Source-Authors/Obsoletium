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

#include "Tag.h"

CTag::CTag(CMPAStream* stream, LPCTSTR name, bool is_appended, unsigned offset,
           unsigned size)
    : m_pStream(stream),
      m_bAppended(is_appended),
      m_dwOffset(offset),
      m_dwSize(size) {
  assert(stream);
  assert(name);

  m_fVersion = 0.0f;
  m_szName = _tcsdup(name);
}

CTag::~CTag() { free(m_szName); }

void CTag::SetVersion(unsigned char version_1, unsigned char version_2,
                      unsigned char version_3) {
  // only values between 0 and 9 are displayed correctly
  m_fVersion = version_1;
  m_fVersion += version_2 * 0.1f;
  m_fVersion += version_3 * 0.01f;
}