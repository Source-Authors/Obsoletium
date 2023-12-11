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

#include "stdafx.h"

#include "MPAFileStream.h"

#include "MPAException.h"
#include "MPAEndOfFileException.h"

#include <Windows.h>

// 1KB is initial buffersize
const unsigned CMPAFileStream::INIT_BUFFERSIZE = 1024U;

CMPAFileStream::CMPAFileStream(LPCTSTR file_name)
    : CMPAStream(file_name), m_dwOffset(0) {
  // open with CreateFile (no limitation of 128byte filename length, like in
  // mmioOpen)
  m_hFile = ::CreateFile(file_name, GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (m_hFile == INVALID_HANDLE_VALUE) {
    // throw error
    throw CMPAException{CMPAException::ErrorIDs::ErrOpenFile, file_name,
                        _T("CreateFile"),
                        true};
  }

  m_bMustReleaseFile = true;
  Init();
}

CMPAFileStream::CMPAFileStream(LPCTSTR file_name, HANDLE hFile)
    : CMPAStream(file_name), m_hFile(hFile), m_dwOffset(0) {
  m_bMustReleaseFile = false;
  Init();
}

void CMPAFileStream::Init() {
  m_dwBufferSize = INIT_BUFFERSIZE;
  // fill buffer for first time
  m_pBuffer = new unsigned char[m_dwBufferSize];

  FillBuffer(m_dwOffset, m_dwBufferSize, false);
}

CMPAFileStream::~CMPAFileStream() {
  delete[] m_pBuffer;

  // close file
  if (m_bMustReleaseFile) ::CloseHandle(m_hFile);
}

// set file position
void CMPAFileStream::SetPosition(unsigned offset) const {
  // convert from unsigned unsigned to signed 64bit long
  const unsigned result =
      ::SetFilePointer(m_hFile, offset, nullptr, FILE_BEGIN);

  if (result == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
    // != NO_ERROR
    // throw error
    throw CMPAException{CMPAException::ErrorIDs::ErrSetPosition, m_szFile,
                        _T("SetFilePointer"), true};
  }
}

unsigned char* CMPAFileStream::ReadBytes(unsigned size, unsigned& offset,
                                         bool move_offset,
                                         bool should_reverse) const {
  // enough bytes in buffer, otherwise read from file
  if (m_dwOffset > offset || (((ptrdiff_t)((m_dwOffset + m_dwBufferSize) -
                                           offset)) < (ptrdiff_t)size)) {
    if (!FillBuffer(offset, size, should_reverse)) {
      throw CMPAEndOfFileException{m_szFile};
    }
  }

  unsigned char* buffer{m_pBuffer + (offset - m_dwOffset)};

  if (move_offset) offset += size;

  return buffer;
}

unsigned CMPAFileStream::GetSize() const {
  unsigned size = ::GetFileSize(m_hFile, nullptr);

  if (size == INVALID_FILE_SIZE)
    throw CMPAException{CMPAException::ErrorIDs::ErrReadFile, m_szFile,
                        _T("GetFileSize"), true};

  return size;
}

// fills internal buffer, returns false if EOF is reached, otherwise true.
// Throws exceptions
bool CMPAFileStream::FillBuffer(unsigned offset, unsigned size,
                                bool should_reverse) const {
  // calc new buffer size
  if (size > m_dwBufferSize) {
    m_dwBufferSize = size;

    // release old buffer
    delete[] m_pBuffer;

    // reserve new buffer
    m_pBuffer = new unsigned char[m_dwBufferSize];
  }

  if (should_reverse) {
    if (offset + size < m_dwBufferSize)
      offset = 0;
    else
      offset = offset + size - m_dwBufferSize;
  }

  // read <m_dwBufferSize> bytes from offset <offset>
  m_dwBufferSize = Read(m_pBuffer, offset, m_dwBufferSize);

  // set new offset
  m_dwOffset = offset;

  if (m_dwBufferSize < size) return false;

  return true;
}

// read from file, return number of bytes read
unsigned CMPAFileStream::Read(void* data, unsigned offset,
                              unsigned size) const {
  DWORD bytes_read = 0;

  // set position first
  SetPosition(offset);

  if (!::ReadFile(m_hFile, data, size, &bytes_read, nullptr))
    throw CMPAException{CMPAException::ErrorIDs::ErrReadFile, m_szFile,
                        _T("ReadFile"), true};

  return bytes_read;
}