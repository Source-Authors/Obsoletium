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

#ifndef MPA_HEADER_INFO_MPA_EXCEPTION_H_
#define MPA_HEADER_INFO_MPA_EXCEPTION_H_

// exception class
class CMPAException {
 public:
  enum class ErrorIDs {
    ErrOpenFile,
    ErrSetPosition,
    ErrReadFile,
    NoVBRHeader,
    IncompleteVBRHeader,
    NoFrameInTolerance,
    EndOfFile,
    HeaderCorrupt,
    FreeBitrate,
    IncompatibleHeader,
    CorruptLyricsTag,
    NumIDs  // this specifies the total number of possible IDs
  };

  CMPAException(ErrorIDs ErrorID, LPCTSTR szFile = nullptr,
                LPCTSTR szFunction = nullptr, bool bGetLastError = false);
  // copy constructor (necessary because of LPSTR members)
  CMPAException(const CMPAException& Source);
  CMPAException& operator=(CMPAException Source);
  virtual ~CMPAException();

  ErrorIDs GetErrorID() const { return m_ErrorID; };
  LPCTSTR GetErrorDescription();
  void ShowError();

 private:
  ErrorIDs m_ErrorID;
  bool m_bGetLastError;
  LPTSTR m_szFunction;
  LPTSTR m_szFile;
  LPTSTR m_szErrorMsg;

  static LPCTSTR m_szErrors[static_cast<int>(CMPAException::ErrorIDs::NumIDs)];
};

#endif  // !MPA_HEADER_INFO_MPA_EXCEPTION_H_