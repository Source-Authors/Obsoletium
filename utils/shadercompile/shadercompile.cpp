// Copyright Valve Corporation, All rights reserved.
//
//

#include "shadercompile.h"

#include "winlite.h"
#include <conio.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <memory>
#include <system_error>

#include "vmpi.h"
#include "filesystem.h"
#include "vmpi_filesystem.h"
#include "vmpi_distribute_work.h"
#include "vmpi_tools_shared.h"
#include "tier0/icommandline.h"
#include "tier0/dbg.h"
#include "tier0/minidump.h"
#include "tier0/tslist.h"
#include "tier1/utlstring.h"
#include "tier1/utlbinaryblock.h"
#include "tier1/UtlStringMap.h"
#include "tier1/checksum_crc.h"
#include "tier1/strtools.h"
#include "tier2/utlstreambuffer.h"
#include "threads.h"
#include "ishadercompiledll.h"
#include "materialsystem/shader_vcs_version.h"
#include "ilaunchabledll.h"
#include "utlnodehash.h"
#include "lzma/lzma.h"
#include "tools_minidump.h"

#include "cmdlib.h"
#include "cmdsink.h"
#include "d3dxfxc.h"
#include "subprocess.h"
#include "cfgprocessor.h"

// Type conversions should be controlled by programmer explicitly -
// shadercompile makes use of 64-bit integer arithmetics #pragma warning( error
// : 4244 )

static inline uint32 uint64_as_uint32(uint64_t x) {
  Assert(x < uint64_t(uint32(~0)));
  return uint32(x);
}

static inline UtlSymId_t int_as_symid(int x) {
  Assert((sizeof(UtlSymId_t) >= sizeof(int)) ||
         (x >= 0 && x < (int)(unsigned int)(UtlSymId_t(~0))));
  return UtlSymId_t(x);
}

// VMPI packets
#define STARTWORK_PACKETID 5
#define WORKUNIT_PACKETID 6
#define ERRMSG_PACKETID 7
#define SHADERHADERROR_PACKETID 8
#define MACHINE_NAME 9

#ifdef _DEBUG
// #define DEBUGFP
#endif

// Dealing with job list
namespace {

std::unique_ptr<se::shader_compile::shader_combo_processor::CfgEntryInfo[]>
    g_arrCompileEntries;
uint64_t g_numShaders = 0, g_numCompileCommands = 0, g_numStaticCombos = 0,
         g_numDynamicCombos = 0;
uint64_t g_nStaticCombosPerWorkUnit = 0, g_numCompletedStaticCombos = 0,
         g_numCommandsCompleted = 0;
uint64_t g_numSkippedStaticCombos = 0;

se::shader_compile::shader_combo_processor::CfgEntryInfo const *
GetEntryByStaticComboNum(uint64_t nStaticCombo, uint64_t *pnStaticCombo) {
  se::shader_compile::shader_combo_processor::CfgEntryInfo const *pInfo;
  uint64_t nRemainStaticCombos = nStaticCombo;

  for (pInfo = g_arrCompileEntries.get(); pInfo && pInfo->m_szName; ++pInfo) {
    if (nRemainStaticCombos >= pInfo->m_numStaticCombos)
      nRemainStaticCombos -= pInfo->m_numStaticCombos;
    else
      break;
  }

  if (pnStaticCombo) *pnStaticCombo = nRemainStaticCombos;

  return pInfo;
}

};  // namespace

char *PrettyPrintNumber(uint64_t k) {
  static char chCompileString[50] = {0};
  char *pchPrint = chCompileString + sizeof(chCompileString) - 3;
  for (uint64_t j = 0; k > 0; k /= 10, ++j) {
    (j && !(j % 3)) ? (*pchPrint-- = ',') : 0;
    *pchPrint-- = '0' + char(k % 10);
  }
  (*++pchPrint) ? 0 : (*pchPrint = 0);
  return pchPrint;
}

const char *g_pShaderPath = NULL;
char g_WorkerTempPath[MAX_PATH];
char g_ExeDir[MAX_PATH];
#ifdef DEBUGFP
FILE *g_WorkerDebugFp = NULL;
#endif
bool g_bGotStartWorkPacket = false;
double g_flStartTime;
bool g_bVerbose = false;
bool g_bSuppressWarnings = false;

FORCEINLINE long AsTargetLong(long x) { return x; }

struct ShaderInfo_t {
  ShaderInfo_t() { memset(this, 0, sizeof(*this)); }

  uint64_t m_nShaderCombo;
  uint64_t m_nTotalShaderCombos;
  const char *m_pShaderName;
  const char *m_pShaderSrc;
  unsigned m_CentroidMask;
  uint64_t m_nDynamicCombos;
  uint64_t m_nStaticCombo;
  unsigned m_Flags;  // from IShader.h
  char m_szShaderModel[12];
};

void Shader_ParseShaderInfoFromCompileCommands(
    se::shader_compile::shader_combo_processor::CfgEntryInfo const *pEntry,
    ShaderInfo_t &shaderInfo);

struct CByteCodeBlock {
  CByteCodeBlock *m_pNext, *m_pPrev;
  int m_nCRC32;
  uint64_t m_nComboID;
  size_t m_nCodeSize;
  uint8 *m_ByteCode;

  CByteCodeBlock() : m_pNext{nullptr}, m_pPrev{nullptr} {
    m_nCRC32 = 0;
    m_nComboID = 0;
    m_nCodeSize = 0;
    m_ByteCode = NULL;
  }

  CByteCodeBlock(void const *pByteCode, size_t nCodeSize, uint64_t nComboID) {
    m_ByteCode = new uint8[nCodeSize];
    m_nComboID = nComboID;
    m_nCodeSize = nCodeSize;
    memcpy(m_ByteCode, pByteCode, nCodeSize);
    m_nCRC32 = CRC32_ProcessSingleBuffer(pByteCode, nCodeSize);
  }

  ~CByteCodeBlock() { delete[] m_ByteCode; }
};

static int __cdecl CompareDynamicComboIDs(CByteCodeBlock *const *pA,
                                          CByteCodeBlock *const *pB) {
  if ((*pA)->m_nComboID < (*pB)->m_nComboID) return -1;
  if ((*pA)->m_nComboID > (*pB)->m_nComboID) return 1;
  return 0;
}

struct CStaticCombo  // all the data for one static combo
{
  CStaticCombo *m_pNext, *m_pPrev;

  uint64_t m_nStaticComboID;

  CUtlVector<CByteCodeBlock *> m_DynamicCombos;

  struct PackedCode : protected CArrayAutoPtr<uint8> {
    size_t GetLength() const {
      if (uint8 *pb = Get())
        return *reinterpret_cast<size_t *>(pb);
      else
        return 0;
    }
    uint8 *GetData() const {
      if (uint8 *pb = Get())
        return pb + sizeof(size_t);
      else
        return NULL;
    }
    uint8 *AllocData(size_t len) {
      Delete();
      if (len) {
        Attach(new uint8[len + sizeof(size_t)]);
        *reinterpret_cast<size_t *>(Get()) = len;
      }
      return GetData();
    }
  } m_abPackedCode;  // Packed code for entire static combo

  uint64_t Key() const { return m_nStaticComboID; }

  explicit CStaticCombo(uint64_t nComboID)
      : m_pNext{nullptr}, m_pPrev{nullptr} {
    m_nStaticComboID = nComboID;
  }

  ~CStaticCombo() { m_DynamicCombos.PurgeAndDeleteElements(); }

  void AddDynamicCombo(uint64_t nComboID, void const *pComboData,
                       size_t nCodeSize) {
    CByteCodeBlock *pNewBlock =
        new CByteCodeBlock(pComboData, nCodeSize, nComboID);
    m_DynamicCombos.AddToTail(pNewBlock);
  }

  void SortDynamicCombos(void) { m_DynamicCombos.Sort(CompareDynamicComboIDs); }

  uint8 *AllocPackedCodeBlock(size_t nPackedCodeSize) {
    return m_abPackedCode.AllocData(nPackedCodeSize);
  }
};

typedef CUtlNodeHash<CStaticCombo, 7097, uint64_t> StaticComboNodeHash_t;

template <>
inline StaticComboNodeHash_t **Construct(StaticComboNodeHash_t **pMemory) {
  return ::new (pMemory)
      StaticComboNodeHash_t *(NULL);  // Explicitly new with NULL
}

struct CShaderMap : public CUtlStringMap<StaticComboNodeHash_t *> {
  ;
} g_ShaderByteCode;

CStaticCombo *StaticComboFromDictAdd(char const *pszShaderName,
                                     uint64_t nStaticComboId) {
  StaticComboNodeHash_t *&rpNodeHash = g_ShaderByteCode[pszShaderName];
  if (!rpNodeHash) {
    rpNodeHash = new StaticComboNodeHash_t;
  }

  // search for this static combo. make it if not found
  CStaticCombo *pStaticCombo = rpNodeHash->FindByKey(nStaticComboId);
  if (!pStaticCombo) {
    pStaticCombo = new CStaticCombo(nStaticComboId);
    rpNodeHash->Add(pStaticCombo);
  }

  return pStaticCombo;
}

CStaticCombo *StaticComboFromDict(char const *pszShaderName,
                                  uint64_t nStaticComboId) {
  if (StaticComboNodeHash_t *pNodeHash = g_ShaderByteCode[pszShaderName])
    return pNodeHash->FindByKey(nStaticComboId);
  else
    return NULL;
}

CUtlStringMap<ShaderInfo_t> g_ShaderToShaderInfo;

class CompilerMsgInfo {
 public:
  CompilerMsgInfo() : m_numTimesReported(0) {}

 public:
  void SetMsgReportedCommand(char const *szCommand, int numTimesReported = 1) {
    if (!m_numTimesReported) m_sFirstCommand = szCommand;
    m_numTimesReported += numTimesReported;
  }

 public:
  char const *GetFirstCommand() const { return m_sFirstCommand.String(); }
  int GetNumTimesReported() const { return m_numTimesReported; }

 protected:
  CUtlString m_sFirstCommand;
  int m_numTimesReported;
};

CUtlStringMap<bool> g_Master_ShaderHadError;
CUtlStringMap<bool> g_Master_ShaderWrittenToDisk;
CUtlStringMap<CompilerMsgInfo> g_Master_CompilerMsgInfo;

namespace Threading {

enum Mode { eSingleThreaded = 0, eMultiThreaded = 1 };

// A special object that makes single-threaded code incur no penalties
// and multithreaded code to be synchronized properly.
template <class MT_MUTEX_TYPE = CThreadFastMutex>
class CSwitchableMutex {
 public:
 public:
  FORCEINLINE explicit CSwitchableMutex(Mode eMode,
                                        MT_MUTEX_TYPE *pMtMutex = NULL)
      : m_pMtx(pMtMutex), m_pUseMtx(eMode ? pMtMutex : NULL) {}

 public:
  FORCEINLINE void SetMtMutex(MT_MUTEX_TYPE *pMtMutex) {
    m_pMtx = pMtMutex;
    m_pUseMtx = (m_pUseMtx ? pMtMutex : NULL);
  }
  FORCEINLINE void SetThreadedMode(Mode eMode) {
    m_pUseMtx = (eMode ? m_pMtx : NULL);
  }

 public:
  FORCEINLINE void Lock() {
    if (MT_MUTEX_TYPE *pUseMtx = m_pUseMtx) pUseMtx->Lock();
  }
  FORCEINLINE void Unlock() {
    if (MT_MUTEX_TYPE *pUseMtx = m_pUseMtx) pUseMtx->Unlock();
  }

  FORCEINLINE bool TryLock() {
    if (MT_MUTEX_TYPE *pUseMtx = m_pUseMtx)
      return pUseMtx->TryLock();
    else
      return true;
  }
  FORCEINLINE bool AssertOwnedByCurrentThread() {
    if (MT_MUTEX_TYPE *pUseMtx = m_pUseMtx)
      return pUseMtx->AssertOwnedByCurrentThread();
    else
      return true;
  }
  FORCEINLINE void SetTrace(bool b) {
    if (MT_MUTEX_TYPE *pUseMtx = m_pUseMtx) pUseMtx->SetTrace(b);
  }

  FORCEINLINE uint32 GetOwnerId() {
    if (MT_MUTEX_TYPE *pUseMtx = m_pUseMtx)
      return pUseMtx->GetOwnerId();
    else
      return 0;
  }
  FORCEINLINE int GetDepth() {
    if (MT_MUTEX_TYPE *pUseMtx = m_pUseMtx)
      return pUseMtx->GetDepth();
    else
      return 0;
  }

 private:
  MT_MUTEX_TYPE *m_pMtx;
  CInterlockedPtr<MT_MUTEX_TYPE> m_pUseMtx;
};

namespace Private {

typedef CThreadMutex MtMutexType_t;
MtMutexType_t g_mtxSyncObjMT;

};  // namespace Private

CSwitchableMutex<Private::MtMutexType_t> g_mtxGlobal(eSingleThreaded,
                                                     &Private::g_mtxSyncObjMT);

class CGlobalMutexAutoLock {
 public:
  CGlobalMutexAutoLock() { g_mtxGlobal.Lock(); }
  ~CGlobalMutexAutoLock() { g_mtxGlobal.Unlock(); }
};

};  // namespace Threading

// Access to global data should be synchronized by these global locks
#define GLOBAL_DATA_MTX_LOCK() Threading::g_mtxGlobal.Lock()
#define GLOBAL_DATA_MTX_UNLOCK() Threading::g_mtxGlobal.Unlock()
#define GLOBAL_DATA_MTX_LOCK_AUTO Threading::CGlobalMutexAutoLock UNIQUE_ID;

CDispatchReg g_DistributeWorkReg(WORKUNIT_PACKETID, DistributeWorkDispatch);

unsigned long VMPI_Stats_GetJobWorkerID() { return 0; }

bool StartWorkDispatch(MessageBuffer *pBuf, int iSource, int iPacketID) {
  g_bGotStartWorkPacket = true;
  return true;
}

CDispatchReg g_StartWorkReg(STARTWORK_PACKETID, StartWorkDispatch);

// Consume all characters for which (isspace) is true
template <typename T>
char *ConsumeCharacters(char *szString, T pred) {
  if (szString) {
    while (*szString && pred(*szString)) {
      ++szString;
    }
  }

  return szString;
}

char *FindNext(char *szString, char *szSearchSet) {
  bool bFound = (szString == NULL);
  char *szNext = NULL;

  if (szString && szSearchSet) {
    for (; *szSearchSet; ++szSearchSet) {
      if (char *szTmp = strchr(szString, *szSearchSet)) {
        szNext = bFound ? (min(szNext, szTmp)) : szTmp;
        bFound = true;
      }
    }
  }

  return bFound ? szNext : (szString + strlen(szString));
}

char *FindLast(char *szString, char *szSearchSet) {
  bool bFound = (szString != NULL);
  char *szNext = NULL;

  if (szString && szSearchSet) {
    for (; *szSearchSet; ++szSearchSet) {
      if (char *szTmp = strrchr(szString, *szSearchSet)) {
        szNext = bFound ? (max(szNext, szTmp)) : szTmp;
        bFound = true;
      }
    }
  }

  return bFound ? szNext : (szString + strlen(szString));
}

void ErrMsgDispatchMsgLine(char const *szCommand, char *szMsgLine,
                           char const *szShaderName = NULL) {
  // When the filename is specified in front of the message, make sure it is
  // truncated to the bare name only
  if (V_isalpha(*szMsgLine) && szMsgLine[1] == ':') {
    // Preceded by drive letter
    szMsgLine += 2;
  }

  // Trim the path from the msg
  // e.g. make string
  //    c:\temp\shadercompiletemp\1234\myfile.fxc(435): warning X3083:
  //    Truncating ...
  // look like
  //    myfile.fxc(435): warning X3083: Truncating ...
  // which will be both readable and same coming from different worker machines
  char *szEndFileLinePlant = FindNext(szMsgLine, ":");
  if (':' == *szEndFileLinePlant) {
    *szEndFileLinePlant = 0;
    if (char *szLastSlash = FindLast(szMsgLine, "\\/")) {
      if (*szLastSlash) {
        *szLastSlash = 0;
        szMsgLine = szLastSlash + 1;
      }
    }
    *szEndFileLinePlant = ':';
  }

  // If the shader file name is not given in the message add it
  if (szShaderName) {
    static char chFitLongMsgLine[4096];

    if (*szMsgLine == '(') {
      sprintf(chFitLongMsgLine, "%s%s", szShaderName, szMsgLine);
      szMsgLine = chFitLongMsgLine;
    } else if (!strncmp(szMsgLine, "memory(", 7)) {
      sprintf(chFitLongMsgLine, "%s%s", szShaderName, szMsgLine + 6);
      szMsgLine = chFitLongMsgLine;
    }
  }

  // Now store the message with the command it was generated from
  g_Master_CompilerMsgInfo[szMsgLine].SetMsgReportedCommand(szCommand);
}

void ErrMsgDispatchInt(char *szMessage, char const *szShaderName = NULL) {
  // First line is the command number "szCommand"
  char *szCommand = ConsumeCharacters(szMessage, isspace);
  char *szMessageListing = FindNext(szCommand, "\r\n");
  char chTerminator = *szMessageListing;
  *(szMessageListing++) = 0;

  // Now come the command lines actually
  while (chTerminator) {
    char *szMsgText = ConsumeCharacters(szMessageListing, isspace);
    szMessageListing = FindNext(szMsgText, "\r\n");
    chTerminator = *szMessageListing;
    *(szMessageListing++) = 0;

    if (*szMsgText) {
      // Trim command at redirection character if present
      *FindNext(szCommand, ">") = '\0';
      ErrMsgDispatchMsgLine(szCommand, szMsgText, szShaderName);
    }
  }
}

//
//	BUFFER:
//			1 byte = *			= buffer type
//
//			string				= message
//			1 byte = \n			= newline delimiting the
// message
//
//			string				= command that first
// encountered the message 			1 byte = \n
// = newline delimiting the command
//
//			string				= printed number of
// times the message was encountered 			1 byte = \n
// = newline delimiting the number
//
//			1 byte = 0			= null-terminator for
// the buffer
//
bool ErrMsgDispatch(MessageBuffer *pBuf, int iSource, int iPacketID) {
  GLOBAL_DATA_MTX_LOCK_AUTO;

  bool bInvalidPkgRetCode = true;

  // Parse the err msg packet
  char *szMsgLine = pBuf->data + 1;

  char *szCommand = FindNext(szMsgLine, "\n");
  if (!*szCommand) return bInvalidPkgRetCode;
  *(szCommand++) = 0;

  char *szNumTimesReported = FindNext(szCommand, "\n");
  if (!*szNumTimesReported) return bInvalidPkgRetCode;
  *(szNumTimesReported++) = 0;

  char *szTerminator = FindNext(szNumTimesReported, "\n");
  if (!*szTerminator) return bInvalidPkgRetCode;
  *(szTerminator++) = 0;

  // Set the msg info
  g_Master_CompilerMsgInfo[szMsgLine].SetMsgReportedCommand(
      szCommand, atoi(szNumTimesReported));

  return true;
}

CDispatchReg g_ErrMsgReg(ERRMSG_PACKETID, ErrMsgDispatch);

void ShaderHadErrorDispatchInt(char const *szShader) {
  g_Master_ShaderHadError[szShader] = true;
}

//
//	BUFFER:
//			1 byte = *			= buffer type
//
//			string				= shader name
//			1 byte = 0			= null-terminator for
// the name
//
bool ShaderHadErrorDispatch(MessageBuffer *pBuf, int iSource, int iPacketID) {
  GLOBAL_DATA_MTX_LOCK_AUTO;

  ShaderHadErrorDispatchInt(pBuf->data + 1);
  return true;
}

CDispatchReg g_ShaderHadErrorReg(SHADERHADERROR_PACKETID,
                                 ShaderHadErrorDispatch);

void DebugOut(const char *pMsg, ...) {
  if (g_bVerbose) {
    char msg[2048];
    va_list marker;
    va_start(marker, pMsg);
    _vsnprintf(msg, sizeof(msg), pMsg, marker);
    va_end(marker);

    Msg("%s", msg);

#ifdef DEBUGFP
    fprintf(g_WorkerDebugFp, "%s", msg);
    fflush(g_WorkerDebugFp);
#endif
  }
}

void Vmpi_Worker_DefaultDisconnectHandler(int procID, const char *pReason) {
  Msg("Master disconnected.\n ");
  DebugOut("Master disconnected.\n");
  ExitProcess(1);
}

typedef void (*DisconnectHandlerFn_t)(int procID, const char *pReason);
DisconnectHandlerFn_t g_fnDisconnectHandler =
    Vmpi_Worker_DefaultDisconnectHandler;

// Worker should implement this so it will quit nicely when the master
// disconnects.
void MyDisconnectHandler(int procID, const char *pReason) {
  // If we're a worker, then it's a fatal error if we lose the connection to the
  // master.
  if (!g_bMPIMaster && g_fnDisconnectHandler) {
    (*g_fnDisconnectHandler)(procID, pReason);
  }
}

// new format:
// ver#
// total shader combos
// total dynamic combos
// flags
// centroid mask
// total non-skipped static combos
// [ (sorted by static combo id)
//   static combo id
//   file offset of packed dynamic combo
// ]
// 0xffffffff  (sentinel key)
// end of file offset (so can tell compressed size of last combo)
//
// # of duplicate static combos  (if version >= 6 )
// [ (sorted by static combo id)
//   static combo id
//   id of static bombo which is identical
// ]
//
// each packed dynamic combo for a given static combo is stored as a series of
// compressed blocks.
//  block 1:
//     ulong blocksize  (high bit set means uncompressed)
//     block data
//  block2..
//  0xffffffff  indicates no more blocks for this combo
//
// each block, when uncompressed, holds one or more dynamic combos:
//   dynamic combo id   (full id if v<6, dynamic combo id only id >=6)
//   size of shader
//   ..
// there is no terminator - the size of the uncompressed shader tells you when
// to stop

// this record is then bzip2'd.

// qsort driver function
// returns negative number if idA is less than idB, positive when idA is greater
// than idB and zero if the ids are equal

static int __cdecl CompareDupComboIndices(const StaticComboAliasRecord_t *pA,
                                          const StaticComboAliasRecord_t *pB) {
  if (pA->m_nStaticComboID < pB->m_nStaticComboID) return -1;
  if (pA->m_nStaticComboID > pB->m_nStaticComboID) return 1;
  return 0;
}

static void FlushCombos(size_t *pnTotalFlushedSize,
                        CUtlBuffer *pDynamicComboBuffer, MessageBuffer *pBuf) {
  if (!pDynamicComboBuffer->TellPut())
    // Nothing to do here
    return;

  unsigned nCompressedSize;
  uint8 *pCompressedShader = LZMA_OpportunisticCompress(
      pDynamicComboBuffer->Base<uint8>(), pDynamicComboBuffer->TellPut(),
      &nCompressedSize);
  // high 2 bits of length =
  // 00 = bzip2 compressed
  // 10 = uncompressed
  // 01 = lzma compressed
  // 11 = unused

  if (!pCompressedShader) {
    // it grew
    long lFlagSize = AsTargetLong(0x80000000 | pDynamicComboBuffer->TellPut());
    pBuf->write(&lFlagSize, sizeof(lFlagSize));
    pBuf->write(pDynamicComboBuffer->Base(), pDynamicComboBuffer->TellPut());
    *pnTotalFlushedSize += sizeof(lFlagSize) + pDynamicComboBuffer->TellPut();
  } else {
    long lFlagSize = AsTargetLong(0x40000000 | nCompressedSize);
    pBuf->write(&lFlagSize, sizeof(lFlagSize));
    pBuf->write(pCompressedShader, nCompressedSize);
    delete[] pCompressedShader;
    *pnTotalFlushedSize += sizeof(lFlagSize) + nCompressedSize;
  }
  pDynamicComboBuffer->Clear();  // start over
}

static void OutputDynamicCombo(size_t *pnTotalFlushedSize,
                               CUtlBuffer *pDynamicComboBuffer,
                               MessageBuffer *pBuf, uint64_t nComboID,
                               int nComboSize, uint8 *pComboCode) {
  if (pDynamicComboBuffer->TellPut() + nComboSize + 16 >=
      MAX_SHADER_UNPACKED_BLOCK_SIZE) {
    FlushCombos(pnTotalFlushedSize, pDynamicComboBuffer, pBuf);
  }

  pDynamicComboBuffer->PutInt(uint64_as_uint32(nComboID));
  pDynamicComboBuffer->PutInt(nComboSize);
  //	pDynamicComboBuffer->PutInt( CRC32_ProcessSingleBuffer( pComboCode,
  // nComboSize ) );
  pDynamicComboBuffer->Put(pComboCode, nComboSize);
}

#if 0
static void OutputDynamicComboDup(size_t *pnTotalFlushedSize,
                                  CUtlBuffer *pDynamicComboBuffer,
                                  MessageBuffer *pBuf, uint64_t nComboID,
                                  uint64_t nBaseCombo) {
  if (pDynamicComboBuffer->TellPut() + 8 >= MAX_SHADER_UNPACKED_BLOCK_SIZE) {
    FlushCombos(pnTotalFlushedSize, pDynamicComboBuffer, pBuf);
  }
  pDynamicComboBuffer->PutInt(uint64_as_uint32(nComboID) | 0x80000000);
  pDynamicComboBuffer->PutInt(uint64_as_uint32(nBaseCombo));
}
#endif

void GetVCSFilenames(char *pszMainOutFileName, ShaderInfo_t const &si) {
  sprintf(pszMainOutFileName, "%s\\shaders\\fxc", g_pShaderPath);

  struct _stat buf;
  if (_stat(pszMainOutFileName, &buf) == -1) {
    printf("mkdir %s\n", pszMainOutFileName);
    // doh. . need to make the directory that the vcs file is going to go into.
    _mkdir(pszMainOutFileName);
  }

  strcat(pszMainOutFileName, "\\");
  strcat(pszMainOutFileName, si.m_pShaderName);
  strcat(pszMainOutFileName,
         ".vcs");  // Different extensions for main output file

  // Check status of vcs file...
  if (_stat(pszMainOutFileName, &buf) != -1) {
    // The file exists, let's see if it's writable.
    if (!(buf.st_mode & _S_IWRITE)) {
      // It isn't writable. . we'd better change its permissions (or check it
      // out possibly)
      printf("Warning: making %s writable!\n", pszMainOutFileName);
      _chmod(pszMainOutFileName, _S_IREAD | _S_IWRITE);
    }
  }
}

// WriteShaderFiles
//
// should be called either on the main thread or
// on the async writing thread.
//
// So the function WriteShaderFiles should not be reentrant, however the
// data that it uses might be updated by the main thread when built pieces
// are received from the workers.
//
#define STATIC_COMBO_HASH_SIZE 73

struct StaticComboAuxInfo_t : StaticComboRecord_t {
  uint32 m_nCRC32;  // CRC32 of packed data
  struct CStaticCombo *m_pByteCode;
};

static int __cdecl CompareComboIds(const StaticComboAuxInfo_t *pA,
                                   const StaticComboAuxInfo_t *pB) {
  if (pA->m_nStaticComboID < pB->m_nStaticComboID) return -1;
  if (pA->m_nStaticComboID > pB->m_nStaticComboID) return 1;
  return 0;
}

static void WriteShaderFiles(const char *pShaderName) {
  if (!g_Master_ShaderWrittenToDisk.Defined(pShaderName))
    g_Master_ShaderWrittenToDisk[pShaderName] = true;
  else
    return;

  bool bShaderFailed = g_Master_ShaderHadError.Defined(pShaderName);
  char const *szShaderFileOperation =
      bShaderFailed ? "Removing failed" : "Writing";

  //
  // Progress indication
  //
  if (g_numCommandsCompleted < g_numCompileCommands) {
    static char chProgress[] = {'/', '-', '\\', '|'};
    static int iProgressSymbol = 0;

    Msg("\b%c", chProgress[(++iProgressSymbol) % 4]);
  } else {
    char chShaderName[33];
    Q_snprintf(chShaderName, 29, "%s...", pShaderName);
    sprintf(chShaderName + sizeof(chShaderName) - 5, "...");

    Msg("\r%s %s   \r", szShaderFileOperation, chShaderName);
  }

  //
  // Retrieve the data we are going to operate on
  // from global variables under lock.
  //
  GLOBAL_DATA_MTX_LOCK();
  StaticComboNodeHash_t *pByteCodeArray;
  {
    // Get a static combo pointer, reset it as well
    StaticComboNodeHash_t *&rp = g_ShaderByteCode[pShaderName];
    pByteCodeArray = rp;
    rp = NULL;
  }

  ShaderInfo_t shaderInfo = g_ShaderToShaderInfo[pShaderName];
  if (!shaderInfo.m_pShaderName) {
    for (auto *pAnalyze = g_arrCompileEntries.get(); pAnalyze->m_szName;
         ++pAnalyze) {
      if (!strcmp(pAnalyze->m_szName, pShaderName)) {
        Shader_ParseShaderInfoFromCompileCommands(pAnalyze, shaderInfo);
        g_ShaderToShaderInfo[pShaderName] = shaderInfo;
        break;
      }
    }
  }
  GLOBAL_DATA_MTX_UNLOCK();

  if (!shaderInfo.m_pShaderName) return;

  //
  // Shader vcs file name
  //
  char szVCSfilename[MAX_PATH];
  GetVCSFilenames(szVCSfilename, shaderInfo);

  if (bShaderFailed) {
    DebugOut("Removing failed shader file \"%s\".\n", szVCSfilename);
    unlink(szVCSfilename);
    return;
  }

  if (!pByteCodeArray) return;

  DebugOut(
      "%s : %I64u combos centroid mask: 0x%x numDynamicCombos: %I64u flags: "
      "0x%x\n",
      pShaderName, shaderInfo.m_nTotalShaderCombos, shaderInfo.m_CentroidMask,
      shaderInfo.m_nDynamicCombos, shaderInfo.m_Flags);

  //
  // Static combo headers
  //
  CUtlVector<StaticComboAuxInfo_t> StaticComboHeaders;

  // we know how much ram we need
  StaticComboHeaders.EnsureCapacity(1 + pByteCodeArray->Count());

  CUtlVector<int> comboIndicesHashedByCRC32[STATIC_COMBO_HASH_SIZE];
  CUtlVector<StaticComboAliasRecord_t> duplicateCombos;

  // now, lets fill in our combo headers, sort, and write
  for (intp nChain = 0; nChain < ssize(pByteCodeArray->m_HashChains);
       nChain++) {
    for (CStaticCombo *pStatic = pByteCodeArray->m_HashChains[nChain].m_pHead;
         pStatic; pStatic = pStatic->m_pNext) {
      if (pStatic->m_abPackedCode.GetLength()) {
        StaticComboAuxInfo_t Hdr = {};
        Hdr.m_nStaticComboID = uint64_as_uint32(pStatic->m_nStaticComboID);
        Hdr.m_nFileOffset = 0;  // fill in later
        Hdr.m_nCRC32 =
            CRC32_ProcessSingleBuffer(pStatic->m_abPackedCode.GetData(),
                                      pStatic->m_abPackedCode.GetLength());
        int nHashIdx = Hdr.m_nCRC32 % STATIC_COMBO_HASH_SIZE;
        Hdr.m_pByteCode = pStatic;
        // now, see if we have an identical static combo
        bool bIsDuplicate = false;
        for (auto &crc : comboIndicesHashedByCRC32[nHashIdx]) {
          StaticComboAuxInfo_t const &check = StaticComboHeaders[crc];
          if ((check.m_nCRC32 == Hdr.m_nCRC32) &&
              (check.m_pByteCode->m_abPackedCode.GetLength() ==
               pStatic->m_abPackedCode.GetLength()) &&
              (memcmp(check.m_pByteCode->m_abPackedCode.GetData(),
                      pStatic->m_abPackedCode.GetData(),
                      check.m_pByteCode->m_abPackedCode.GetLength()) == 0)) {
            // this static combo is the same as another one!!
            StaticComboAliasRecord_t aliasHdr = {};
            aliasHdr.m_nStaticComboID = Hdr.m_nStaticComboID;
            aliasHdr.m_nSourceStaticCombo = check.m_nStaticComboID;
            duplicateCombos.AddToTail(aliasHdr);
            bIsDuplicate = true;

            break;
          }
        }

        if (!bIsDuplicate) {
          StaticComboHeaders.AddToTail(Hdr);
          comboIndicesHashedByCRC32[nHashIdx].AddToTail(
              StaticComboHeaders.Count() - 1);
        }
      }
    }
  }
  // add sentinel key
  StaticComboAuxInfo_t Hdr = {};
  Hdr.m_nStaticComboID = 0xffffffff;
  Hdr.m_nFileOffset = 0;
  StaticComboHeaders.AddToTail(Hdr);

  // now, sort. sentinel key will end up at end
  StaticComboHeaders.Sort(CompareComboIds);

  // Set the CRC to zero for now. . will patch in copyshaders.pl with the
  // correct CRC.
  unsigned int crc32 = 0;

  //
  // Shader file stream buffer
  //
  // Streaming buffer for vcs file (since this can blow memory)
  CUtlStreamBuffer ShaderFile(szVCSfilename, NULL);
  ShaderFile.SetBigEndian(false);  // Swap the header bytes to X360 format

  // ------ Header --------------
  ShaderFile.PutInt(SHADER_VCS_VERSION_NUMBER);  // Version
  ShaderFile.PutInt(uint64_as_uint32(shaderInfo.m_nTotalShaderCombos));
  ShaderFile.PutInt(uint64_as_uint32(shaderInfo.m_nDynamicCombos));
  ShaderFile.PutUnsignedInt(shaderInfo.m_Flags);
  ShaderFile.PutUnsignedInt(shaderInfo.m_CentroidMask);
  ShaderFile.PutUnsignedInt(StaticComboHeaders.Count());
  ShaderFile.PutUnsignedInt(crc32);

  // static combo dictionary
  intp nDictionaryOffset = ShaderFile.TellPut();

  // we will re write this one we know the offsets
  // dummy write, 8 bytes per static combo
  ShaderFile.Put(StaticComboHeaders.Base(),
                 sizeof(StaticComboRecord_t) * StaticComboHeaders.Count());

  ShaderFile.PutUnsignedInt(duplicateCombos.Count());
  // now, write out all duplicate header records
  // sort duplicate combo records for binary search
  duplicateCombos.Sort(CompareDupComboIndices);

  for (auto &c : duplicateCombos) {
    ShaderFile.PutUnsignedInt(c.m_nStaticComboID);
    ShaderFile.PutUnsignedInt(c.m_nSourceStaticCombo);
  }

  // now, write out all static combos
  for (auto &SRec : StaticComboHeaders) {
    SRec.m_nFileOffset = ShaderFile.TellPut();
    if (SRec.m_nStaticComboID != 0xffffffff)  // sentinel key?
    {
      CStaticCombo *pStatic = pByteCodeArray->FindByKey(SRec.m_nStaticComboID);
      Assert(pStatic);

      // Put the packed chunk of code for this static combo
      if (size_t nPackedLen = pStatic->m_abPackedCode.GetLength())
        ShaderFile.Put(pStatic->m_abPackedCode.GetData(), nPackedLen);

      ShaderFile.PutInt(0xffffffff);  // end of dynamic combos
    }
  }
  ShaderFile.Close();

  //
  // Re-writing the combo header
  //
  {
    FILE *Handle = fopen(szVCSfilename, "rb+");
    if (!Handle) {
      fprintf(stderr, " failed to re-open %s\n", szVCSfilename);
      exit(1);
    }

    fseek(Handle, nDictionaryOffset, SEEK_SET);

    // now, rewrite header. data is already byte-swapped appropriately
    for (auto &c : StaticComboHeaders) {
      fwrite(&(c.m_nStaticComboID), 4, 1, Handle);
      fwrite(&(c.m_nFileOffset), 4, 1, Handle);
    }

    fclose(Handle);
  }

  // Finalize, free memory
  delete pByteCodeArray;

  if (g_numCommandsCompleted >= g_numCompileCommands) {
    Msg("\r                                                                \r");
  }
}

// pBuf is ready to read the results written to the buffer in ProcessWorkUnitFn.
// work is done. .master gets it back this way.
// compiled code in pBuf
void Master_ReceiveWorkUnitFn(uint64_t iWorkUnit, MessageBuffer *pBuf,
                              int iWorker) {
  GLOBAL_DATA_MTX_LOCK_AUTO;

  uint64_t comboStart = iWorkUnit * g_nStaticCombosPerWorkUnit;
  uint64_t comboEnd = comboStart + g_nStaticCombosPerWorkUnit;
  comboEnd = min(g_numStaticCombos, comboEnd);

  char const *chLastShaderName = "";
  ShaderInfo_t siLastShaderInfo;
  memset(&siLastShaderInfo, 0, sizeof(siLastShaderInfo));
  siLastShaderInfo.m_pShaderName = chLastShaderName;

  uint64_t nComboOfTheEntry = 0;
  se::shader_compile::shader_combo_processor::CfgEntryInfo const *pEntry =
      GetEntryByStaticComboNum(comboStart, &nComboOfTheEntry);
  nComboOfTheEntry = pEntry->m_numStaticCombos - 1 - nComboOfTheEntry;

  for (uint64_t iCombo = comboStart; iCombo++ < comboEnd;
       ((!nComboOfTheEntry--)
            ? (++pEntry, nComboOfTheEntry = pEntry->m_numStaticCombos - 1)
            : 0)) {
    Assert(nComboOfTheEntry < pEntry->m_numStaticCombos);

    // Read length
    int len;
    pBuf->read(&len, sizeof(len));

    // Length can indicate the number of skips to make
    if (len <= 0) {
      // remember how many static combos get skipped
      g_numSkippedStaticCombos += -len;

      // then we skip as instructed
      for (int64 numSkips = -len - 1; numSkips > 0;) {
        if (numSkips <= nComboOfTheEntry) {
          nComboOfTheEntry -= numSkips;
          iCombo += numSkips;
          numSkips = 0;
        } else {
          numSkips -= nComboOfTheEntry + 1;
          iCombo += nComboOfTheEntry + 1;
          ++pEntry;
          nComboOfTheEntry = pEntry->m_numStaticCombos - 1;
        }
      }

      if (iCombo < comboEnd)
        continue;
      else
        break;
    }

    // Shader code arrived
    char const *chShaderName = pEntry->m_szName;

    // If starting new shader remember shader info
    if (chLastShaderName != chShaderName) {
      Shader_ParseShaderInfoFromCompileCommands(pEntry, siLastShaderInfo);

      chLastShaderName = chShaderName;
      g_ShaderToShaderInfo[chLastShaderName] = siLastShaderInfo;
    }

    // Read buffer
    uint8 *pCodeBuffer = StaticComboFromDictAdd(chShaderName, nComboOfTheEntry)
                             ->AllocPackedCodeBlock(len);

    if (pCodeBuffer) pBuf->read(pCodeBuffer, len);
  }
}

//
// A function that will wait for right Ctrl+Alt+Shift to be held down
// simultaneously.  This is useful for debugging short-lived processes and gives
// time for debugger to get attached.
//
void DebugSafeWaitPoint(bool bForceWait) {
  static bool s_bDebuggerAttached =
      (CommandLine()->FindParm("-debugwait") == 0);

  if (bForceWait) {
    s_bDebuggerAttached = false;
  }

  if (!s_bDebuggerAttached) {
    Msg("Waiting for right Ctrl+Alt+Shift to continue...");
    while (!s_bDebuggerAttached) {
      Msg(".");
      Sleep(1000);

      if (short(GetAsyncKeyState(VK_RCONTROL)) < 0 &&
          short(GetAsyncKeyState(VK_RSHIFT)) < 0 &&
          short(GetAsyncKeyState(VK_RMENU)) < 0) {
        s_bDebuggerAttached = true;
      }
    }
    Msg(" ok.\n");
  }
}

// same as "system", but doesn't pop up a window
void MySystem(char const *const pCommand,
              se::shader_compile::command_sink::IResponse **ppResponse) {
  // Trap the command in se::shader_compile::fxc_intercept
  if (se::shader_compile::fxc_intercept::TryExecuteCommand(pCommand,
                                                           ppResponse)) {
    SwitchToThread();
    return;
  }

  unlink("shader.o");

  FILE *batFp = fopen("temp.bat", "w");
  fprintf(batFp, "%s\n", pCommand);
  fclose(batFp);

  STARTUPINFO si{(DWORD)sizeof(si)};
  PROCESS_INFORMATION pi = {};

  // Start the child process.
  if (!CreateProcess(
          NULL,            // No module name (use command line).
          "\"temp.bat\"",  // Command line.
          NULL,            // Process handle not inheritable.
          NULL,            // Thread handle not inheritable.
          FALSE,           // Set handle inheritance to FALSE.
          IDLE_PRIORITY_CLASS | CREATE_NO_WINDOW,  // No creation flags.
          NULL,              // Use parent's environment block.
          g_WorkerTempPath,  // Use parent's starting directory.
          &si,               // Pointer to STARTUPINFO structure.
          &pi)               // Pointer to PROCESS_INFORMATION structure.
  ) {
    Error("Unable to create console process (%s)!\n",
          std::system_category().message(GetLastError()).c_str());
  }

  // Wait until child process exits.
  WaitForSingleObject(pi.hProcess, INFINITE);

  // Close process and thread handles.
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
}

// Assemble a reply package to the master from the compiled bytecode
// return the length of the package.
size_t AssembleWorkerReplyPackage(
    se::shader_compile::shader_combo_processor::CfgEntryInfo const *pEntry,
    uint64_t nComboOfEntry, MessageBuffer *pBuf) {
  GLOBAL_DATA_MTX_LOCK();
  CStaticCombo *pStComboRec =
      StaticComboFromDict(pEntry->m_szName, nComboOfEntry);
  StaticComboNodeHash_t *pByteCodeArray = g_ShaderByteCode[pEntry->m_szName];
  GLOBAL_DATA_MTX_UNLOCK();

  size_t nBytesWritten = 0;

  if (pStComboRec && pStComboRec->m_DynamicCombos.Count()) {
    CUtlBuffer ubDynamicComboBuffer;
    ubDynamicComboBuffer.SetBigEndian(false);

    pStComboRec->SortDynamicCombos();
    // iterate over all dynamic combos.
    for (int i = 0; i < pStComboRec->m_DynamicCombos.Count(); i++) {
      CByteCodeBlock *pCode = pStComboRec->m_DynamicCombos[i];
      // check if we have already output an identical combo
      bool bDup = false;
#if 0
			// check for duplicate bytecode. actually doesn't save much because bzip does a good
			// job compressing dupes.
			for( int j = 0; j < i; j++ )
			{
				if (
					( pCode->m_nCRC32 == pStComboRec->m_DynamicCombos[j]->m_nCRC32 ) &&
					( pCode->m_nCodeSize == pStComboRec->m_DynamicCombos[j]->m_nCodeSize ) &&
					( memcmp( pCode->m_ByteCode, pStComboRec->m_DynamicCombos[i]->m_ByteCode, pCode->m_nCodeSize ) == 0 )
					)										// identical bytecode?
				{
					bDup = true;
					OutputDynamicComboDup( &nBytesWritten, &ubDynamicComboBuffer,
										   pBuf, pCode->m_nComboID,
										   pStComboRec->m_DynamicCombos[j]->m_nComboID );
				}
			}
#endif
      if (!bDup)
        OutputDynamicCombo(&nBytesWritten, &ubDynamicComboBuffer, pBuf,
                           pCode->m_nComboID, pCode->m_nCodeSize,
                           pCode->m_ByteCode);
    }
    FlushCombos(&nBytesWritten, &ubDynamicComboBuffer, pBuf);
  }

  // Time to limit amount of prints
  static float s_fLastInfoTime = 0;
  float fCurTime = (float)Plat_FloatTime();

  GLOBAL_DATA_MTX_LOCK();
  if (pStComboRec) pByteCodeArray->DeleteByKey(nComboOfEntry);
  if (fabs(fCurTime - s_fLastInfoTime) > 1.f) {
    Msg("\rCompiling  %s  [ %2llu remaining ] ...         \r", pEntry->m_szName,
        nComboOfEntry);
    s_fLastInfoTime = fCurTime;
  }
  GLOBAL_DATA_MTX_UNLOCK();

  return nBytesWritten;
}

// Copy a reply package to the master from the compiled bytecode
// return the length of the data copied.
size_t CopyWorkerReplyPackage(
    se::shader_compile::shader_combo_processor::CfgEntryInfo const *pEntry,
    uint64_t nComboOfEntry, MessageBuffer *pBuf, int nSkipsSoFar) {
  GLOBAL_DATA_MTX_LOCK();
  CStaticCombo *pStComboRec =
      StaticComboFromDict(pEntry->m_szName, nComboOfEntry);
  StaticComboNodeHash_t *pByteCodeArray =
      g_ShaderByteCode[pEntry->m_szName];  // Get a static combo pointer
  GLOBAL_DATA_MTX_UNLOCK();

  int len = pStComboRec ? pStComboRec->m_abPackedCode.GetLength() : NULL;

  if (len) {
    if (nSkipsSoFar) {
      pBuf->write(&nSkipsSoFar, sizeof(nSkipsSoFar));
    }

    pBuf->write(&len, sizeof(len));
    if (len) pBuf->write(pStComboRec->m_abPackedCode.GetData(), len);
  }

  if (pStComboRec) {
    GLOBAL_DATA_MTX_LOCK();
    pByteCodeArray->DeleteByKey(nComboOfEntry);
    GLOBAL_DATA_MTX_UNLOCK();
  }

  return size_t(len);
}

template <typename TMutexType>
class CWorkerAccumState
    : public CParallelProcessorBase<CWorkerAccumState<TMutexType>> {
  friend class ThisParallelProcessorBase_t;

 private:
  static bool &DisconnectState() {
    static bool sb = false;
    return sb;
  }
  static void Special_DisconnectHandler(int procID, const char *pReason) {
    DisconnectState() = true;
  }

 public:
  explicit CWorkerAccumState(TMutexType *pMutex)
      : m_pMutex(pMutex),
        m_iFirstCommand(0),
        m_iNextCommand(0),
        m_iEndCommand(0),
        m_iLastFinished(0),
        m_hCombo(NULL),
        m_fnOldDisconnectHandler(g_fnDisconnectHandler),
        m_autoRestoreDisconnectHandler(g_fnDisconnectHandler) {
    DisconnectState() = false;
  }
  ~CWorkerAccumState() { QuitSubs(); }

  void RangeBegin(uint64_t iFirstCommand, uint64_t iEndCommand);
  void RangeFinished(void);

  void ExecuteCompileCommand(
      se::shader_compile::shader_combo_processor::ComboHandle hCombo);
  void ExecuteCompileCommandThreaded(
      se::shader_compile::shader_combo_processor::ComboHandle hCombo);
  void HandleCommandResponse(
      se::shader_compile::shader_combo_processor::ComboHandle hCombo,
      se::shader_compile::command_sink::IResponse *pResponse);

 public:
  using ThisParallelProcessorBase_t::Run;

 public:
  bool OnProcess();
  bool OnProcessST();

 protected:
  TMutexType *m_pMutex;

 protected:
  struct SubProcess {
    intp dwIndex;
    DWORD dwSvcThreadId;
    uint64_t iRunningCommand;
    PROCESS_INFORMATION pi;
    SubProcessKernelObjects *pCommObjs;
  };
  CThreadLocal<SubProcess *> m_lpSubProcessInfo;
  CUtlVector<SubProcess *> m_arrSubProcessInfos;
  uint64_t m_iFirstCommand;
  uint64_t m_iNextCommand;
  uint64_t m_iEndCommand;

  uint64_t m_iLastFinished;

  se::shader_compile::shader_combo_processor::ComboHandle m_hCombo;

  DisconnectHandlerFn_t m_fnOldDisconnectHandler;
  CAutoPushPop<DisconnectHandlerFn_t> m_autoRestoreDisconnectHandler;

  void QuitSubs(void);
  void TryToPackageData(uint64_t iCommandNumber);
  void PrepareSubProcess(SubProcess **ppSp,
                         SubProcessKernelObjects **ppCommObjs);
};

template <typename TMutexType>
void CWorkerAccumState<TMutexType>::RangeBegin(uint64_t iFirstCommand,
                                               uint64_t iEndCommand) {
  m_iFirstCommand = iFirstCommand;
  m_iNextCommand = iFirstCommand;
  m_iEndCommand = iEndCommand;
  m_iLastFinished = iFirstCommand;
  m_hCombo = NULL;
  se::shader_compile::shader_combo_processor::Combo_GetNext(
      m_iNextCommand, m_hCombo, m_iEndCommand);

  g_fnDisconnectHandler = Special_DisconnectHandler;

  // Notify all connected sub-processes that the master is still alive
  for (SubProcess *pSp : m_arrSubProcessInfos) {
    if (pSp) {
      SubProcessKernelObjects_Memory shrmem(pSp->pCommObjs);

      if (void *pvMemory = shrmem.Lock()) {
        strcpy((char *)pvMemory, "keepalive");
        shrmem.Unlock();
      }
    }
  }
}

template <typename TMutexType>
void CWorkerAccumState<TMutexType>::RangeFinished() {
  if (!DisconnectState()) {
    // Finish packaging data
    TryToPackageData(m_iEndCommand - 1);
  } else {
    // Master disconnected
    QuitSubs();
  }

  g_fnDisconnectHandler = m_fnOldDisconnectHandler;
}

template <typename TMutexType>
void CWorkerAccumState<TMutexType>::QuitSubs() {
  CUtlVector<HANDLE> m_arrWait;
  m_arrWait.EnsureCapacity(m_arrSubProcessInfos.Count());

  for (SubProcess *pSp : m_arrSubProcessInfos) {
    if (pSp) {
      SubProcessKernelObjects_Memory shrmem(pSp->pCommObjs);
      if (void *pvMemory = shrmem.Lock()) {
        strcpy((char *)pvMemory, "quit");
        shrmem.Unlock();
      }

      m_arrWait.AddToTail(pSp->pi.hProcess);
    }
  }

  if (m_arrWait.Count()) {
    DWORD dwWait = WaitForMultipleObjects(m_arrWait.Count(), m_arrWait.Base(),
                                          TRUE, 2 * 1000);
    if (WAIT_TIMEOUT == dwWait) {
      Warning("Timed out while waiting for sub-processes to shut down!\n");
    }
  }

  for (SubProcess *pSp : m_arrSubProcessInfos) {
    if (pSp) {
      CloseHandle(pSp->pi.hThread);
      CloseHandle(pSp->pi.hProcess);

      delete pSp->pCommObjs;
      delete pSp;
    }
  }

  if (DisconnectState())
    Vmpi_Worker_DefaultDisconnectHandler(
        0, "Master disconnected during compilation.");
}

template <typename TMutexType>
void CWorkerAccumState<TMutexType>::PrepareSubProcess(
    SubProcess **out_sub_process, SubProcessKernelObjects **out_objects) {
  SubProcess *sub_process = m_lpSubProcessInfo.Get();
  SubProcessKernelObjects *objects{nullptr};

  if (sub_process) {
    objects = sub_process->pCommObjs;
  } else {
    sub_process = new SubProcess;
    sub_process->dwSvcThreadId = ThreadGetCurrentId();

    m_lpSubProcessInfo.Set(sub_process);

    char base_name[0x30] = {};
    sprintf_s(base_name, "SHCMPL_SUB_%08X_%08llX_%08X",
              sub_process->dwSvcThreadId, (long long)time(nullptr),
              GetCurrentProcessId());
    objects = sub_process->pCommObjs =
        new SubProcessKernelObjects_Create(base_name);

    ZeroMemory(&sub_process->pi, sizeof(sub_process->pi));

    STARTUPINFO si{(DWORD)sizeof(si)};

    char command_line[0x100];
    sprintf_s(command_line, "\"%s\\shadercompile.exe\" -subprocess %s",
              g_WorkerTempPath, base_name);

#ifdef _DEBUG
    strcat_s(command_line, " -allowdebug");
#endif

    if (!CreateProcess(NULL, command_line, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, g_WorkerTempPath, &si, &sub_process->pi)) {
      Error("Unable to create child compiler process (%s)!\n",
            std::system_category().message(GetLastError()).c_str());
    }

    m_pMutex->Lock();
    sub_process->dwIndex = m_arrSubProcessInfos.AddToTail(sub_process);
    m_pMutex->Unlock();
  }

  if (out_sub_process) *out_sub_process = sub_process;
  if (out_objects) *out_objects = objects;
}

template <typename TMutexType>
void CWorkerAccumState<TMutexType>::ExecuteCompileCommandThreaded(
    se::shader_compile::shader_combo_processor::ComboHandle hCombo) {
  // DebugOut( "threaded: running: \"%s\"\n", szCommand );

  SubProcessKernelObjects *pCommObjs = NULL;
  PrepareSubProcess(NULL, &pCommObjs);

  // Execute the command
  SubProcessKernelObjects_Memory shrmem(pCommObjs);

  {
    void *pvMemory = shrmem.Lock();
    Assert(pvMemory);

    Combo_FormatCommand(hCombo, (char *)pvMemory);

    shrmem.Unlock();
  }

  // Obtain the command response
  {
    void const *pvMemory = shrmem.Lock();
    Assert(pvMemory);

    // TODO: Vitaliy :: TEMP fix:
    // Usually what happens if we fail to lock here is
    // when our subprocess dies and to recover we will
    // attempt to restart on another worker.
    if (!pvMemory)
      // ::RaiseException( GetLastError(), EXCEPTION_NONCONTINUABLE, 0, NULL );
      ExitProcess(1);

    se::shader_compile::command_sink::IResponse *pResponse;
    if (pvMemory)
      pResponse = new CSubProcessResponse(pvMemory);
    else
      pResponse = new se::shader_compile::command_sink::CResponseError;

    HandleCommandResponse(hCombo, pResponse);

    shrmem.Unlock();
  }
}

template <typename TMutexType>
void CWorkerAccumState<TMutexType>::ExecuteCompileCommand(
    se::shader_compile::shader_combo_processor::ComboHandle hCombo) {
  se::shader_compile::command_sink::IResponse *pResponse = NULL;

  {
    char chBuffer[4096] = {};
    Combo_FormatCommand(hCombo, chBuffer);

    DebugOut("running: \"%s\"\n", chBuffer);

    MySystem(chBuffer, &pResponse);
  }

  HandleCommandResponse(hCombo, pResponse);
}

template <typename TMutexType>
void CWorkerAccumState<TMutexType>::HandleCommandResponse(
    se::shader_compile::shader_combo_processor::ComboHandle hCombo,
    se::shader_compile::command_sink::IResponse *pResponse) {
  const bool should_use_vmpi{CommandLine()->FindParm("-nompi") == 0};
  // dimhotepus: Do not handle socket errors if no vmpi.
  if (should_use_vmpi) VMPI_HandleSocketErrors();

  if (!pResponse) {
    pResponse = new se::shader_compile::command_sink::CResponseFiles(
        "shader.o", "output.txt");
  }

  // Command info
  se::shader_compile::shader_combo_processor::CfgEntryInfo const *pEntryInfo =
      Combo_GetEntryInfo(hCombo);
  uint64_t iComboIndex = Combo_GetComboNum(hCombo);
  uint64_t iCommandNumber = Combo_GetCommandNum(hCombo);

  if (pResponse->Succeeded()) {
    GLOBAL_DATA_MTX_LOCK();
    uint64_t nStComboIdx = iComboIndex / pEntryInfo->m_numDynamicCombos;
    uint64_t nDyComboIdx =
        iComboIndex - (nStComboIdx * pEntryInfo->m_numDynamicCombos);
    StaticComboFromDictAdd(pEntryInfo->m_szName, nStComboIdx)
        ->AddDynamicCombo(nDyComboIdx, pResponse->GetResultBuffer(),
                          pResponse->GetResultBufferLen());
    GLOBAL_DATA_MTX_UNLOCK();
  }

  // Tell the master that this shader failed
  if (!pResponse->Succeeded()) {
    GLOBAL_DATA_MTX_LOCK();
    ShaderHadErrorDispatchInt(pEntryInfo->m_szName);
    GLOBAL_DATA_MTX_UNLOCK();
  }

  // Process listing even if the shader succeeds for warnings
  char const *szListing = pResponse->GetListing();
  if ((!g_bSuppressWarnings && szListing) || !pResponse->Succeeded()) {
    char chCommandNumber[32];
    sprintf(chCommandNumber, "%I64u", iCommandNumber);

    char chUnreportedListing[0xFF];
    if (!szListing) {
      sprintf(chUnreportedListing,
              "(0): error 0000: Compiler failed without error description, "
              "latest version of fxc.exe might give a description.");
      szListing = chUnreportedListing;
    }

    // Send the listing for dispatch
    CUtlBinaryBlock errMsg;
    errMsg.SetLength(strlen(chCommandNumber) + 1 +  // command + newline
                     strlen(szListing) + 1 +        // listing + newline
                     1                              // null-terminator
    );

    char *msg = static_cast<char *>(errMsg.Get());

    sprintf(msg, "%s\n%s\n", chCommandNumber, szListing);

    GLOBAL_DATA_MTX_LOCK();
    ErrMsgDispatchInt(msg, pEntryInfo->m_szShaderFileName);
    GLOBAL_DATA_MTX_UNLOCK();
  }

  // Maybe zip things up
  TryToPackageData(iCommandNumber);
}

template <typename TMutexType>
void CWorkerAccumState<TMutexType>::TryToPackageData(uint64_t iCommandNumber) {
  m_pMutex->Lock();

  uint64_t iFinishedByNow = iCommandNumber + 1;

  // Check if somebody is running an earlier command
  for (SubProcess *pSp : m_arrSubProcessInfos) {
    if (pSp && pSp->iRunningCommand < iCommandNumber) {
      iFinishedByNow = 0;
      break;
    }
  }

  uint64_t iLastFinished = m_iLastFinished;
  if (iFinishedByNow > m_iLastFinished) {
    m_iLastFinished = iFinishedByNow;
    m_pMutex->Unlock();
  } else {
    m_pMutex->Unlock();
    return;
  }

  se::shader_compile::shader_combo_processor::ComboHandle hChBegin =
      se::shader_compile::shader_combo_processor::Combo_GetCombo(iLastFinished);
  se::shader_compile::shader_combo_processor::ComboHandle hChEnd =
      se::shader_compile::shader_combo_processor::Combo_GetCombo(
          iFinishedByNow);

  Assert(hChBegin && hChEnd);

  se::shader_compile::shader_combo_processor::CfgEntryInfo const *pInfoBegin =
      Combo_GetEntryInfo(hChBegin);
  se::shader_compile::shader_combo_processor::CfgEntryInfo const *pInfoEnd =
      Combo_GetEntryInfo(hChEnd);

  uint64_t nComboBegin =
      Combo_GetComboNum(hChBegin) / pInfoBegin->m_numDynamicCombos;
  uint64_t nComboEnd = Combo_GetComboNum(hChEnd) / pInfoEnd->m_numDynamicCombos;

  for (; pInfoBegin &&
         ((pInfoBegin->m_iCommandStart < pInfoEnd->m_iCommandStart) ||
          (nComboBegin > nComboEnd));) {
    // Zip this combo
    MessageBuffer mbPacked;
    size_t nPackedLength =
        AssembleWorkerReplyPackage(pInfoBegin, nComboBegin, &mbPacked);

    if (nPackedLength) {
      // Packed buffer
      GLOBAL_DATA_MTX_LOCK();
      uint8 *pCodeBuffer =
          StaticComboFromDictAdd(pInfoBegin->m_szName, nComboBegin)
              ->AllocPackedCodeBlock(nPackedLength);
      GLOBAL_DATA_MTX_UNLOCK();

      if (pCodeBuffer) mbPacked.read(pCodeBuffer, nPackedLength);
    }

    // Next iteration
    if (!nComboBegin--) {
      Combo_Free(hChBegin);
      if ((hChBegin =
               se::shader_compile::shader_combo_processor::Combo_GetCombo(
                   pInfoBegin->m_iCommandEnd)) != NULL) {
        pInfoBegin = Combo_GetEntryInfo(hChBegin);
        nComboBegin = pInfoBegin->m_numStaticCombos - 1;
      }
    }
  }

  Combo_Free(hChBegin);
  Combo_Free(hChEnd);
}

template <typename TMutexType>
bool CWorkerAccumState<TMutexType>::OnProcess() {
  m_pMutex->Lock();
  se::shader_compile::shader_combo_processor::ComboHandle hThreadCombo =
      m_hCombo ? Combo_Alloc(m_hCombo) : NULL;
  m_pMutex->Unlock();

  uint64_t iThreadCommand = ~uint64_t(0);

  SubProcess *pSp = NULL;
  PrepareSubProcess(&pSp, NULL);

  for (;;) {
    m_pMutex->Lock();
    if (DisconnectState()) Combo_Free(m_hCombo);

    if (m_hCombo) {
      Combo_Assign(hThreadCombo, m_hCombo);
      pSp->iRunningCommand = Combo_GetCommandNum(hThreadCombo);
      Combo_GetNext(iThreadCommand, m_hCombo, m_iEndCommand);
    } else {
      Combo_Free(hThreadCombo);
      iThreadCommand = ~uint64_t(0);
      pSp->iRunningCommand = ~uint64_t(0);
    }
    m_pMutex->Unlock();

    if (hThreadCombo) {
      ExecuteCompileCommandThreaded(hThreadCombo);
    } else
      break;
  }

  Combo_Free(hThreadCombo);
  return false;
}

template <typename TMutexType>
bool CWorkerAccumState<TMutexType>::OnProcessST() {
  while (m_hCombo) {
    ExecuteCompileCommand(m_hCombo);

    Combo_GetNext(m_iNextCommand, m_hCombo, m_iEndCommand);
  }
  return false;
}

//
// Worker_ProcessCommandRange_Singleton
//
class Worker_ProcessCommandRange_Singleton {
 public:
  static Worker_ProcessCommandRange_Singleton *&Instance() {
    static Worker_ProcessCommandRange_Singleton *s_ptr = NULL;
    return s_ptr;
  }
  static Worker_ProcessCommandRange_Singleton *GetInstance() {
    Worker_ProcessCommandRange_Singleton *p = Instance();
    Assert(p);
    return p;
  }

 public:
  Worker_ProcessCommandRange_Singleton() {
    Assert(!Instance());
    Instance() = this;
    Startup();
  }
  ~Worker_ProcessCommandRange_Singleton() {
    Assert(Instance() == this);
    Instance() = NULL;
    Shutdown();
  }

 public:
  void ProcessCommandRange(uint64_t shaderStart, uint64_t shaderEnd) const;

 protected:
  void Startup(void);
  void Shutdown(void);

  //
  // Multi-threaded section
 protected:
  struct MT {
    MT() : pWorkerObj(NULL), pThreadPool(NULL) {}

    typedef CThreadFastMutex MultiThreadMutex_t;
    MultiThreadMutex_t mtx;

    typedef CWorkerAccumState<MultiThreadMutex_t> WorkerClass_t;
    WorkerClass_t *pWorkerObj;

    IThreadPool *pThreadPool;
    ThreadPoolStartParams_t tpsp;
  } m_MT;

  //
  // Single-threaded section
 protected:
  struct ST {
    ST() : pWorkerObj(NULL) {}

    typedef CThreadNullMutex MultiThreadMutex_t;
    MultiThreadMutex_t mtx;

    typedef CWorkerAccumState<MultiThreadMutex_t> WorkerClass_t;
    WorkerClass_t *pWorkerObj;
  } m_ST;
};

void Worker_ProcessCommandRange_Singleton::Startup(void) {
  bool bInitializedThreadPool = false;
  CPUInformation const &cpu = *GetCPUInformation();

  if (cpu.m_nLogicalProcessors > 1) {
    // Attempt to initialize thread pool
    m_MT.pThreadPool = g_pThreadPool;
    if (m_MT.pThreadPool) {
      m_MT.tpsp.bIOThreads = false;
      m_MT.tpsp.nThreads = cpu.m_nLogicalProcessors - 1;

      if (m_MT.pThreadPool->Start(m_MT.tpsp)) {
        if (m_MT.pThreadPool->NumThreads() >= 1) {
          // Make sure that our mutex is in multi-threaded mode
          Threading::g_mtxGlobal.SetThreadedMode(Threading::eMultiThreaded);

          m_MT.pWorkerObj = new MT::WorkerClass_t(&m_MT.mtx);

          bInitializedThreadPool = true;
        } else {
          m_MT.pThreadPool->Stop();
        }
      }

      if (!bInitializedThreadPool) m_MT.pThreadPool = NULL;
    }
  }

  // Otherwise initialize single-threaded mode
  if (!bInitializedThreadPool) {
    m_ST.pWorkerObj = new ST::WorkerClass_t(&m_ST.mtx);
  }
}

void Worker_ProcessCommandRange_Singleton::Shutdown(void) {
  if (m_MT.pThreadPool) {
    if (m_MT.pWorkerObj) delete m_MT.pWorkerObj;

    m_MT.pThreadPool->Stop();
    m_MT.pThreadPool = NULL;
  } else {
    if (m_ST.pWorkerObj) delete m_ST.pWorkerObj;
  }
}

void Worker_ProcessCommandRange_Singleton::ProcessCommandRange(
    uint64_t shaderStart, uint64_t shaderEnd) const {
  if (m_MT.pThreadPool) {
    MT::WorkerClass_t *pWorkerObj = m_MT.pWorkerObj;

    pWorkerObj->RangeBegin(shaderStart, shaderEnd);
    pWorkerObj->Run();
    pWorkerObj->RangeFinished();
  } else {
    ST::WorkerClass_t *pWorkerObj = m_ST.pWorkerObj;

    pWorkerObj->RangeBegin(shaderStart, shaderEnd);
    pWorkerObj->OnProcessST();
    pWorkerObj->RangeFinished();
  }
}

// You must process the work unit range.
void Worker_ProcessCommandRange(uint64_t shaderStart, uint64_t shaderEnd) {
  Worker_ProcessCommandRange_Singleton::GetInstance()->ProcessCommandRange(
      shaderStart, shaderEnd);
}

// You must append data to pBuf with the work unit results.
void Worker_ProcessWorkUnitFn(int iThread, uint64_t iWorkUnit,
                              MessageBuffer *pBuf) {
  uint64_t comboStart = iWorkUnit * g_nStaticCombosPerWorkUnit;
  uint64_t comboEnd = comboStart + g_nStaticCombosPerWorkUnit;
  comboEnd = min(g_numStaticCombos, comboEnd);

  // Determine the commands required to be executed:
  uint64_t nComboOfTheEntry = 0;
  se::shader_compile::shader_combo_processor::CfgEntryInfo const *pEntry =
      nullptr;

  pEntry = GetEntryByStaticComboNum(comboEnd, &nComboOfTheEntry);
  uint64_t commandEnd =
      pEntry->m_iCommandStart + nComboOfTheEntry * pEntry->m_numDynamicCombos;
  Assert(commandEnd <= g_numCompileCommands);

  pEntry = GetEntryByStaticComboNum(comboStart, &nComboOfTheEntry);
  uint64_t commandStart =
      pEntry->m_iCommandStart + nComboOfTheEntry * pEntry->m_numDynamicCombos;

  // Compile all the shader combos
  Worker_ProcessCommandRange(commandStart, commandEnd);
  nComboOfTheEntry = pEntry->m_numStaticCombos - 1 - nComboOfTheEntry;

  // Copy off the reply packages
  int nSkipsSoFar = 0;
  for (uint64_t kCombo = comboStart; kCombo < comboEnd; ++kCombo) {
    size_t nCpBytes =
        CopyWorkerReplyPackage(pEntry, nComboOfTheEntry, pBuf, nSkipsSoFar);
    if (nCpBytes)
      nSkipsSoFar = 0;
    else
      --nSkipsSoFar;
    if (nComboOfTheEntry == 0) {
      ++pEntry;
      nComboOfTheEntry = pEntry->m_numStaticCombos;
    }
    nComboOfTheEntry--;
  }

  if (nSkipsSoFar) {
    pBuf->write(&nSkipsSoFar, sizeof(nSkipsSoFar));
  }

  //////////////////////////////////////////////////////////////////////////
  //
  // Now deliver all our accumulated spew to the master
  //
  //////////////////////////////////////////////////////////////////////////

  // Failed shaders
  for (intp k = 0, kEnd = g_Master_ShaderHadError.GetNumStrings(); k < kEnd;
       ++k) {
    char const *szShaderName = g_Master_ShaderHadError.String(k);
    if (!g_Master_ShaderHadError[int_as_symid(k)]) continue;

    intp const len = V_strlen(szShaderName);

    CUtlBinaryBlock bb;
    bb.SetLength(1 + len + 1);
    sprintf((char *)bb.Get(), "%c%s", SHADERHADERROR_PACKETID, szShaderName);

    VMPI_SendData(bb.Get(), bb.Length(), VMPI_MASTER_ID);
    VMPI_HandleSocketErrors();
  }

  // Compiler spew
  for (intp k = 0, kEnd = g_Master_CompilerMsgInfo.GetNumStrings(); k < kEnd;
       ++k) {
    char const *const szMsg = g_Master_CompilerMsgInfo.String(k);
    CompilerMsgInfo const &cmi = g_Master_CompilerMsgInfo[int_as_symid(k)];

    char const *const szFirstCmd = cmi.GetFirstCommand();
    int const numReported = cmi.GetNumTimesReported();

    char chNumReported[0x40];
    sprintf(chNumReported, "%d", numReported);

    CUtlBinaryBlock bb;
    bb.SetLength(1 + strlen(szMsg) + 1 + strlen(szFirstCmd) + 1 +
                 strlen(chNumReported) + 1 + 1);
    sprintf((char *)bb.Get(), "%c%s\n%s\n%s\n", ERRMSG_PACKETID, szMsg,
            szFirstCmd, chNumReported);

    VMPI_SendData(bb.Get(), bb.Length(), VMPI_MASTER_ID);
    VMPI_HandleSocketErrors();
  }

  // Clean all reported msgs
  g_Master_CompilerMsgInfo.Purge();
}

void Shader_ParseShaderInfoFromCompileCommands(
    se::shader_compile::shader_combo_processor::CfgEntryInfo const *pEntry,
    ShaderInfo_t &shaderInfo) {
  if (auto hCombo = se::shader_compile::shader_combo_processor::Combo_GetCombo(
          pEntry->m_iCommandStart)) {
    char cmd[4096];
    Combo_FormatCommand(hCombo, cmd);

    {
      memset(&shaderInfo, 0, sizeof(ShaderInfo_t));

      const char *pCentroidMask = strstr(cmd, "/DCENTROIDMASK=");
      const char *pFlags = strstr(cmd, "/DFLAGS=0x");
      const char *pShaderModel = strstr(cmd, "/DSHADER_MODEL_");

      if (!pCentroidMask || !pFlags || !pShaderModel) {
        Assert(!"!pCentroidMask || !pFlags || !pShaderModel");
        return;
      }

      sscanf(pCentroidMask + ssize("/DCENTROIDMASK=") - 1, "%u",
             &shaderInfo.m_CentroidMask);
      sscanf(pFlags + ssize("/DFLAGS=0x") - 1, "%x", &shaderInfo.m_Flags);

      // Copy shader model
      pShaderModel += ssize("/DSHADER_MODEL_") - 1;
      for (char *pszSm = shaderInfo.m_szShaderModel,
                *const pszEnd = pszSm + sizeof(shaderInfo.m_szShaderModel) - 1;
           pszSm < pszEnd; ++pszSm) {
        char &rchLastChar = (*pszSm = *pShaderModel++);
        if (!rchLastChar || V_isspace(rchLastChar) || '=' == rchLastChar) {
          rchLastChar = 0;
          break;
        }
      }

      shaderInfo.m_nShaderCombo = 0;
      shaderInfo.m_nTotalShaderCombos = pEntry->m_numCombos;
      shaderInfo.m_nDynamicCombos = pEntry->m_numDynamicCombos;
      shaderInfo.m_nStaticCombo = 0;

      shaderInfo.m_pShaderName = pEntry->m_szName;
      shaderInfo.m_pShaderSrc = pEntry->m_szShaderFileName;
    }

    Combo_Free(hCombo);
  }
}

void Worker_GetLocalCopyOfShaders() {
  // Create virtual files for all of the stuff that we need to compile the
  // shader make sure and prefix the file name so that it doesn't find it
  // locally.

  char filename[1024];
  sprintf(filename, "%s\\uniquefilestocopy.txt", g_pShaderPath);

  CUtlInplaceBuffer bffr(0, 0, CUtlBuffer::TEXT_BUFFER);
  if (!g_pFileSystem->ReadFile(filename, NULL, bffr)) {
    fprintf(stderr, "Can't open %s to read files to copy!\n", filename);
    exit(-1);
  }

  while (char *pszLineToCopy = bffr.InplaceGetLinePtr()) {
    V_MakeAbsolutePath(filename, sizeof(filename), pszLineToCopy,
                       g_pShaderPath);

    DebugOut("getting local copy of shader: \"%s\" (\"%s\")\n", pszLineToCopy,
             filename);

    CUtlBuffer fileBuf;
    if (!g_pFileSystem->ReadFile(filename, NULL, fileBuf)) {
      Warning("Can't find \"%s\"\n", filename);
      continue;
    }

    // Grab just the filename.
    char justFilename[MAX_PATH];
    char *pLastSlash =
        max(strrchr(pszLineToCopy, '/'), strrchr(pszLineToCopy, '\\'));
    if (pLastSlash)
      Q_strncpy(justFilename, pLastSlash + 1, sizeof(justFilename));
    else
      Q_strncpy(justFilename, pszLineToCopy, sizeof(justFilename));

    sprintf(filename, "%s%s", g_WorkerTempPath, justFilename);
    DebugOut("creating \"%s\"\n", filename);

    FILE *fp3 = fopen(filename, "wb");
    if (!fp3) {
      Error("Can't open '%s' for writing.", pszLineToCopy);
      continue;
    }

    fwrite(fileBuf.Base(), 1, fileBuf.GetBytesRemaining(), fp3);
    fclose(fp3);

    // SUPER EVIL, but if we don't do this, Windows will randomly nuke files of
    // ours while we're running since they're in the temp path.

    static CUtlVector<FILE *> s_arrHackedFiles;
    static struct X_s_arrHackedFiles {
      ~X_s_arrHackedFiles() {
        for (int k = 0; k < s_arrHackedFiles.Count(); ++k)
          fclose(s_arrHackedFiles[k]);
      }
    } s_autoCloseHackedFiles;

    /* THIS IS THE EVIL LINE ----> */ FILE *fHack = fopen(filename, "r");
    s_arrHackedFiles.AddToTail(fHack);
    // -- END of EVIL
  }
}

void Worker_GetLocalCopyOfBinary(const char *pFilename) {
  CUtlBuffer fileBuf;
  char tmpFilename[MAX_PATH];
  sprintf(tmpFilename, "%s\\%s", g_ExeDir, pFilename);
  DebugOut("trying to open: %s\n", tmpFilename);

  FILE *fp = fopen(tmpFilename, "rb");
  if (!fp) {
    AssertMsg(false, "Unable to open '%s' for read", tmpFilename);
    fprintf(stderr, "Can't open %s!\n", pFilename);
    exit(-1);
  }
  fseek(fp, 0, SEEK_END);
  int fileLen = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  fileBuf.EnsureCapacity(fileLen);
  int nBytesRead = fread(fileBuf.Base(), 1, fileLen, fp);
  fclose(fp);
  fileBuf.SeekPut(CUtlBuffer::SEEK_HEAD, nBytesRead);

  char newFilename[MAX_PATH];
  sprintf(newFilename, "%s%s", g_WorkerTempPath, pFilename);

  FILE *fp2 = fopen(newFilename, "wb");
  if (!fp2) {
    Assert(0);
    fprintf(stderr, "Can't open %s!\n", newFilename);
    exit(-1);
  }
  fwrite(fileBuf.Base(), 1, fileLen, fp2);
  fclose(fp2);

  // SUPER EVIL, but if we don't do this, Windows will randomly nuke files of
  // ours while we're running since they're in the temp path.
  fopen(newFilename, "r");
}

void Worker_GetLocalCopyOfBinaries() {
  // This is necessary so VMPI doesn't run in SDK
  Worker_GetLocalCopyOfBinary("mysql_wrapper.dll");
  Worker_GetLocalCopyOfBinary("vstdlib.dll");
  Worker_GetLocalCopyOfBinary("tier0.dll");
}

void Shared_ParseListOfCompileCommands() {
  double tt_start = Plat_FloatTime();

  char fileListFileName[MAX_PATH];
  sprintf_s(fileListFileName, "%s\\filelist.txt", g_pShaderPath);

  CUtlInplaceBuffer bffr(0, 0, CUtlInplaceBuffer::TEXT_BUFFER);
  if (!g_pFileSystem->ReadFile(fileListFileName, NULL, bffr)) {
    fprintf(stderr, "Can't open %s to read compilation configurations!\n",
            fileListFileName);
    exit(-1);
  }

  se::shader_compile::shader_combo_processor::ReadConfiguration(&bffr);
  se::shader_compile::shader_combo_processor::DescribeConfiguration(
      g_arrCompileEntries);

  for (auto *info = g_arrCompileEntries.get(); info && info->m_szName; ++info) {
    ++g_numShaders;

    g_numStaticCombos += info->m_numStaticCombos;
    g_numDynamicCombos += info->m_numDynamicCombos;
    g_numCompileCommands = info->m_iCommandEnd;
  }

  double tt_end = Plat_FloatTime();

  Msg("\rRead %s shaders compilation configurations in %.2fs.\n",
      PrettyPrintNumber(g_numShaders), tt_end - tt_start);

  char chCommands[32], chNumShaders[32], chStaticCombos[32],
      chDynamicCombos[32];

  sprintf(chCommands, "%s", PrettyPrintNumber(g_numCompileCommands));
  sprintf(chNumShaders, "%s", PrettyPrintNumber(g_numShaders));
  sprintf(chStaticCombos, "%s", PrettyPrintNumber(g_numStaticCombos));
  sprintf(chDynamicCombos, "%s", PrettyPrintNumber(g_numDynamicCombos));

  Msg("\rGoing to compile %s commands from %s shaders. Static combos %s, "
      "dynamic combos %s.\n",
      chCommands, chNumShaders, chStaticCombos, chDynamicCombos);
}

void SetupExeDir(int argc, char **argv) {
  V_strcpy_safe(g_ExeDir, argv[0]);
  Q_StripFilename(g_ExeDir);

  if (g_ExeDir[0] == '\0') {
    Q_strncpy(g_ExeDir, ".\\", sizeof(g_ExeDir));
  }

  Q_FixSlashes(g_ExeDir);
}

void SetupPaths(int argc, char **argv) {
  GetTempPath(sizeof(g_WorkerTempPath), g_WorkerTempPath);
  strcat_s(g_WorkerTempPath, "shadercompiletemp\\");

  char tmp[MAX_PATH];
  sprintf_s(tmp, "rd /s /q \"%s\"", g_WorkerTempPath);
  system(tmp);

  _mkdir(g_WorkerTempPath);

  g_pShaderPath = CommandLine()->ParmValue("-shaderpath", "");
  g_bVerbose = CommandLine()->FindParm("-verbose") != 0;
}

void SetupDebugFile() {
#ifdef DEBUGFP
  const char *pComputerName = getenv("COMPUTERNAME");
  char filename[MAX_PATH];
  sprintf(filename, "\\\\fileserver\\user\\gary\\debug\\%s.txt", pComputerName);
  g_WorkerDebugFp = fopen(filename, "w");
  Assert(g_WorkerDebugFp);
  DebugOut("opened debug file\n");
#endif
}

void CompileShaders_NoVMPI() {
  Worker_ProcessCommandRange_Singleton pcr;

  //
  // We will iterate on the cfg entries and process them
  //
  for (auto *pEntry = g_arrCompileEntries.get(); pEntry && pEntry->m_szName;
       ++pEntry) {
    //
    // Stick the shader info
    //
    ShaderInfo_t siLastShaderInfo;
    memset(&siLastShaderInfo, 0, sizeof(siLastShaderInfo));

    Shader_ParseShaderInfoFromCompileCommands(pEntry, siLastShaderInfo);

    g_ShaderToShaderInfo[pEntry->m_szName] = siLastShaderInfo;

    //
    // Compile stuff
    //
    Worker_ProcessCommandRange(pEntry->m_iCommandStart, pEntry->m_iCommandEnd);

    //
    // Now when the whole shader is finished we can write it
    //
    char const *szShaderToWrite = pEntry->m_szName;
    g_numCommandsCompleted = g_numCompileCommands;
    WriteShaderFiles(szShaderToWrite);
    g_numCommandsCompleted = pEntry->m_iCommandEnd;
  }

  Msg("\r                                                  \r");
}

class CDistributeShaderCompileMaster : public IWorkUnitDistributorCallbacks {
 public:
  CDistributeShaderCompileMaster();
  ~CDistributeShaderCompileMaster();

 public:
  virtual void OnWorkUnitsCompleted(uint64_t numWorkUnits);

 private:
  void ThreadProc();

  friend DWORD WINAPI
  CDistributeShaderCompileMaster::ThreadProcAdapter(LPVOID pvArg);

  static DWORD WINAPI ThreadProcAdapter(LPVOID pvArg) {
    reinterpret_cast<CDistributeShaderCompileMaster *>(pvArg)->ThreadProc();
    return 0;
  }

 private:
  HANDLE m_hThread;
  HANDLE m_hEvent;
  CThreadFastMutex m_mtx;
  BOOL m_bRunning;

 private:
  se::shader_compile::shader_combo_processor::CfgEntryInfo const
      *m_pAnalyzeShaders;
  CUtlVector<char const *> m_arrShaderNamesToWrite;
};

CDistributeShaderCompileMaster::CDistributeShaderCompileMaster()
    : m_hThread(NULL), m_hEvent(NULL), m_bRunning(TRUE) {
  m_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  m_hThread = CreateThread(NULL, 0, ThreadProcAdapter,
                           reinterpret_cast<LPVOID>(this), 0, NULL);

  m_pAnalyzeShaders = g_arrCompileEntries.get();
}

CDistributeShaderCompileMaster::~CDistributeShaderCompileMaster() {
  m_bRunning = FALSE;

  SetEvent(m_hEvent);
  WaitForSingleObject(m_hThread, INFINITE);

  CloseHandle(m_hThread);
  CloseHandle(m_hEvent);
}

void CDistributeShaderCompileMaster::OnWorkUnitsCompleted(
    uint64_t numWorkUnits) {
  // Make sure that our mutex is in multi-threaded mode
  Threading::g_mtxGlobal.SetThreadedMode(Threading::eMultiThreaded);

  // Figure out how many commands have completed based on work units
  g_numCompletedStaticCombos = numWorkUnits * g_nStaticCombosPerWorkUnit;
  uint64_t numStaticCombosOfTheEntry = 0;
  se::shader_compile::shader_combo_processor::CfgEntryInfo const *pEntry =
      GetEntryByStaticComboNum(g_numCompletedStaticCombos,
                               &numStaticCombosOfTheEntry);
  g_numCommandsCompleted =
      pEntry->m_iCommandStart +
      numStaticCombosOfTheEntry * pEntry->m_numDynamicCombos;

  // Iterate over the shaders yet to be written and see if we can queue them
  for (; m_pAnalyzeShaders->m_szName &&
         m_pAnalyzeShaders->m_iCommandEnd <= g_numCommandsCompleted;
       ++m_pAnalyzeShaders) {
    m_mtx.Lock();
    m_arrShaderNamesToWrite.AddToTail(m_pAnalyzeShaders->m_szName);
    SetEvent(m_hEvent);
    m_mtx.Unlock();
  }
}

void CDistributeShaderCompileMaster::ThreadProc() {
  for (; m_bRunning;) {
    WaitForSingleObject(m_hEvent, INFINITE);

    // Do a pump of shaders to write
    for (int numShadersWritten = 0; /* forever */; ++numShadersWritten) {
      m_mtx.Lock();

      char const *szShaderToWrite = NULL;
      if (m_arrShaderNamesToWrite.Count() > numShadersWritten)
        szShaderToWrite = m_arrShaderNamesToWrite[numShadersWritten];
      else
        m_arrShaderNamesToWrite.RemoveAll();

      m_mtx.Unlock();

      if (!szShaderToWrite) break;

      // We have the shader to write asynchronously
      WriteShaderFiles(szShaderToWrite);
    }
  }
}

class ScopedConsoleCtrlHandler {
 public:
  explicit ScopedConsoleCtrlHandler(PHANDLER_ROUTINE handler) noexcept
      : handler_{handler}, ok_{SetConsoleCtrlHandler(handler, TRUE)} {
    AssertMsg(ok_, "Unable to set console ctrl handler for process.");
  }
  ~ScopedConsoleCtrlHandler() noexcept {
    ok_ = SetConsoleCtrlHandler(handler_, FALSE);
    AssertMsg(ok_, "Unable to reset console ctrl handler for process.");
  }

  ScopedConsoleCtrlHandler(ScopedConsoleCtrlHandler &) = delete;
  ScopedConsoleCtrlHandler &operator=(ScopedConsoleCtrlHandler &) = delete;

 private:
  PHANDLER_ROUTINE handler_;
  BOOL ok_;
};

static BOOL WINAPI OnConsoleControlSignal(DWORD ctrl_type) {
  // Because the system creates a new thread in the process to execute the
  // handler function, it is possible that the handler function will be
  // terminated by another thread in the process.  Be sure to synchronize
  // threads in the process with the thread for the handler function.
  const bool is_child_process{CommandLine()->FindParm("-subprocess") != 0};
  if (is_child_process) {
    fprintf(stderr, "Stopping child process.\n");
    // Ok, we are subprocess, just stop.  Parent will handle it.
    // Default handler will exit process.
    return FALSE;
  }

  // Parent process here.  Notify children.
  // Default handler will exit process.
  fprintf(stderr, "Stopping parent process.\n");
  return FALSE;
}

int ShaderCompile_Main(int argc, char *argv[]) {
  const ScopedConsoleCtrlHandler scoped_ctrl_handler{OnConsoleControlSignal};

  EnableCrashingOnCrashes();

  CommandLine()->CreateCmdLine(argc, argv);
  InstallSpewFunction();

  g_bSuppressPrintfOutput = false;
  g_flStartTime = Plat_FloatTime();

  SetupDebugFile();

  // managed specifically in
  // Worker_ProcessCommandRange_Singleton::Startup
  numthreads = 1;

  // Special section of code implementing "-subprocess" flag
  if (const int subprocess{CommandLine()->FindParm("-subprocess") != 0}) {
    const char *subprocess_data{CommandLine()->GetParm(1 + subprocess)};

    ThreadSetDebugName("ShaderCompile_Child");

    return ShaderCompile_Subprocess_Main(subprocess_data);
  }

  ThreadSetDebugName("ShaderCompile_Main");

  // This needs to get called before VMPI is setup because in SDK mode, VMPI
  // will change the args around.
  SetupExeDir(argc, argv);
  SetupPaths(argc, argv);

  const bool should_use_vmpi{CommandLine()->FindParm("-nompi") == 0};
  if (should_use_vmpi) {
    // Master, start accepting connections.
    // Worker, make a connection.
    DebugOut("Before VMPI_Init\n");

    g_bSuppressPrintfOutput = true;
    VMPIRunMode mode = VMPIRunMode::VMPI_RUN_NETWORKED;

    if (!VMPI_Init(argc, argv, "dependency_info_shadercompile.txt",
                   MyDisconnectHandler, mode)) {
      g_bSuppressPrintfOutput = false;
      DebugOut("MPI_Init failed.\n");
      Error("MPI_Init failed.");
    }

    extern void VMPI_SetWorkUnitsPartitionSize(int numWusToDeal);
    VMPI_SetWorkUnitsPartitionSize(32);

    g_bSuppressPrintfOutput = false;
    DebugOut("After VMPI_Init\n");
  }

  // Setting up the minidump handlers
  if (should_use_vmpi && !g_bMPIMaster)
    se::utils::common::SetupToolsMinidumpHandler(VMPI_ExceptionFilter);
  else
    se::utils::common::SetupDefaultToolsMinidumpHandler();

  if (CommandLine()->FindParm("-game") == 0) {
    // Used with filesystem_stdio.dll
    FileSystem_Init(nullptr, 0, FS_INIT_COMPATIBILITY_MODE);
  } else {
    // SDK uses this since it only has filesystem_steam.dll.
    FileSystem_Init(nullptr, 0, FS_INIT_FULL);
  }

  DebugOut("After VMPI_FileSystem_Init\n");
  Shared_ParseListOfCompileCommands();
  DebugOut("After Shared_ParseListOfCompileCommands\n");

  if (should_use_vmpi) {
    // Partition combos
    g_nStaticCombosPerWorkUnit = 0;

    if (g_numStaticCombos) {
      if (g_numStaticCombos <= 1024)
        g_nStaticCombosPerWorkUnit = 1;
      else if (g_numStaticCombos > 1024 * 10)
        g_nStaticCombosPerWorkUnit = 10;
      else
        g_nStaticCombosPerWorkUnit = g_numStaticCombos / 1024;
    }

    uint64_t nWorkUnits;
    if (g_nStaticCombosPerWorkUnit == 0) {
      nWorkUnits = 1;

      g_nStaticCombosPerWorkUnit = g_numStaticCombos;
    } else {
      nWorkUnits = g_numStaticCombos / g_nStaticCombosPerWorkUnit + 1;
    }

    DebugOut("Before conditional\n");
    if (g_bMPIMaster) {
      // Send all of the workers the complete list of work to do.
      DebugOut("Before STARTWORK_PACKETID\n");

      char packetID = STARTWORK_PACKETID;
      VMPI_SendData(&packetID, sizeof(packetID), VMPI_PERSISTENT);

      // Compile master distribution tracker
      CDistributeShaderCompileMaster dscm;
      g_pDistributeWorkCallbacks = &dscm;

      {
        char chCommands[32], chNumWorkUnits[32], chNumShaders[32];

        sprintf(chCommands, "%s", PrettyPrintNumber(g_numCompileCommands));
        sprintf(chNumWorkUnits, "%s", PrettyPrintNumber(nWorkUnits));
        sprintf(chNumShaders, "%s", PrettyPrintNumber(g_numShaders));

        Msg("\rCompiling %s commands in %s work units from %s shaders.\n",
            chCommands, chNumWorkUnits, chNumShaders);
      }

      // nWorkUnits is how many work units. . .1000 is good.
      // The work unit number impies which combo to do.
      DebugOut("Before DistributeWork\n");
      DistributeWork(nWorkUnits, WORKUNIT_PACKETID, NULL,
                     Master_ReceiveWorkUnitFn);

      g_pDistributeWorkCallbacks = NULL;
    } else {
      // wait until we get a packet from the master to start doing stuff.
      MessageBuffer buf;
      DebugOut("Before VMPI_DispatchUntil\n");
      while (!g_bGotStartWorkPacket) {
        VMPI_DispatchNextMessage();
      }
      DebugOut("after VMPI_DispatchUntil\n");

      DebugOut("Before Worker_GetLocalCopyOfShaders\n");
      Worker_GetLocalCopyOfShaders();
      DebugOut("Before Worker_GetLocalCopyOfBinaries\n");
      Worker_GetLocalCopyOfBinaries();

      DebugOut("Before _chdir\n");
      _chdir(g_WorkerTempPath);

      // nWorkUnits is how many work units. . .1000 is good.
      // The work unit number impies which combo to do.
      DebugOut("Before DistributeWork\n");

      // Allows calling into ProcessCommandRange inside the worker function
      {
        Worker_ProcessCommandRange_Singleton pcr;
        DistributeWork(nWorkUnits, WORKUNIT_PACKETID, Worker_ProcessWorkUnitFn,
                       NULL);
      }
    }

    g_bSuppressPrintfOutput = false;
  } else {
    // no VMPI
    Worker_GetLocalCopyOfShaders();
    Worker_GetLocalCopyOfBinaries();

    _chdir(g_WorkerTempPath);

    {
      char chCommands[32], chStaticCombos[32], chDynamicCombos[32];

      sprintf(chCommands, "%s", PrettyPrintNumber(g_numCompileCommands));
      sprintf(chStaticCombos, "%s", PrettyPrintNumber(g_numStaticCombos));
      sprintf(chDynamicCombos, "%s", PrettyPrintNumber(g_numDynamicCombos));

      Msg("\rCompiling %s commands in %s static, %s dynamic combos...\n",
          chCommands, chStaticCombos, chDynamicCombos);
    }

    CompileShaders_NoVMPI();
  }

  Msg("\r                                                                \r");
  if (g_bMPIMaster || !should_use_vmpi) {
    char str[4096];

    // Write everything that succeeded
    intp nStrings = g_ShaderByteCode.GetNumStrings();
    for (intp i = 0; i < nStrings; i++) {
      WriteShaderFiles(g_ShaderByteCode.String(i));
    }

    // Write all the errors
    //////////////////////////////////////////////////////////////////////////
    //
    // Now deliver all our accumulated spew to the output
    //
    //////////////////////////////////////////////////////////////////////////

    bool bValveVerboseComboErrors =
        (getenv("VALVE_VERBOSE_COMBO_ERRORS") &&
         strtol(getenv("VALVE_VERBOSE_COMBO_ERRORS"), nullptr, 10))
            ? true
            : false;

    // Compiler spew
    for (intp k = 0, kEnd = g_Master_CompilerMsgInfo.GetNumStrings(); k < kEnd;
         ++k) {
      char const *const szMsg = g_Master_CompilerMsgInfo.String(k);
      CompilerMsgInfo const &cmi = g_Master_CompilerMsgInfo[int_as_symid(k)];

      char const *const szFirstCmd = cmi.GetFirstCommand();
      int const numReported = cmi.GetNumTimesReported();

      uint64_t iFirstCommand = strtoull(szFirstCmd, nullptr, 10);

      se::shader_compile::shader_combo_processor::ComboHandle hCombo = NULL;
      se::shader_compile::shader_combo_processor::CfgEntryInfo const
          *pComboEntryInfo = NULL;

      if (se::shader_compile::shader_combo_processor::Combo_GetNext(
              iFirstCommand, hCombo, g_numCompileCommands)) {
        Combo_FormatCommand(hCombo, str);
        pComboEntryInfo = Combo_GetEntryInfo(hCombo);
        Combo_Free(hCombo);
      } else {
        sprintf_s(str, "cmd # %s", szFirstCmd);
      }

      Msg("\n%s\n", szMsg);
      Msg("    Reported %d time(s), example command:\n", numReported);

      if (bValveVerboseComboErrors) {
        Msg("    Verbose Description:\n");
        if (pComboEntryInfo) {
          Msg("        Src File: %s\n", pComboEntryInfo->m_szShaderFileName);
          Msg("        Tgt File: %s\n", pComboEntryInfo->m_szName);
        }

        // Between     /DSHADERCOMBO=   and    /Dmain
        char const *pBegin = strstr(str, "/DSHADERCOMBO=");
        char const *pEnd = strstr(str, "/Dmain");
        if (pBegin) {
          pBegin += std::size("/DSHADERCOMBO=") - 1;
          char const *pSpace = strchr(pBegin, ' ');
          if (pSpace)
            Msg("        Combo # : %.*s\n",
                static_cast<unsigned>(pSpace - pBegin), pBegin);
        }

        if (!pEnd) pEnd = str + strlen(str);
        while (pBegin && *pBegin && !V_isspace(*pBegin)) ++pBegin;
        while (pBegin && *pBegin && V_isspace(*pBegin)) ++pBegin;

        // Now parse all combo defines in [pBegin, pEnd]
        while (pBegin && *pBegin && (pBegin < pEnd)) {
          char const *pDefine = strstr(pBegin, "/D");
          if (!pDefine || pDefine >= pEnd) break;

          char const *pEqSign = strchr(pDefine, '=');
          if (!pEqSign || pEqSign >= pEnd) break;

          char const *pSpace = strchr(pEqSign, ' ');
          if (!pSpace || pSpace >= pEnd) pSpace = pEnd;

          pBegin = pSpace;

          Msg("                  %.*s %.*s\n", static_cast<unsigned>(pSpace - pEqSign - 1),
              pEqSign + 1, static_cast<unsigned>(pEqSign - pDefine - 2), pDefine + 2);
        }
      }
      Msg("    %s\n", str);
    }

    // Failed shaders summary
    for (intp k = 0, kEnd = g_Master_ShaderHadError.GetNumStrings(); k < kEnd;
         ++k) {
      const char *shader_name = g_Master_ShaderHadError.String(k);
      if (!g_Master_ShaderHadError[int_as_symid(k)]) continue;

      Msg("FAILED:    %s\n", shader_name);
    }

    // End
    double end = Plat_FloatTime();

    GetHourMinuteSecondsString((int)(end - g_flStartTime), str, sizeof(str));
    DebugOut("%s elapsed\n", str);
    DebugOut("Precise timing = %.5fs\n", (end - g_flStartTime));

    if (should_use_vmpi) {
      VMPI_FileSystem_Term();
      DebugOut("Before VMPI_Finalize\n");
      VMPI_Finalize();
    }
  }

  return g_Master_ShaderHadError.GetNumStrings();
}

struct ShaderCompileDLL final : public IShaderCompileDLL {
  int main(int argc, char **argv) override {
    return ShaderCompile_Main(argc, argv);
  }
};

struct LaunchableDLL final : public ILaunchableDLL {
  int main(int argc, char **argv) override {
    return ShaderCompile_Main(argc, argv);
  }
};

EXPOSE_SINGLE_INTERFACE(ShaderCompileDLL, IShaderCompileDLL,
                        SHADER_COMPILE_INTERFACE_VERSION);
EXPOSE_SINGLE_INTERFACE(LaunchableDLL, ILaunchableDLL,
                        LAUNCHABLE_DLL_INTERFACE_VERSION);
