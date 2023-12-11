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

#include "Tags.h"

#include "MPAException.h"

#include "ID3V1Tag.h"
#include "ID3V2Tag.h"
#include "Lyrics3Tag.h"
#include "APETag.h"
#include "MusicMatchTag.h"

const CTags::FindTagFunctionPtr CTags::m_appendedTagFactories[] = {
    (CTags::FindTagFunctionPtr)&CID3V1Tag::FindTag,
    (CTags::FindTagFunctionPtr)&CMusicMatchTag::FindTag,
    (CTags::FindTagFunctionPtr)&CID3V2Tag::FindTag,
    (CTags::FindTagFunctionPtr)&CLyrics3Tag::FindTag,
    (CTags::FindTagFunctionPtr)&CAPETag::FindTag};

const CTags::FindTagFunctionPtr CTags::m_prependedTagFactories[] = {
    (CTags::FindTagFunctionPtr)&CID3V2Tag::FindTag,
    (CTags::FindTagFunctionPtr)&CAPETag::FindTag};

CTags::CTags(CMPAStream* stream) : m_dwBegin(0), m_dwEnd(stream->GetSize()) {
  // all appended tags
  /////////////////////////////
  while (FindAppendedTag(stream))
    ;

  // all prepended tags
  /////////////////////////////
  while (FindPrependedTag(stream))
    ;
}

CTags::~CTags() {
  for (auto* tag : m_Tags) delete tag;
}

bool CTags::FindPrependedTag(CMPAStream* stream) {
  for (auto&& f : m_prependedTagFactories) {
    if (FindTag(f, stream, false)) return true;
  }
  return false;
}

bool CTags::FindAppendedTag(CMPAStream* stream) {
  for (auto&& f : m_appendedTagFactories) {
    if (FindTag(f, stream, true)) return true;
  }
  return false;
}

bool CTags::FindTag(FindTagFunctionPtr find_tag, CMPAStream* stream,
                    bool is_appended) {
  try {
    CTag* tag = find_tag(stream, is_appended, m_dwBegin, m_dwEnd);
    if (tag) {
      if (is_appended)
        m_dwEnd = tag->GetOffset();
      else
        m_dwBegin = tag->GetEnd();

      m_Tags.push_back(tag);
      return true;
    }
  } catch (CMPAException& ex) {
    ex.ShowError();
  }

  return false;
}

CTag* CTags::GetNextTag(size_t& index) const {
  CTag* tag{index < m_Tags.size() ? m_Tags[index] : nullptr};

  index++;

  return tag;
}
