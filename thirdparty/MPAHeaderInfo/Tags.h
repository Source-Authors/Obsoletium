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

#ifndef MPA_HEADER_INFO_TAGS_H_
#define MPA_HEADER_INFO_TAGS_H_

#include <vector>

#include "MPAStream.h"
#include "Tag.h"

class CTags {
 public:
  explicit CTags(CMPAStream* stream);
  ~CTags();

  [[nodiscard]] CTag* GetNextTag(size_t& nIndex) const;
  template <typename TagClass>
  [[nodiscard]] bool FindTag(TagClass*& pTag) const;

  // get begin offset after prepended tags
  [[nodiscard]] unsigned GetBegin() const { return m_dwBegin; };
  // get end offset before appended tags
  [[nodiscard]] unsigned GetEnd() const { return m_dwEnd; };

 private:
  [[nodiscard]] bool FindAppendedTag(CMPAStream* stream);
  [[nodiscard]] bool FindPrependedTag(CMPAStream* stream);

  // definition of function pointer type
  using FindTagFunctionPtr = CTag* (*)(CMPAStream*, bool, unsigned, unsigned);
  [[nodiscard]] bool FindTag(FindTagFunctionPtr find_tag, CMPAStream* stream,
                             bool is_appended);

  std::vector<CTag*> m_Tags;
  unsigned m_dwBegin, m_dwEnd;

  static const FindTagFunctionPtr m_appendedTagFactories[];
  static const FindTagFunctionPtr m_prependedTagFactories[];
};

// you need to compile with runtime information to use this method
template <typename TagClass>
bool CTags::FindTag(TagClass*& pTag) const {
  for (auto* tag : m_Tags) {
    pTag = dynamic_cast<TagClass*>(tag);
    if (pTag) return true;
  }

  return false;
}

#endif  // !MPA_HEADER_INFO_TAGS_H_
