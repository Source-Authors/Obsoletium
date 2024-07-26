// Copyright Valve Corporation, All rights reserved.
//
// Command sink interface implementation.

#include "cmdsink.h"

namespace se::shader_compile::command_sink {

CResponseFiles::CResponseFiles(char const *szFileResult,
                               char const *szFileListing)
    : m_fResult(nullptr),
      m_fListing(nullptr),
      m_lenResult(0),
      m_dataResult(nullptr),
      m_dataListing(nullptr) {
  sprintf(m_szFileResult, szFileResult);
  sprintf(m_szFileListing, szFileListing);
}

CResponseFiles::~CResponseFiles() {
  if (m_fResult) fclose(m_fResult);

  if (m_fListing) fclose(m_fListing);
}

bool CResponseFiles::Succeeded() const {
  OpenResultFile();
  return (m_fResult != nullptr);
}

size_t CResponseFiles::GetResultBufferLen() {
  ReadResultFile();
  return m_lenResult;
}

const void *CResponseFiles::GetResultBuffer() {
  ReadResultFile();
  return m_dataResult;
}

const char *CResponseFiles::GetListing() {
  ReadListingFile();
  return ((m_dataListing && *m_dataListing) ? m_dataListing : nullptr);
}

void CResponseFiles::OpenResultFile() const {
  if (!m_fResult) {
    m_fResult = fopen(m_szFileResult, "rb");
  }
}

void CResponseFiles::ReadResultFile() {
  if (!m_dataResult) {
    OpenResultFile();

    if (m_fResult) {
      fseek(m_fResult, 0, SEEK_END);
      m_lenResult = (size_t)ftell(m_fResult);

      if (m_lenResult != size_t(-1)) {
        m_bufResult.EnsureCapacity(m_lenResult);
        fseek(m_fResult, 0, SEEK_SET);
        fread(m_bufResult.Base(), 1, m_lenResult, m_fResult);
        m_dataResult = m_bufResult.Base();
      }
    }
  }
}

void CResponseFiles::ReadListingFile() {
  if (!m_dataListing) {
    if (!m_fListing) m_fListing = fopen(m_szFileListing, "rb");

    if (m_fListing) {
      fseek(m_fListing, 0, SEEK_END);
      size_t len = (size_t)ftell(m_fListing);

      if (len != size_t(-1)) {
        m_bufListing.EnsureCapacity(len);

        fseek(m_fListing, 0, SEEK_SET);
        fread(m_bufListing.Base(), 1, len, m_fListing);

        m_dataListing = m_bufListing.Base<const char>();
      }
    }
  }
}

};  // namespace se::shader_compile::command_sink
