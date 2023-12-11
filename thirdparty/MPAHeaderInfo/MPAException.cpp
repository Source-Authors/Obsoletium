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

#include "MPAException.h"

#include <Windows.h>

/// CMPAException: exception class
//////////////////////////////////////////////

CMPAException::CMPAException(ErrorIDs ErrorID, LPCTSTR szFile,
                             LPCTSTR szFunction, bool bGetLastError)
    : m_ErrorID(ErrorID),
      m_bGetLastError(bGetLastError),
      m_szErrorMsg(nullptr) {
  m_szFile = _tcsdup(szFile);
  m_szFunction = _tcsdup(szFunction);
}

// copy constructor (necessary for exception throwing without pointers)
CMPAException::CMPAException(const CMPAException& Source) {
  m_ErrorID = Source.m_ErrorID;
  m_bGetLastError = Source.m_bGetLastError;
  m_szFile = _tcsdup(Source.m_szFile);
  m_szFunction = _tcsdup(Source.m_szFunction);
  if (Source.m_szErrorMsg) {
    const size_t maxSize = strlen(Source.m_szErrorMsg) + 1;
    m_szErrorMsg = new char[maxSize];
    _tcscpy_s(m_szErrorMsg, maxSize, Source.m_szErrorMsg);
  } else {
    m_szErrorMsg = nullptr;
  }
}

CMPAException& CMPAException::operator=(CMPAException Source) {
  std::swap(m_ErrorID, Source.m_ErrorID);
  std::swap(m_bGetLastError, Source.m_bGetLastError);
  std::swap(m_szFunction, Source.m_szFunction);
  std::swap(m_szFile, Source.m_szFile);
  std::swap(m_szErrorMsg, Source.m_szErrorMsg);

  return *this;
}

// destructor
CMPAException::~CMPAException() {
  free(m_szFile);
  free(m_szFunction);
  delete[] m_szErrorMsg;
}

// should be in resource file for multi language applications
LPCTSTR CMPAException::m_szErrors[static_cast<int>(
    CMPAException::ErrorIDs::NumIDs)] = {
    _T("Can't open the file."),
    _T("Can't set file position."),
    _T("Can't read from file."),
    _T("No VBR Header found."),
    _T("Incomplete VBR Header."),
    _T("No frame found within tolerance range."),
    _T("No frame found before end of file was reached."),
    _T("Header corrupt"),
    _T("Free Bitrate currently not supported"),
    _T("Incompatible Header"),
    _T("Corrupt Lyrics3 Tag")};

constexpr unsigned MAX_ERR_LENGTH{256};

void CMPAException::ShowError() {
  LPCTSTR pErrorMsg = GetErrorDescription();
  // show error message
  ::MessageBox(nullptr, pErrorMsg, _T("MPAFile Error"), MB_OK | MB_ICONERROR);
}

LPCTSTR CMPAException::GetErrorDescription() {
  if (!m_szErrorMsg) {
    m_szErrorMsg = new TCHAR[MAX_ERR_LENGTH];
    m_szErrorMsg[0] = '\0';

    TCHAR help[MAX_ERR_LENGTH];

    // this is not buffer-overflow-proof!
    if (m_szFunction) {
      _stprintf_s(help, MAX_ERR_LENGTH, _T("%s: "), m_szFunction);
      _tcscat_s(m_szErrorMsg, MAX_ERR_LENGTH, help);
    }

    if (m_szFile) {
      _stprintf_s(help, MAX_ERR_LENGTH, _T("'%s'\n"), m_szFile);
      _tcscat_s(m_szErrorMsg, MAX_ERR_LENGTH, help);
    }

    _tcscat_s(m_szErrorMsg, MAX_ERR_LENGTH,
              m_szErrors[static_cast<int>(m_ErrorID)]);

    if (m_bGetLastError) {
      // get error message of last system error id
      LPVOID buffer = nullptr;
      if (FormatMessage(
              FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS,
              nullptr, GetLastError(),
              MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // Default language
              (LPTSTR)&buffer, 0, nullptr)) {
        _tcscat_s(m_szErrorMsg, MAX_ERR_LENGTH, _T("\n"));
        _tcscat_s(m_szErrorMsg, MAX_ERR_LENGTH, (LPCTSTR)buffer);
        LocalFree(buffer);
      }
    }

    // make sure string is null-terminated
    m_szErrorMsg[MAX_ERR_LENGTH - 1] = '\0';
  }

  return m_szErrorMsg;
}