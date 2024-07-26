// Copyright Valve Corporation, All rights reserved.
//
//

#include "subprocess.h"

#include "winlite.h"
#include "cmdsink.h"
#include "d3dxfxc.h"
#include "tools_minidump.h"

//
// Base implementation of the shaderd kernel objects
//

SubProcessKernelObjects::SubProcessKernelObjects()
    : m_hMemorySection(nullptr), m_hMutex(nullptr) {
  ZeroMemory(m_hEvent, sizeof(m_hEvent));
  m_dwCookie = 0;
}

SubProcessKernelObjects::~SubProcessKernelObjects() { Close(); }

BOOL SubProcessKernelObjects::Create(char const *szBaseName) {
  char chBufferName[0x100] = {0};
  sprintf(chBufferName, "%s_msec", szBaseName);

  // 4Mb for a piece
  m_hMemorySection =
      CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                        4 * 1024 * 1024, chBufferName);
  if (nullptr != m_hMemorySection) {
    if (ERROR_ALREADY_EXISTS == GetLastError()) {
      CloseHandle(m_hMemorySection);
      m_hMemorySection = nullptr;

      Assert(0 && "CreateFileMapping - already exists!\n");
    }
  }

  sprintf(chBufferName, "%s_mtx", szBaseName);
  m_hMutex = CreateMutex(nullptr, FALSE, chBufferName);

  for (int k = 0; k < 2; ++k) {
    sprintf(chBufferName, "%s_evt%d", szBaseName, k);

    m_hEvent[k] = CreateEvent(nullptr, FALSE, (k ? TRUE /* = master */ : FALSE),
                              chBufferName);
  }

  return IsValid();
}

BOOL SubProcessKernelObjects::Open(char const *szBaseName) {
  char chBufferName[0x100] = {0};

  sprintf(chBufferName, "%s_msec", szBaseName);
  m_hMemorySection = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, chBufferName);

  sprintf(chBufferName, "%s_mtx", szBaseName);
  m_hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, chBufferName);

  for (int k = 0; k < 2; ++k) {
    sprintf(chBufferName, "%s_evt%d", szBaseName, k);
    m_hEvent[k] = OpenEvent(EVENT_ALL_ACCESS, FALSE, chBufferName);
  }

  return IsValid();
}

BOOL SubProcessKernelObjects::IsValid() const {
  return m_hMemorySection && m_hMutex && m_hEvent;
}

void SubProcessKernelObjects::Close() {
  if (m_hMemorySection) CloseHandle(m_hMemorySection);

  if (m_hMutex) CloseHandle(m_hMutex);

  for (int k = 0; k < 2; ++k)
    if (m_hEvent[k]) CloseHandle(m_hEvent[k]);
}

//
// Helper class to send data back and forth
//

void *SubProcessKernelObjects_Memory::Lock() {
  // Wait for our turn to act
  for (unsigned iWaitAttempt = 0; iWaitAttempt < 13u; ++iWaitAttempt) {
    DWORD dwWait =
        ::WaitForSingleObject(m_pObjs->m_hEvent[m_pObjs->m_dwCookie], 10000);

    switch (dwWait) {
      case WAIT_OBJECT_0: {
        m_pLockData = MapViewOfFile(m_pObjs->m_hMemorySection,
                                    FILE_MAP_ALL_ACCESS, 0, 0, 0);

        if (m_pLockData && *(const DWORD *)m_pLockData != m_pObjs->m_dwCookie) {
          // Yes, this is our turn, set our cookie in that memory segment
          *(DWORD *)m_pLockData = m_pObjs->m_dwCookie;
          m_pMemory = ((byte *)m_pLockData) + 2 * sizeof(DWORD);

          return m_pMemory;
        }

        if (m_pLockData) {
          // We just acted, still waiting for result
          UnmapViewOfFile(m_pLockData);
          m_pLockData = NULL;
        }

        SetEvent(m_pObjs->m_hEvent[!m_pObjs->m_dwCookie]);
        SwitchToThread();

        continue;
      } break;

      case WAIT_TIMEOUT: {
        char chMsg[0x100];
        sprintf(chMsg, "th%08X> WAIT_TIMEOUT in Memory::Lock (attempt %d).\n",
                GetCurrentThreadId(), iWaitAttempt);
        OutputDebugString(chMsg);
      }
        continue;  // retry

      default:
        OutputDebugString("WAIT failure in Memory::Lock\n");
        SetLastError(ERROR_BAD_UNIT);
        return NULL;
    }
  }

  OutputDebugString("Ran out of wait attempts in Memory::Lock\n");
  SetLastError(ERROR_NOT_READY);
  return NULL;
}

BOOL SubProcessKernelObjects_Memory::Unlock() {
  if (m_pLockData) {
    // Assert that the memory hasn't been spoiled
    Assert(m_pObjs->m_dwCookie == *(const DWORD *)m_pLockData);

    UnmapViewOfFile(m_pLockData);
    m_pMemory = NULL;
    m_pLockData = NULL;

    SetEvent(m_pObjs->m_hEvent[!m_pObjs->m_dwCookie]);
    YieldProcessor();

    return TRUE;
  }

  return FALSE;
}

//////////////////////////////////////////////////////////////////////////
//
// Implementation of the command subprocess:
//
// MASTER      ---- command ------->     SUB
//				string		-	zero terminated command
// string.
//
//
// MASTER     <---- result --------      SUB
//				dword		-	1 if succeeded, 0 if
// failed 				dword		-	result buffer
// length, 0 if failed 				<bytes> -	result buffer
// data, none if result buffer length is 0 				string -
// zero-terminated listing string
//
//////////////////////////////////////////////////////////////////////////

CSubProcessResponse::CSubProcessResponse(void const *pvMemory)
    : m_pvMemory(pvMemory) {
  byte const *pBytes = (byte const *)pvMemory;

  m_dwResult = *(DWORD const *)pBytes;
  pBytes += sizeof(DWORD);

  m_dwResultBufferLength = *(DWORD const *)pBytes;
  pBytes += sizeof(DWORD);

  m_pvResultBuffer = pBytes;
  pBytes += m_dwResultBufferLength;

  m_szListing = (char const *)(*pBytes ? pBytes : NULL);
}

static void ShaderCompile_Subprocess_ExceptionHandler(unsigned long code,
                                                      void *info) {
  // Subprocesses just silently die in our case, then this case will be detected
  // by the worker process and an error code will be passed to the master
  Assert(!"ShaderCompile_Subprocess_ExceptionHandler");

  ::TerminateProcess(::GetCurrentProcess(), code);
}

int ShaderCompile_Subprocess_Main(char const *subprocess_data) {
  // Set our crash handler
  se::utils::common::SetupToolsMinidumpHandler(
      ShaderCompile_Subprocess_ExceptionHandler);

  // Get our kernel objects
  SubProcessKernelObjects_Open objs(subprocess_data);
  if (!objs.IsValid()) return -1;

  // Enter the command pumping loop
  SubProcessKernelObjects_Memory shrmem(&objs);
  for (void *mem = nullptr; nullptr != (mem = shrmem.Lock()); shrmem.Unlock()) {
    // The memory is actually a command
    char const *command{static_cast<char const *>(mem)};

    if (!stricmp("keepalive", command)) {
      ZeroMemory(mem, 4 * sizeof(DWORD));
      continue;
    }

    if (!stricmp("quit", command)) {
      ZeroMemory(mem, 4 * sizeof(DWORD));
      return 0;
    }

    se::shader_compile::command_sink::IResponse *response{nullptr};
    if (se::shader_compile::fxc_intercept::TryExecuteCommand(command,
                                                             &response)) {
      byte *bytes = static_cast<byte *>(mem);

      // Result
      const DWORD succeeded{response->Succeeded() ? 1UL : 0UL};
      memcpy(bytes, &succeeded, sizeof(DWORD));
      bytes += sizeof(DWORD);

      // Result buffer len.
      const DWORD buffer_size{
          static_cast<DWORD>(response->GetResultBufferLen())};
      memcpy(bytes, &buffer_size, sizeof(DWORD));
      bytes += sizeof(DWORD);

      // Result buffer.
      const void *buffer{response->GetResultBuffer()};
      memcpy(bytes, buffer, buffer_size);
      bytes += buffer_size;

      // Listing - copy string.
      const char *errors{response->GetListing()};
      if (errors) {
        while (0 != (*(bytes++) = *(errors++))) {
        }
      } else {
        *(bytes++) = 0;
      }

      response->Release();
    } else {
      ZeroMemory(mem, 4 * sizeof(DWORD));
    }
  }

  return -2;
}
