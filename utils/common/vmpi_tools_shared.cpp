// Copyright Valve Corporation, All rights reserved.
//
//

#include "vmpi_tools_shared.h"

#include "winlite.h"
#include "scoped_dll.h"

#include <dbghelp.h>
#include <system_error>

#include "vmpi.h"
#include "cmdlib.h"
#include "tier0/minidump.h"
#include "tier1/strtools.h"
#include "mpi_stats.h"
#include "iphelpers.h"

namespace {

// Have we gotten the qdir info yet?
bool g_bReceivedDirectoryInfo = false;
bool g_bReceivedDBInfo = false;
CDBInfo g_DBInfo;
unsigned long g_JobPrimaryID;

// Tracks how many remote processes have disconnected ungracefully.
long g_nDisconnects = 0;

bool SharedDispatch(MessageBuffer *pBuf, int iSource, int iPacketID) {
  char *pInPos = &pBuf->data[2];

  switch (pBuf->data[1]) {
    case VMPI_SUBPACKETID_DIRECTORIES: {
      V_strcpy_safe(gamedir, pInPos);
      pInPos += strlen(pInPos) + 1;

      V_strcpy_safe(qdir, pInPos);

      g_bReceivedDirectoryInfo = true;
    }
      return true;

    case VMPI_SUBPACKETID_DBINFO: {
      g_DBInfo = *((CDBInfo *)pInPos);
      pInPos += sizeof(CDBInfo);
      g_JobPrimaryID = *((unsigned long *)pInPos);

      g_bReceivedDBInfo = true;
    }
      return true;

    case VMPI_SUBPACKETID_CRASH: {
      char const chCrashInfoType = *pInPos;
      pInPos += 2;
      switch (chCrashInfoType) {
        case 't':
          Warning("\nWorker '%s' dead: %s\n", VMPI_GetMachineName(iSource),
                  pInPos);
          break;

        case 'f': {
          int iFileSize = *reinterpret_cast<int const *>(pInPos);
          pInPos += sizeof(iFileSize);

          // Temp folder
          char const *szFolder = NULL;
          if (!szFolder) szFolder = getenv("TEMP");
          if (!szFolder) szFolder = getenv("TMP");
          if (!szFolder) szFolder = "c:";

          // Base module name
          char chModuleName[_MAX_PATH] = {}, *pModuleName = chModuleName;
          ::GetModuleFileName(NULL, chModuleName,
                              sizeof(chModuleName) / sizeof(chModuleName[0]));

          if (char *pch = strrchr(chModuleName, '.')) *pch = 0;
          if (char *pch = strrchr(chModuleName, '\\'))
            *pch = 0, pModuleName = pch + 1;

          // Current time
          time_t currTime = ::time(NULL);
          struct tm *pTime = ::localtime(&currTime);

          // Number of minidumps this run
          static unsigned int s_numMiniDumps = 0;

          // Prepare the filename
          char chSaveFileName[2 * _MAX_PATH] = {0};
          sprintf(chSaveFileName,
                  "%s\\vmpi_%s_on_%s_%d%02d%02d_%02d%02d%02d_%u.mdmp",
                  szFolder, pModuleName, VMPI_GetMachineName(iSource),
                  pTime->tm_year + 1900, /* Year less 2000 */
                  pTime->tm_mon + 1,     /* month (0 - 11 : 0 = January) */
                  pTime->tm_mday,        /* day of month (1 - 31) */
                  pTime->tm_hour,        /* hour (0 - 23) */
                  pTime->tm_min,         /* minutes (0 - 59) */
                  pTime->tm_sec,         /* seconds (0 - 59) */
                  InterlockedIncrement(&s_numMiniDumps));

          if (FILE *fDump = fopen(chSaveFileName, "wb")) {
            fwrite(pInPos, 1, iFileSize, fDump);
            fclose(fDump);

            Warning("\nSaved worker crash minidump '%s', size %d byte(s).\n",
                    chSaveFileName, iFileSize);
          } else {
            Warning(
                "\nReceived worker crash minidump size %d byte(s), failed to "
                "save.\n",
                iFileSize);
          }
        } break;
      }
    }
      return true;
  }

  return false;
}

CDispatchReg g_dr(VMPI_SHARED_PACKET_ID, SharedDispatch);

}  // namespace

void SendQDirInfo() {
  char cPacketID[2] = {VMPI_SHARED_PACKET_ID, VMPI_SUBPACKETID_DIRECTORIES};

  MessageBuffer mb;
  mb.write(cPacketID, 2);
  mb.write(gamedir, V_strlen(gamedir) + 1);
  mb.write(qdir, V_strlen(qdir) + 1);

  VMPI_SendData(mb.data, mb.getLen(), VMPI_PERSISTENT);
}

void RecvQDirInfo() {
  while (!g_bReceivedDirectoryInfo) {
    VMPI_DispatchNextMessage();
  }
}

void SendDBInfo(const CDBInfo *pInfo, unsigned long jobPrimaryID) {
  char cPacketInfo[2] = {VMPI_SHARED_PACKET_ID, VMPI_SUBPACKETID_DBINFO};
  const void *pChunks[] = {cPacketInfo, pInfo, &jobPrimaryID};
  ptrdiff_t chunkLengths[] = {2, sizeof(CDBInfo), sizeof(jobPrimaryID)};

  VMPI_SendChunks(pChunks, chunkLengths, ARRAYSIZE(pChunks), VMPI_PERSISTENT);
}

void RecvDBInfo(CDBInfo *pInfo, unsigned long *pJobPrimaryID) {
  while (!g_bReceivedDBInfo) VMPI_DispatchNextMessage();

  *pInfo = g_DBInfo;
  *pJobPrimaryID = g_JobPrimaryID;
}

// If the file is successfully opened, read and sent returns the size of the
// file in bytes otherwise returns 0 and nothing is sent
int VMPI_SendFileChunk(const void *pvChunkPrefix, int lenPrefix,
                       tchar const *ptchFileName) {
  HANDLE hFile = NULL;
  HANDLE hMapping = NULL;
  void const *pvMappedData = NULL;
  int iResult = 0;

  hFile = ::CreateFile(ptchFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
  if (!hFile || (hFile == INVALID_HANDLE_VALUE)) goto done;

  hMapping = ::CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
  if (!hMapping || (hMapping == INVALID_HANDLE_VALUE)) goto done;

  pvMappedData = ::MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
  if (!pvMappedData) goto done;

  int iMappedFileSize = ::GetFileSize(hFile, NULL);
  if (INVALID_FILE_SIZE == iMappedFileSize) goto done;

  // Send the data over VMPI
  if (VMPI_Send3Chunks(pvChunkPrefix, lenPrefix, &iMappedFileSize,
                       sizeof(iMappedFileSize), pvMappedData, iMappedFileSize,
                       VMPI_MASTER_ID))
    iResult = iMappedFileSize;

  // Fall-through for cleanup code to execute
done:
  if (pvMappedData) ::UnmapViewOfFile(pvMappedData);

  if (hMapping && (hMapping != INVALID_HANDLE_VALUE)) ::CloseHandle(hMapping);

  if (hFile && (hFile != INVALID_HANDLE_VALUE)) ::CloseHandle(hFile);

  return iResult;
}

void VMPI_HandleCrash(const char *pMessage, void *pvExceptionInfo,
                      bool bAssert) {
  static LONG crashHandlerCount = 0;
  if (InterlockedIncrement(&crashHandlerCount) == 1) {
    Msg("\nFAILURE: '%s' (assert: %d)\n", pMessage, bAssert);

    // Send a message to the master.
    char crashMsg[4] = {VMPI_SHARED_PACKET_ID, VMPI_SUBPACKETID_CRASH, 't',
                        ':'};

    VMPI_Send2Chunks(crashMsg, sizeof(crashMsg), pMessage,
                     V_strlen(pMessage) + 1, VMPI_MASTER_ID);

    // Now attempt to create a minidump with the given exception information
    if (pvExceptionInfo) {
      EXCEPTION_POINTERS *pvExPointers = (EXCEPTION_POINTERS *)pvExceptionInfo;
      tchar tchMinidumpFileName[_MAX_PATH] = {0};

      bool bSucceededWritingMinidump = WriteMiniDumpUsingExceptionInfo(
          pvExPointers->ExceptionRecord->ExceptionCode, pvExPointers,
          (MINIDUMP_TYPE)(MiniDumpWithDataSegs |
                          MiniDumpWithIndirectlyReferencedMemory |
                          MiniDumpWithProcessThreadData),
          tchMinidumpFileName);

      if (bSucceededWritingMinidump) {
        crashMsg[2] = 'f';

        VMPI_SendFileChunk(crashMsg, sizeof(crashMsg), tchMinidumpFileName);

        ::DeleteFile(tchMinidumpFileName);
      }
    }

    // Let the messages go out.
    Sleep(500);
  }

  InterlockedDecrement(&crashHandlerCount);
}

// This is called if we crash inside our crash handler. It just terminates the
// process immediately.
LONG __stdcall VMPI_SecondExceptionFilter(
    struct _EXCEPTION_POINTERS *ExceptionInfo) {
  TerminateProcess(GetCurrentProcess(), 2);
  return EXCEPTION_EXECUTE_HANDLER;  // (never gets here anyway)
}

#define SRC_CODE_2_ERROR_RECORD(name) \
  { name, #name }

struct {
  DWORD code;
  const char *pReason;
} g_errorDescriptions[] = {
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_ACCESS_VIOLATION),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_ARRAY_BOUNDS_EXCEEDED),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_BREAKPOINT),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_DATATYPE_MISALIGNMENT),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_FLT_DENORMAL_OPERAND),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_FLT_DIVIDE_BY_ZERO),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_FLT_INEXACT_RESULT),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_FLT_INVALID_OPERATION),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_FLT_OVERFLOW),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_FLT_STACK_CHECK),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_FLT_UNDERFLOW),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_ILLEGAL_INSTRUCTION),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_IN_PAGE_ERROR),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_INT_DIVIDE_BY_ZERO),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_INT_OVERFLOW),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_INVALID_DISPOSITION),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_NONCONTINUABLE_EXCEPTION),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_PRIV_INSTRUCTION),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_SINGLE_STEP),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_STACK_OVERFLOW),
    SRC_CODE_2_ERROR_RECORD(EXCEPTION_ACCESS_VIOLATION),
};

template <size_t size>
const char *GetExceptionDescription(unsigned long code, const char *reason,
                                    char (&description)[size]) {
  source::ScopedDll ntdll{"ntdll.dll", LOAD_LIBRARY_SEARCH_SYSTEM32};

  using RtlNtStatusToDosErrorFunction = ULONG (*)(LONG Status);

  const auto ntstatus2win32mapper =
      ntdll.GetFunction<RtlNtStatusToDosErrorFunction>("RtlNtStatusToDosError");

  if (ntstatus2win32mapper) {
    const ULONG win32err{ntstatus2win32mapper(code)};

    V_sprintf_safe(description, "%s: %s", reason,
                   std::system_category().message(win32err).c_str());
  }

  return description;
}

void VMPI_ExceptionFilter(unsigned long code, void *exception_info) {
  // This is called if we crash inside our crash handler. It just terminates the
  // process immediately.
  ::SetUnhandledExceptionFilter(VMPI_SecondExceptionFilter);

  constexpr ptrdiff_t nErrors = ssize(g_errorDescriptions);
  ptrdiff_t i = 0;

  const char *pchReason = nullptr;
  char chUnknownBuffer[32];
  for (i; (i < nErrors) && !pchReason; i++) {
    if (g_errorDescriptions[i].code == code)
      pchReason = g_errorDescriptions[i].pReason;
  }

  if (i == nErrors) {
    sprintf_s(chUnknownBuffer, "Error code 0x%08X", code);
    pchReason = chUnknownBuffer;
  }

  char description[512];

  VMPI_HandleCrash(GetExceptionDescription(code, pchReason, description),
                   exception_info, true);

  ::TerminateProcess(::GetCurrentProcess(), code);
}

void HandleMPIDisconnect(int procID, const char *pReason) {
  int nLiveWorkers = VMPI_GetCurrentNumberOfConnections() -
                     ReadVolatileMemory(&g_nDisconnects) - 1;

  // We ran into the size limit before and it wasn't readily apparent that the
  // size limit had been breached, so make sure to show errors about invalid
  // packet sizes..
  bool bOldSuppress = g_bSuppressPrintfOutput;
  g_bSuppressPrintfOutput = Q_stristr(pReason, "invalid packet size") == 0;

  Warning("\n\n--- WARNING: lost connection to '%s' (%s).\n",
          VMPI_GetMachineName(procID), pReason);

  if (g_bMPIMaster) {
    Warning("%d workers remain.\n\n", nLiveWorkers);

    InterlockedIncrement(&g_nDisconnects);
    /*
    if ( VMPI_GetCurrentNumberOfConnections() - g_nDisconnects <= 1 )
    {
            Error( "All machines disconnected!" );
    }
    */
  } else {
    VMPI_HandleAutoRestart();

    Error("Worker quitting.");
  }

  g_bSuppressPrintfOutput = bOldSuppress;
}
