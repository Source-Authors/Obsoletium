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

#ifndef MPA_HEADER_INFO_MPA_FILE_STREAM_H_
#define MPA_HEADER_INFO_MPA_FILE_STREAM_H_

#include "MPAStream.h"

using HANDLE = void*;

class CMPAFileStream : public CMPAStream {
 public:
  explicit CMPAFileStream(LPCTSTR file_name);
  CMPAFileStream(LPCTSTR file_name, HANDLE hFile);

  virtual ~CMPAFileStream();

 private:
  static const unsigned INIT_BUFFERSIZE;

  HANDLE m_hFile;
  bool m_bMustReleaseFile;

  // concerning read-buffer
  mutable unsigned char* m_pBuffer;
  mutable unsigned m_dwOffset;  // offset (beginning if m_pBuffer)
  mutable unsigned m_dwBufferSize;

  void Init();
  unsigned Read(void* data, unsigned offset, unsigned size) const;
  bool FillBuffer(unsigned offset, unsigned size, bool should_reverse) const;
  void SetPosition(unsigned offset) const;

 protected:
  // methods for file access (must be implemented by all derived classes)

  virtual unsigned char* ReadBytes(unsigned size, unsigned& offset,
                                   bool should_move_offset = true,
                                   bool should_reverse = false) const;
  virtual unsigned GetSize() const;
};

#endif  // !MPA_HEADER_INFO_MPA_FILE_STREAM_H_
