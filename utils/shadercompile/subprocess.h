// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_UTILS_SHADERCOMPILE_SUBPROCESS_H_
#define SRC_UTILS_SHADERCOMPILE_SUBPROCESS_H_

#include "cmdsink.h"

using BOOL = int;
using HANDLE = void*;
using DWORD = unsigned long;

class SubProcessKernelObjects {
  friend class SubProcessKernelObjects_Memory;

 public:
  SubProcessKernelObjects();
  ~SubProcessKernelObjects();

  SubProcessKernelObjects(SubProcessKernelObjects const &) = delete;
  SubProcessKernelObjects &operator=(SubProcessKernelObjects const &) = delete;

 protected:
  BOOL Create(char const *szBaseName);
  BOOL Open(char const *szBaseName);

 public:
  BOOL IsValid() const;
  void Close();

 protected:
  HANDLE m_hMemorySection;
  HANDLE m_hMutex;
  HANDLE m_hEvent[2];
  DWORD m_dwCookie;
};

class SubProcessKernelObjects_Create : public SubProcessKernelObjects {
 public:
  SubProcessKernelObjects_Create(char const *szBaseName) {
    Create(szBaseName), m_dwCookie = 1;
  }
};

class SubProcessKernelObjects_Open : public SubProcessKernelObjects {
 public:
  SubProcessKernelObjects_Open(char const *szBaseName) {
    Open(szBaseName), m_dwCookie = 0;
  }
};

class SubProcessKernelObjects_Memory {
 public:
  SubProcessKernelObjects_Memory(SubProcessKernelObjects *p)
      : m_pObjs(p), m_pLockData(nullptr), m_pMemory(nullptr) {}
  ~SubProcessKernelObjects_Memory() { Unlock(); }

 public:
  void *Lock();
  BOOL Unlock();

 public:
  BOOL IsValid() const { return m_pLockData != nullptr; }
  void *GetMemory() const { return m_pMemory; }

 protected:
  void *m_pMemory;

 private:
  SubProcessKernelObjects *m_pObjs;
  void *m_pLockData;
};

//
// Response implementation
//
class CSubProcessResponse : public se::shader_compile::command_sink::IResponse {
 public:
  explicit CSubProcessResponse(void const *pvMemory);
  ~CSubProcessResponse() = default;

  bool Succeeded() const override { return 1 == m_dwResult; }

  size_t GetResultBufferLen() override {
    return Succeeded() ? m_dwResultBufferLength : 0;
  }
  const void *GetResultBuffer() override {
    return Succeeded() ? m_pvResultBuffer : nullptr;
  }
  const char *GetListing() override {
    return m_szListing && *m_szListing ? static_cast<const char *>(m_szListing)
                                       : nullptr;
  }

 protected:
  void const *m_pvMemory;
  DWORD m_dwResult;
  DWORD m_dwResultBufferLength;
  void const *m_pvResultBuffer;
  char const *m_szListing;
};

int ShaderCompile_Subprocess_Main(char const *szSubProcessData);

#endif  // !SRC_UTILS_SHADERCOMPILE_SUBPROCESS_H_
