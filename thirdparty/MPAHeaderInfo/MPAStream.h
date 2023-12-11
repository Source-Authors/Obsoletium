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

#ifndef MPA_HEADER_INFO_MPA_STREAM_H_
#define MPA_HEADER_INFO_MPA_STREAM_H_

class CMPAStream {
 public:
  virtual ~CMPAStream();

  virtual [[nodiscard]] unsigned GetSize() const = 0;
  virtual [[nodiscard]] unsigned char* ReadBytes(
      unsigned dwSize, unsigned& offset, bool bMoveOffset = true,
      bool bReverse = false) const = 0;

  [[nodiscard]] unsigned ReadBEValue(unsigned dwNumBytes, unsigned& offset,
                                     bool bMoveOffset = true) const;
  [[nodiscard]] unsigned ReadLEValue(unsigned dwNumBytes, unsigned& offset,
                                     bool bMoveOffset = true) const;
  [[nodiscard]] LPCTSTR GetFilename() const { return m_szFile; };

 protected:
  LPTSTR m_szFile;

  CMPAStream(LPCTSTR file_name);
};

#endif  // !MPA_HEADER_INFO_MPA_STREAM_H_
