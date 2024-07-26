// Copyright Valve Corporation, All rights reserved.
//
// Purpose: Command sink interface implementation.
//

#ifndef SRC_UTILS_SHADERCOMPILE_CMDSINK_H_
#define SRC_UTILS_SHADERCOMPILE_CMDSINK_H_

#include <cstdio>
#include "tier1/utlbuffer.h"

namespace se::shader_compile::command_sink {

/**
 * @brief Interface to give back command execution results.
 */
struct IResponse {
  virtual ~IResponse() = default;
  virtual void Release() { delete this; }

  // Returns whether the command succeeded
  virtual bool Succeeded() const = 0;

  // If the command succeeded returns the result buffer length, otherwise zero
  virtual size_t GetResultBufferLen() = 0;

  // If the command succeeded returns the result buffer base pointer, otherwise
  // NULL
  virtual const void *GetResultBuffer() = 0;

  // Returns a zero-terminated string of messages reported during command
  // execution, or NULL if nothing was reported
  virtual const char *GetListing() = 0;
};

/**
 * @brief Response implementation when the result should appear in one file and
 * the listing should appear in another file.
 */
class CResponseFiles final : public IResponse {
 public:
  CResponseFiles(char const *szFileResult, char const *szFileListing);
  virtual ~CResponseFiles();

  // Returns whether the command succeeded
  bool Succeeded() const override;

  // If the command succeeded returns the result buffer length, otherwise zero
  size_t GetResultBufferLen() override;

  // If the command succeeded returns the result buffer base pointer, otherwise
  // NULL
  const void *GetResultBuffer() override;

  // Returns a zero-terminated string of messages reported during command
  // execution
  const char *GetListing() override;

 protected:
  void OpenResultFile() const;  //!< Opens the result file if not open yet
  void ReadResultFile();        //!< Reads the result buffer if not read yet
  void ReadListingFile();       //!< Reads the listing buffer if not read yet

 protected:
  char m_szFileResult[MAX_PATH];   //!< Name of the result file
  char m_szFileListing[MAX_PATH];  //!< Name of the listing file

  mutable FILE *m_fResult;  //!< Result file (NULL if not open)
  FILE *m_fListing;         //!< Listing file (NULL if not open)

  CUtlBuffer m_bufResult;  //!< Buffer holding the result data
  size_t m_lenResult;      //!< Result data length (0 if result not read yet)

  //!< Data buffer pointer (NULL if result not read yet)
  const void *m_dataResult;

  CUtlBuffer m_bufListing;  //!< Buffer holding the listing

  //!< Listing buffer pointer (NULL if listing not read yet)
  const char *m_dataListing;
};

/**
 * @brief Response implementation when the result is a generic error.
 */
struct CResponseError : public IResponse {
  explicit CResponseError() = default;
  ~CResponseError() = default;

  bool Succeeded() const override { return false; }

  size_t GetResultBufferLen() override { return 0; }
  const void *GetResultBuffer() override { return nullptr; }

  const char *GetListing() override { return nullptr; }
};

};  // namespace se::shader_compile::command_sink

#endif  // !SRC_UTILS_SHADERCOMPILE_CMDSINK_H_
