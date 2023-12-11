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

#ifndef MPA_HEADER_INFO_MPA_FILE_H_
#define MPA_HEADER_INFO_MPA_FILE_H_

#include "MPAException.h"
#include "MPAFileStream.h"
#include "MPAFrame.h"
#include "Tags.h"

class CMPAFile {
 public:
  explicit CMPAFile(LPCTSTR szFile);
  ~CMPAFile();

  [[nodiscard]] unsigned GetBegin() const { return m_pTags->GetBegin(); };
  [[nodiscard]] unsigned GetEnd() const { return m_pTags->GetEnd(); };
  [[nodiscard]] unsigned GetFileSize() const {
    return (GetEnd() - GetBegin());
  };
  [[nodiscard]] unsigned GetLengthSec() const {
    return (GetFileSize() / m_dwBytesPerSec);
  };

  enum class GetType { First, Last, Next, Prev, Resync };

  [[nodiscard]] CMPAFrame* GetFrame(GetType type, CMPAFrame* frame = nullptr,
                                    bool should_delete_old_frame = true,
                                    unsigned offset = 0);

 private:
  void CalcBytesPerSec();

  CMPAStream* m_pStream;
  unsigned m_dwBytesPerSec;

 public:
  CTags* m_pTags;            // contain list of present tags
  CMPAFrame* m_pFirstFrame;  // always first frame
  CVBRHeader* m_pVBRHeader;  // XING or VBRI or nullptr
};

#endif  // !MPA_HEADER_INFO_MPA_FILE_H_