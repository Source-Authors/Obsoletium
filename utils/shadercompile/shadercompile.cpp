// Copyright Valve Corporation, All rights reserved.
//
// Command-line tool used to compile shaders in the Source engine.
//
// See
// https://developer.valvesoftware.com/wiki/Shader_authoring/Compiling_Shaders

#include "ishadercompiledll.h"

#include <direct.h>
#include <io.h>

#include "tier0/minidump.h"
#include "tier0/icommandline.h"
#include "tier1/utlstring.h"
#include "tier1/UtlStringMap.h"
#include "tier1/utlbinaryblock.h"
#include "tier2/utlstreambuffer.h"

#include "materialsystem/shader_vcs_version.h"
#include "lzma/lzma.h"
#include "vmpi/messbuf.h"
#include "vstdlib/jobthread.h"

#include "tools_minidump.h"
#include "utlnodehash.h"
#include "cmdlib.h"
#include "cmdsink.h"
#include "d3dxfxc.h"
#include "cfgprocessor.h"
#include "ilaunchabledll.h"

#include "posix_file_stream.h"
#include "winlite.h"

// Type conversions should be controlled by programmer explicitly -
// shadercompile makes use of 64-bit integer arithmetics
// #pragma warning( error : 4244 )

namespace {

struct CByteCodeBlock {
  CByteCodeBlock *m_pNext, *m_pPrev;
  CRC32_t m_nCRC32;
  uint64_t m_nComboID;
  size_t m_nCodeSize;
  uint8_t *m_ByteCode;

  CByteCodeBlock() : m_pNext{nullptr}, m_pPrev{nullptr} {
    m_nCRC32 = 0;
    m_nComboID = 0;
    m_nCodeSize = 0;
    m_ByteCode = nullptr;
  }

  CByteCodeBlock(void const *pByteCode, size_t nCodeSize, uint64_t nComboID) {
    m_ByteCode = new uint8_t[nCodeSize];
    m_nComboID = nComboID;
    m_nCodeSize = nCodeSize;
    memcpy(m_ByteCode, pByteCode, nCodeSize);
    m_nCRC32 = CRC32_ProcessSingleBuffer(pByteCode, nCodeSize);
  }

  ~CByteCodeBlock() { delete[] m_ByteCode; }
};

int __cdecl CompareDynamicComboIDs(CByteCodeBlock *const *pA,
                                   CByteCodeBlock *const *pB) {
  if ((*pA)->m_nComboID < (*pB)->m_nComboID) return -1;
  if ((*pA)->m_nComboID > (*pB)->m_nComboID) return 1;
  return 0;
}

// all the data for one static combo
struct CStaticCombo {
  CStaticCombo *m_pNext, *m_pPrev;
  uint64_t m_nStaticComboID;
  CUtlVector<CByteCodeBlock *> m_DynamicCombos;

  struct PackedCode : protected CArrayAutoPtr<uint8_t> {
    size_t GetLength() const {
      if (uint8_t *pb = Get()) return *reinterpret_cast<size_t *>(pb);

      return 0;
    }
    uint8_t *GetData() const {
      if (uint8_t *pb = Get()) return pb + sizeof(size_t);

      return nullptr;
    }
    uint8_t *AllocData(size_t len) {
      Delete();
      if (len) {
        Attach(new uint8_t[len + sizeof(size_t)]);
        *reinterpret_cast<size_t *>(Get()) = len;
      }
      return GetData();
    }
  } m_abPackedCode;  // Packed code for entire static combo

  uint64_t Key() const { return m_nStaticComboID; }

  explicit CStaticCombo(uint64_t nComboID)
      : m_pNext{nullptr}, m_pPrev{nullptr}, m_nStaticComboID{nComboID} {}
  ~CStaticCombo() { m_DynamicCombos.PurgeAndDeleteElements(); }

  void AddDynamicCombo(uint64_t nComboID, void const *pComboData,
                       size_t nCodeSize) {
    auto *pNewBlock = new CByteCodeBlock(pComboData, nCodeSize, nComboID);
    m_DynamicCombos.AddToTail(pNewBlock);
  }

  void SortDynamicCombos() { m_DynamicCombos.Sort(CompareDynamicComboIDs); }

  uint8_t *AllocPackedCodeBlock(size_t nPackedCodeSize) {
    return m_abPackedCode.AllocData(nPackedCodeSize);
  }
};

using StaticComboNodeHash_t = CUtlNodeHash<CStaticCombo, 7097, uint64_t>;

}  // namespace

template <>
inline StaticComboNodeHash_t **Construct(StaticComboNodeHash_t **memory) {
  // Explicitly new with nullptr
  return ::new (memory) StaticComboNodeHash_t *(nullptr);
}

namespace {

void DebugOut(bool is_verbose, PRINTF_FORMAT_STRING const char *fmt, ...) {
  if (is_verbose) {
    char msg[2048] = {'d', 'b', 'g', ':', ' ', '\0'};
    va_list marker;
    va_start(marker, fmt);
    V_vsnprintf(msg + 5, std::size(msg) - 5, fmt, marker);
    va_end(marker);

    Msg("%s", msg);
  }
}

[[nodiscard]] inline uint32 uint64_as_uint32(uint64_t x) {
  Assert(x < uint64_t(uint32(~0)));
  return uint32(x);
}

[[nodiscard]] constexpr inline UtlSymId_t ushort_as_symid(unsigned short x) {
  static_assert(std::is_same_v<decltype(x), UtlSymId_t>);
  return UtlSymId_t(x);
}

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

FORCEINLINE long AsTargetLong(long x) { return x; }

struct ShaderInfo_t {
  ShaderInfo_t() { memset(this, 0, sizeof(*this)); }

  uint64_t m_nShaderCombo;
  uint64_t m_nTotalShaderCombos;
  const char *m_pShaderName;
  const char *m_pShaderSrc;
  uint32_t m_CentroidMask;
  uint64_t m_nDynamicCombos;
  uint64_t m_nStaticCombo;
  uint32_t m_Flags;  // from IShader.h
  char m_szShaderModel[12];
};

void ParseShaderInfoFromCompileCommands(
    se::shader_compile::shader_combo_processor::CfgEntryInfo const *pEntry,
    ShaderInfo_t &shaderInfo);

using CShaderMap = CUtlStringMap<StaticComboNodeHash_t *>;

CStaticCombo *StaticComboFromDictAdd(CShaderMap &byte_code,
                                     char const *pszShaderName,
                                     uint64_t nStaticComboId) {
  StaticComboNodeHash_t *&rpNodeHash = byte_code[pszShaderName];
  if (!rpNodeHash) rpNodeHash = new StaticComboNodeHash_t;

  // search for this static combo. make it if not found
  CStaticCombo *pStaticCombo = rpNodeHash->FindByKey(nStaticComboId);
  if (!pStaticCombo) {
    pStaticCombo = new CStaticCombo(nStaticComboId);
    rpNodeHash->Add(pStaticCombo);
  }

  return pStaticCombo;
}

CStaticCombo *StaticComboFromDict(CShaderMap &byte_code,
                                  char const *pszShaderName,
                                  uint64_t nStaticComboId) {
  if (const StaticComboNodeHash_t *pNodeHash = byte_code[pszShaderName]) {
    return pNodeHash->FindByKey(nStaticComboId);
  }

  return nullptr;
}

class CompilerMsgInfo {
 public:
  CompilerMsgInfo() : m_reported_num_times(0) {}

 public:
  void SetMsgReportedCommand(char const *command,
                             uint64_t reported_num_times = 1) {
    if (!m_reported_num_times) m_first_command = command;

    m_reported_num_times += reported_num_times;
  }

  char const *GetFirstCommand() const { return m_first_command.String(); }
  uint64_t GetNumTimesReported() const { return m_reported_num_times; }

 private:
  CUtlString m_first_command;
  uint64_t m_reported_num_times;
};

struct CompilerShaderStats {
  CUtlStringMap<bool> shader_had_error_map;
  CUtlStringMap<bool> shader_is_written_map;
  CUtlStringMap<CompilerMsgInfo> shader_message_info_map;
};

namespace threading {

enum class Mode { SingleThreaded = 0, MultiThreaded = 1 };

// A special object that makes single-threaded code incur no penalties
// and multithreaded code to be synchronized properly.
template <typename TMutex = CThreadFastMutex>
class CSwitchableMutex {
 public:
  FORCEINLINE explicit CSwitchableMutex(Mode eMode, TMutex *pMtMutex = nullptr)
      : m_pMtx(pMtMutex),
        m_pUseMtx(eMode == Mode::MultiThreaded ? pMtMutex : nullptr) {}

 public:
  FORCEINLINE void SetMtMutex(TMutex *pMtMutex) {
    m_pMtx = pMtMutex;
    m_pUseMtx = (m_pUseMtx ? pMtMutex : nullptr);
  }

  FORCEINLINE void SetThreadedMode(Mode eMode) {
    m_pUseMtx = eMode == Mode::MultiThreaded ? m_pMtx : nullptr;
  }

 public:
  FORCEINLINE void Lock() {
    if (TMutex *pUseMtx = m_pUseMtx) pUseMtx->Lock();
  }
  FORCEINLINE void Unlock() {
    if (TMutex *pUseMtx = m_pUseMtx) pUseMtx->Unlock();
  }

  FORCEINLINE bool TryLock() {
    if (TMutex *pUseMtx = m_pUseMtx) return pUseMtx->TryLock();

    return true;
  }
  FORCEINLINE bool AssertOwnedByCurrentThread() {
    if (TMutex *pUseMtx = m_pUseMtx)
      return pUseMtx->AssertOwnedByCurrentThread();

    return true;
  }
  FORCEINLINE void SetTrace(bool b) {
    if (TMutex *pUseMtx = m_pUseMtx) pUseMtx->SetTrace(b);
  }

  FORCEINLINE uint32 GetOwnerId() const {
    if (TMutex *pUseMtx = m_pUseMtx) return pUseMtx->GetOwnerId();

    return 0;
  }
  FORCEINLINE int GetDepth() const {
    if (TMutex *pUseMtx = m_pUseMtx) return pUseMtx->GetDepth();

    return 0;
  }

 private:
  TMutex *m_pMtx;
  CInterlockedPtr<TMutex> m_pUseMtx;
};

namespace details {

using MtMutexType_t = CThreadMutex;
MtMutexType_t g_mtxSyncObjMT;

};  // namespace details

CSwitchableMutex<details::MtMutexType_t> g_mtxGlobal(Mode::SingleThreaded,
                                                     &details::g_mtxSyncObjMT);

struct CGlobalMutexAutoLock {
  CGlobalMutexAutoLock() { g_mtxGlobal.Lock(); }
  ~CGlobalMutexAutoLock() { g_mtxGlobal.Unlock(); }
};

};  // namespace threading

// Access to global data should be synchronized by these global locks
#define GLOBAL_DATA_MTX_LOCK() threading::g_mtxGlobal.Lock()
#define GLOBAL_DATA_MTX_UNLOCK() threading::g_mtxGlobal.Unlock()
#define GLOBAL_DATA_MTX_LOCK_AUTO threading::CGlobalMutexAutoLock UNIQUE_ID;

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
  bool bFound = szString == nullptr;
  char *szNext = nullptr;

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

char *FindLast(_In_z_ char *szString, char *szSearchSet) {
  bool bFound = true;
  char *szNext = nullptr;

  if (szSearchSet) {
    for (; *szSearchSet; ++szSearchSet) {
      if (char *szTmp = strrchr(szString, *szSearchSet)) {
        szNext = bFound ? (max(szNext, szTmp)) : szTmp;
        bFound = true;
      }
    }
  }

  return bFound ? szNext : (szString + strlen(szString));
}

void ErrMsgDispatchMsgLine(
    char const *szCommand, char *szMsgLine, char const *szShaderName,
    CUtlStringMap<CompilerMsgInfo> &shader_message_info_map) {
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
      V_sprintf_safe(chFitLongMsgLine, "%s%s", szShaderName, szMsgLine);
      szMsgLine = chFitLongMsgLine;
    } else if (!V_strncmp(szMsgLine, "memory(", 7)) {
      V_sprintf_safe(chFitLongMsgLine, "%s%s", szShaderName, szMsgLine + 6);
      szMsgLine = chFitLongMsgLine;
    }
  }

  // Now store the message with the command it was generated from
  shader_message_info_map[szMsgLine].SetMsgReportedCommand(szCommand);
}

void ErrMsgDispatchInt(
    char *szMessage, char const *szShaderName,
    CUtlStringMap<CompilerMsgInfo> &shader_message_info_map) {
  // First line is the command number "szCommand"
  char *szCommand = ConsumeCharacters(szMessage, isspace);
  char *szMessageListing = FindNext(szCommand, "\r\n");
  char chTerminator = *szMessageListing;
  *(szMessageListing++) = '\0';

  // Now come the command lines actually
  while (chTerminator) {
    char *szMsgText = ConsumeCharacters(szMessageListing, isspace);
    szMessageListing = FindNext(szMsgText, "\r\n");
    chTerminator = *szMessageListing;
    *(szMessageListing++) = 0;

    if (*szMsgText) {
      // Trim command at redirection character if present
      *FindNext(szCommand, ">") = '\0';
      ErrMsgDispatchMsgLine(szCommand, szMsgText, szShaderName,
                            shader_message_info_map);
    }
  }
}

void ShaderHadErrorDispatchInt(char const *szShader,
                               CUtlStringMap<bool> &shader_had_error_map) {
  shader_had_error_map[szShader] = true;
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

int __cdecl CompareDupComboIndices(const StaticComboAliasRecord_t *pA,
                                   const StaticComboAliasRecord_t *pB) {
  return pA->m_nStaticComboID - pB->m_nStaticComboID;
}

void FlushCombos(size_t *pnTotalFlushedSize, CUtlBuffer *pDynamicComboBuffer,
                 MessageBuffer *pBuf) {
  if (!pDynamicComboBuffer->TellPut())
    // Nothing to do here
    return;

  unsigned nCompressedSize;
  uint8_t *pCompressedShader = LZMA_OpportunisticCompress(
      pDynamicComboBuffer->Base<uint8_t>(), pDynamicComboBuffer->TellPut(),
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

void OutputDynamicCombo(size_t *pnTotalFlushedSize, CUtlBuffer *buffer,
                        MessageBuffer *pBuf, uint64_t nComboID, intp nComboSize,
                        uint8_t *pComboCode) {
  if (buffer->TellPut() + nComboSize + 16 >= MAX_SHADER_UNPACKED_BLOCK_SIZE) {
    FlushCombos(pnTotalFlushedSize, buffer, pBuf);
  }

  buffer->PutInt(uint64_as_uint32(nComboID));
  buffer->PutInt(nComboSize);
  buffer->Put(pComboCode, nComboSize);
}

template <intp outSize>
void GetVCSFilenames(ShaderInfo_t const &si, const char *shader_path,
                     OUT_Z_ARRAY char (&pszMainOutFileName)[outSize]) {
  V_sprintf_safe(pszMainOutFileName, "%s\\shaders\\fxc", shader_path);

  struct _stat buf;
  if (_stat(pszMainOutFileName, &buf) == -1) {
    Msg("mkdir %s\n", pszMainOutFileName);
    // doh. . need to make the directory that the vcs file is going to go into.
    if (_mkdir(pszMainOutFileName) && errno != EEXIST) {
      Error("Unable to create directory for '%s' to place vcs files: '%s'.\n",
            pszMainOutFileName, std::generic_category().message(errno).c_str());
    }
  }

  V_strcat_safe(pszMainOutFileName, "\\");
  V_strcat_safe(pszMainOutFileName, si.m_pShaderName);
  V_strcat_safe(pszMainOutFileName,
                ".vcs");  // Different extensions for main output file

  // Check status of vcs file...
  if (_stat(pszMainOutFileName, &buf) != -1) {
    // The file exists, let's see if it's writable.
    if (!(buf.st_mode & _S_IWRITE)) {
      // It isn't writable. . we'd better change its permissions (or check it
      // out possibly)
      Warning("making %s writable!\n", pszMainOutFileName);
      if (_chmod(pszMainOutFileName, _S_IREAD | _S_IWRITE)) {
        Error("Unable to make file '%s' writable: '%s'.\n", pszMainOutFileName,
              std::generic_category().message(errno).c_str());
      }
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
  CRC32_t m_nCRC32;  // CRC32 of packed data
  struct CStaticCombo *m_pByteCode;
};

int __cdecl CompareComboIds(const StaticComboAuxInfo_t *pA,
                            const StaticComboAuxInfo_t *pB) {
  return pA->m_nStaticComboID - pB->m_nStaticComboID;
}

void WriteShaderFiles(
    const char *shader_path, const char *shader_name,
    const std::unique_ptr<
        se::shader_compile::shader_combo_processor::CfgEntryInfo[]> &configs,
    CUtlStringMap<ShaderInfo_t> &shader_info_map, CShaderMap &byte_code,
    CompilerShaderStats &compiler_stats, uint64_t total_commands_num,
    uint64_t completed_commands_num, bool is_verbose) {
  if (!compiler_stats.shader_is_written_map.Defined(shader_name))
    compiler_stats.shader_is_written_map[shader_name] = true;
  else
    return;

  const bool is_shader_failed =
      compiler_stats.shader_had_error_map.Defined(shader_name);
  const char *shader_operation_name =
      is_shader_failed ? "Removing failed" : "Writing";

  //
  // Progress indication
  //
  if (completed_commands_num < total_commands_num) {
    static constexpr char progress[] = {'/', '-', '\\', '|'};
    static std::size_t progress_index = 0;

    progress_index = ++progress_index % std::size(progress);

    Msg("\b%c", progress[progress_index]);
  } else {
    char shader_name_msg[33];

    // dimhotepus: Truncate here, so no V_strcpy_safe.
    V_strncpy(shader_name_msg, shader_name, sizeof(shader_name_msg));
    V_strncpy(shader_name_msg + sizeof(shader_name_msg) - 4, "...", 4);

    // dimhotepus: Correctly rewrite long strings.
    Msg("\r%s %s\t\t\t\t\r", shader_operation_name, shader_name_msg);
  }

  //
  // Retrieve the data we are going to operate on
  // from global variables under lock.
  //
  GLOBAL_DATA_MTX_LOCK();
  StaticComboNodeHash_t *pByteCodeArray;
  {
    // Get a static combo pointer, reset it as well
    StaticComboNodeHash_t *&rp = byte_code[shader_name];
    pByteCodeArray = rp;
    rp = nullptr;
  }

  ShaderInfo_t &shaderInfo = shader_info_map[shader_name];
  if (!shaderInfo.m_pShaderName) {
    for (const auto *pAnalyze = configs.get(); pAnalyze->m_szName; ++pAnalyze) {
      if (!strcmp(pAnalyze->m_szName, shader_name)) {
        ParseShaderInfoFromCompileCommands(pAnalyze, shaderInfo);
        shader_info_map[shader_name] = shaderInfo;
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
  GetVCSFilenames(shaderInfo, shader_path, szVCSfilename);

  if (is_shader_failed) {
    DebugOut(is_verbose, "removing failed shader file \"%s\".\n",
             szVCSfilename);
    if (unlink(szVCSfilename) && errno != ENOENT) {
      Warning("Unable to remove failed shader file '%s': %s.\n", szVCSfilename,
              std::generic_category().message(errno).c_str());
    }
    return;
  }

  if (!pByteCodeArray) return;

  DebugOut(is_verbose,
           "%s: %llu combos centroid mask: 0x%x numDynamicCombos: %llu flags: "
           "0x%x\n",
           shader_name, shaderInfo.m_nTotalShaderCombos,
           shaderInfo.m_CentroidMask, shaderInfo.m_nDynamicCombos,
           shaderInfo.m_Flags);

  //
  // Static combo headers
  //
  CUtlVector<StaticComboAuxInfo_t> StaticComboHeaders;

  // we know how much ram we need
  StaticComboHeaders.EnsureCapacity(pByteCodeArray->Count() + 1);

  CUtlVector<intp> comboIndicesHashedByCRC32[STATIC_COMBO_HASH_SIZE];
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
        CRC32_t nHashIdx = Hdr.m_nCRC32 % STATIC_COMBO_HASH_SIZE;
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
  intp nDictionaryOffset = 0;

  {
    //
    // Shader file stream buffer
    //
    // Streaming buffer for vcs file (since this can blow memory)
    CUtlStreamBuffer ShaderFile(szVCSfilename, nullptr);
    RunCodeAtScopeExit(ShaderFile.Close());
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
    nDictionaryOffset = ShaderFile.TellPut();

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
        CStaticCombo *pStatic =
            pByteCodeArray->FindByKey(SRec.m_nStaticComboID);
        Assert(pStatic);

        // Put the packed chunk of code for this static combo
        if (size_t nPackedLen = pStatic->m_abPackedCode.GetLength())
          ShaderFile.Put(pStatic->m_abPackedCode.GetData(), nPackedLen);

        ShaderFile.PutInt(0xffffffff);  // end of dynamic combos
      }
    }
  }

  //
  // Re-writing the combo header
  //
  {
    FILE *Handle = fopen(szVCSfilename, "rb+");
    if (!Handle) {
      int rc = errno;
      Warning("Failed to re-open %s: %s.\n", szVCSfilename,
              std::generic_category().message(rc).c_str());
      exit(rc);
    }

    RunCodeAtScopeExit(fclose(Handle));

    _fseeki64(Handle, nDictionaryOffset, SEEK_SET);

    // now, rewrite header. data is already byte-swapped appropriately
    for (auto &c : StaticComboHeaders) {
      fwrite(&(c.m_nStaticComboID), 4, 1, Handle);
      fwrite(&(c.m_nFileOffset), 4, 1, Handle);
    }
  }

  // Finalize, free memory
  delete pByteCodeArray;

  if (completed_commands_num >= total_commands_num) {
    Msg("\r                                                                \r");
  }
}

// same as "system", but doesn't pop up a window
std::unique_ptr<se::shader_compile::command_sink::IResponse> MySystem(
    char const *const pCommand, const char *temp_path) {
  // Trap the command in se::shader_compile::fxc_intercept
  if (auto response =
          se::shader_compile::fxc_intercept::TryExecuteCommand(pCommand)) {
    SwitchToThread();
    return response;
  }

  if (unlink(se::shader_compile::fxc_intercept::kShaderArtefactOutputName) &&
      errno != ENOENT) {
    Warning("Unable to remove failed shader file '%s' (%d: %s).\n",
            se::shader_compile::fxc_intercept::kShaderArtefactOutputName, errno,
            std::generic_category().message(errno).c_str());
  }

  // dimhotepus: temp.bat -> multi thread/process capable name. CS:GO
  char temp_file_name[128];
  V_sprintf_safe(temp_file_name, "sc%lu_%lu.bat", GetCurrentProcessId(),
                 GetCurrentThreadId());

  {
    auto [fp, rc] =
        se::posix::posix_file_stream_factory::open(temp_file_name, "w");
    if (rc) {
      Error("Unable to create writable %s to execute commands (%d: %s).\n",
            temp_file_name, rc.value(), rc.message().c_str());
    }

    // dimhotepus: Honor command (fxc) exit code.
    std::tie(std::ignore, rc) = fp.print("%s\nexit %%errorlevel%%\n", pCommand);
    if (rc) {
      Error("Unable to write commands to %s (%d: %s).\n", temp_file_name,
            rc.value(), rc.message().c_str());
    }
  }

  STARTUPINFO si{(DWORD)sizeof(si)};
  PROCESS_INFORMATION pi = {};

  // Start the child process.
  if (!CreateProcessA(
          nullptr,         // No module name (use command line).
          temp_file_name,  // Command line.
          nullptr,         // Process handle not inheritable.
          nullptr,         // Thread handle not inheritable.
          FALSE,           // Set handle inheritance to FALSE.
          IDLE_PRIORITY_CLASS | CREATE_NO_WINDOW,  // No creation flags.
          nullptr,    // Use parent's environment block.
          temp_path,  // Use parent's starting directory.
          &si,        // Pointer to STARTUPINFO structure.
          &pi)        // Pointer to PROCESS_INFORMATION structure.
  ) {
    const DWORD rc{GetLastError()};
    Error("Unable to create console process '%s/%s' (%lu: %s)!\n", temp_path,
          temp_file_name, rc, std::system_category().message(rc).c_str());
  }

  // Wait until child process exits.
  WaitForSingleObject(pi.hProcess, INFINITE);

  DWORD rc;
  if (GetExitCodeProcess(pi.hProcess, &rc) && rc != 0) {
    Warning("'%s' command failed w/e %lu.\n", pCommand, rc);
  }

  // Close process and thread handles.
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);

  if (unlink(temp_file_name) && errno != ENOENT) {
    Warning("Unable to remove '%s' (%d: %s).\n", pCommand, errno,
            std::generic_category().message(errno).c_str());
  }

  return std::make_unique<se::shader_compile::command_sink::CResponseFiles>(
      se::shader_compile::fxc_intercept::kShaderArtefactOutputName,
      kShaderCompilerOutputName);
}

// Assemble a reply package to the master from the compiled bytecode
// return the length of the package.
size_t AssembleWorkerReplyPackage(
    se::shader_compile::shader_combo_processor::CfgEntryInfo const *pEntry,
    uint64_t nComboOfEntry, CShaderMap &byte_code, MessageBuffer *pBuf) {
  CStaticCombo *pStComboRec;
  StaticComboNodeHash_t *pByteCodeArray;

  {
    GLOBAL_DATA_MTX_LOCK_AUTO;
    pStComboRec =
        StaticComboFromDict(byte_code, pEntry->m_szName, nComboOfEntry);
    pByteCodeArray = byte_code[pEntry->m_szName];
  }

  size_t nBytesWritten = 0;

  if (pStComboRec && pStComboRec->m_DynamicCombos.Count()) {
    CUtlBuffer ubDynamicComboBuffer;
    ubDynamicComboBuffer.SetBigEndian(false);

    pStComboRec->SortDynamicCombos();
    // iterate over all dynamic combos.
    for (auto *pCode : pStComboRec->m_DynamicCombos) {
      OutputDynamicCombo(&nBytesWritten, &ubDynamicComboBuffer, pBuf,
                         pCode->m_nComboID, pCode->m_nCodeSize,
                         pCode->m_ByteCode);
    }
    FlushCombos(&nBytesWritten, &ubDynamicComboBuffer, pBuf);
  }

  // Time to limit amount of prints
  static double s_fLastInfoTime = 0;
  double fCurTime = Plat_FloatTime();

  GLOBAL_DATA_MTX_LOCK_AUTO;
  if (pStComboRec) pByteCodeArray->DeleteByKey(nComboOfEntry);
  if (fabs(fCurTime - s_fLastInfoTime) > 1.f) {
    // dimhotepus: Correctly rewrite long strings.
    Msg("Compiling  %s  [ %s remaining ] ...                            \r",
        pEntry->m_szName, PrettyPrintNumber(nComboOfEntry));
    s_fLastInfoTime = fCurTime;
  }

  return nBytesWritten;
}

template <typename TMutexType>
class CWorkerAccumState
    : public CParallelProcessorBase<CWorkerAccumState<TMutexType>> {
  friend class ThisParallelProcessorBase_t;

 public:
  explicit CWorkerAccumState(TMutexType *pMutex, IThreadPool *pPool)
      : CParallelProcessorBase<CWorkerAccumState<TMutexType>>{pPool},
        m_pMutex(pMutex),
        m_iFirstCommand(0),
        m_iNextCommand(0),
        m_iEndCommand(0),
        m_iLastFinished(0),
        m_hCombo(nullptr),
        m_is_verbose{false},
        m_byte_code{nullptr},
        m_compiler_stats{nullptr} {}
  ~CWorkerAccumState() {}

  void RangeBegin(uint64_t iFirstCommand, uint64_t iEndCommand,
                  const char *temp_path, bool is_verbose, CShaderMap &byte_code,
                  CompilerShaderStats &compiler_stats);
  void RangeFinished();

  void ExecuteCompileCommand(
      se::shader_compile::shader_combo_processor::ComboHandle hCombo);
  void HandleCommandResponse(
      se::shader_compile::shader_combo_processor::ComboHandle hCombo,
      std::unique_ptr<se::shader_compile::command_sink::IResponse> pResponse);

 public:
  void Run() {
    // Without this line infos grow causes invalidation of iterators and crash
    // due to iterators being shared between threads without checks.
    m_arrSubProcessInfos.EnsureCapacity(
        m_pThreadPool ? (m_pThreadPool->NumThreads() + 1) : 1);

    ThisParallelProcessorBase_t::Run();

    m_arrSubProcessInfos.Purge();
  }

 protected:
  bool OnProcess();

  TMutexType *m_pMutex;

 protected:
  CUtlVector<uint64_t> m_arrSubProcessInfos;
  uint64_t m_iFirstCommand;
  uint64_t m_iNextCommand;
  uint64_t m_iEndCommand;

  uint64_t m_iLastFinished;

  se::shader_compile::shader_combo_processor::ComboHandle m_hCombo;
  CUtlString m_temp_path;
  CShaderMap *m_byte_code;
  CompilerShaderStats *m_compiler_stats;
  bool m_is_verbose;

  void TryToPackageData(uint64_t iCommandNumber);
};

template <typename TMutexType>
void CWorkerAccumState<TMutexType>::RangeBegin(
    uint64_t iFirstCommand, uint64_t iEndCommand, const char *temp_path,
    bool is_verbose, CShaderMap &byte_code,
    CompilerShaderStats &compiler_stats) {
  m_iFirstCommand = iFirstCommand;
  m_iNextCommand = iFirstCommand;
  m_iEndCommand = iEndCommand;
  m_iLastFinished = iFirstCommand;
  m_hCombo = nullptr;
  m_temp_path = temp_path;
  m_byte_code = &byte_code;
  m_compiler_stats = &compiler_stats;
  m_is_verbose = is_verbose;

  se::shader_compile::shader_combo_processor::Combo_GetNext(
      m_iNextCommand, m_hCombo, m_iEndCommand);
}

template <typename TMutexType>
void CWorkerAccumState<TMutexType>::RangeFinished() {
  // Finish packaging data
  TryToPackageData(m_iEndCommand - 1);
}

template <typename TMutexType>
void CWorkerAccumState<TMutexType>::ExecuteCompileCommand(
    se::shader_compile::shader_combo_processor::ComboHandle hCombo) {
  char chBuffer[4096] = {};
  Combo_FormatCommand(hCombo, chBuffer);

  auto response = MySystem(chBuffer, m_temp_path.Get());

  HandleCommandResponse(hCombo, std::move(response));
}

template <typename TMutexType>
void CWorkerAccumState<TMutexType>::HandleCommandResponse(
    se::shader_compile::shader_combo_processor::ComboHandle hCombo,
    std::unique_ptr<se::shader_compile::command_sink::IResponse> pResponse) {
  // Command info
  se::shader_compile::shader_combo_processor::CfgEntryInfo const *pEntryInfo =
      Combo_GetEntryInfo(hCombo);
  uint64_t iComboIndex = Combo_GetComboNum(hCombo);
  uint64_t iCommandNumber = Combo_GetCommandNum(hCombo);

  {
    GLOBAL_DATA_MTX_LOCK_AUTO;
    if (pResponse->Succeeded()) {
      uint64_t nStComboIdx = iComboIndex / pEntryInfo->m_numDynamicCombos;
      uint64_t nDyComboIdx =
          iComboIndex - (nStComboIdx * pEntryInfo->m_numDynamicCombos);
      StaticComboFromDictAdd(*m_byte_code, pEntryInfo->m_szName, nStComboIdx)
          ->AddDynamicCombo(nDyComboIdx, pResponse->GetResultBuffer(),
                            pResponse->GetResultBufferLen());
    } else {
      // Tell the master that this shader failed
      ShaderHadErrorDispatchInt(pEntryInfo->m_szName,
                                m_compiler_stats->shader_had_error_map);
    }
  }

  // Process listing even if the shader succeeds for warnings
  char const *szListing = pResponse->GetListing();
  if (szListing || !pResponse->Succeeded()) {
    char chCommandNumber[32];
    V_to_chars(chCommandNumber, iCommandNumber);

    char chUnreportedListing[0xFF];
    if (!szListing) {
      V_sprintf_safe(
          chUnreportedListing,
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

    V_snprintf(msg, errMsg.Length(), "%s\n%s\n", chCommandNumber, szListing);

    GLOBAL_DATA_MTX_LOCK_AUTO;
    ErrMsgDispatchInt(msg, pEntryInfo->m_szShaderFileName,
                      m_compiler_stats->shader_message_info_map);
  }

  // Maybe zip things up
  TryToPackageData(iCommandNumber);
}

template <typename TMutexType>
void CWorkerAccumState<TMutexType>::TryToPackageData(uint64_t iCommandNumber) {
  uint64_t iFinishedByNow = iCommandNumber + 1;
  uint64_t iLastFinished;

  {
    AUTO_LOCK(*m_pMutex);

    // Check if somebody is running an earlier command
    for (const auto &iRunningCommand : m_arrSubProcessInfos) {
      if (iRunningCommand < iCommandNumber) {
        iFinishedByNow = 0;
        break;
      }
    }

    iLastFinished = m_iLastFinished;
    if (iFinishedByNow > m_iLastFinished) {
      m_iLastFinished = iFinishedByNow;
    } else {
      return;
    }
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
    size_t nPackedLength = AssembleWorkerReplyPackage(pInfoBegin, nComboBegin,
                                                      *m_byte_code, &mbPacked);

    if (nPackedLength) {
      uint8_t *pCodeBuffer;
      {
        // Packed buffer
        GLOBAL_DATA_MTX_LOCK_AUTO;
        pCodeBuffer = StaticComboFromDictAdd(*m_byte_code, pInfoBegin->m_szName,
                                             nComboBegin)
                          ->AllocPackedCodeBlock(nPackedLength);
      }

      if (pCodeBuffer) mbPacked.read(pCodeBuffer, nPackedLength);
    }

    // Next iteration
    if (!nComboBegin--) {
      Combo_Free(hChBegin);
      if ((hChBegin =
               se::shader_compile::shader_combo_processor::Combo_GetCombo(
                   pInfoBegin->m_iCommandEnd)) != nullptr) {
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
  se::shader_compile::shader_combo_processor::ComboHandle hThreadCombo;

  uint64_t *iCurrentId;

  {
    AUTO_LOCK(*m_pMutex);
    hThreadCombo = m_hCombo ? Combo_Alloc(m_hCombo) : nullptr;
    iCurrentId = &m_arrSubProcessInfos[m_arrSubProcessInfos.AddToTail()];
  }

  uint64_t iThreadCommand = ~0ULL;

  for (;;) {
    {
      AUTO_LOCK(*m_pMutex);

      if (m_hCombo) {
        Combo_Assign(hThreadCombo, m_hCombo);
        *iCurrentId = Combo_GetCommandNum(hThreadCombo);
        Combo_GetNext(iThreadCommand, m_hCombo, m_iEndCommand);
      } else {
        Combo_Free(hThreadCombo);
        iThreadCommand = ~0ULL;
        *iCurrentId = ~0ULL;
      }
    }

    if (hThreadCombo) {
      ExecuteCompileCommand(hThreadCombo);
    } else
      break;
  }

  Combo_Free(hThreadCombo);
  return false;
}

//
// Processor
//
class RangeProcessor {
 public:
  explicit RangeProcessor(IThreadPool *pool)
      : m_worker{nullptr}, m_thread_pool{pool} {
    // Make sure that our mutex is in multi-threaded mode
    threading::g_mtxGlobal.SetThreadedMode(threading::Mode::MultiThreaded);

    m_worker = new WorkerClass_t(&m_mutex, pool);
  }
  ~RangeProcessor() {
    delete m_worker;

    m_thread_pool->Stop();
    m_thread_pool = nullptr;
  }

  void ProcessCommandRange(uint64_t shaderStart, uint64_t shaderEnd,
                           const char *temp_path, bool is_verbose,
                           CShaderMap &byte_code,
                           CompilerShaderStats &compiler_stats) const;

  //
  // Multi-threaded section
 private:
  using MultiThreadMutex_t = CThreadFastMutex;
  MultiThreadMutex_t m_mutex;

  using WorkerClass_t = CWorkerAccumState<MultiThreadMutex_t>;
  WorkerClass_t *m_worker;

  IThreadPool *m_thread_pool;
};

void RangeProcessor::ProcessCommandRange(
    uint64_t shaderStart, uint64_t shaderEnd, const char *temp_path,
    bool is_verbose, CShaderMap &byte_code,
    CompilerShaderStats &compiler_stats) const {
  if (m_thread_pool) {
    m_worker->RangeBegin(shaderStart, shaderEnd, temp_path, is_verbose,
                         byte_code, compiler_stats);
    m_worker->Run();
    m_worker->RangeFinished();
  }
}

// You must process the work unit range.
void ProcessCommandRange(RangeProcessor &processor, uint64_t shaderStart,
                         uint64_t shaderEnd, const char *temp_path,
                         bool is_verbose, CShaderMap &byte_code,
                         CompilerShaderStats &compiler_stats) {
  processor.ProcessCommandRange(shaderStart, shaderEnd, temp_path, is_verbose,
                                byte_code, compiler_stats);
}

void ParseShaderInfoFromCompileCommands(
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

int GetLocalCopyOfFiles(const char *shader_path, const char *temp_path,
                        IBaseFileSystem *file_system, bool is_verbose) {
  // Create virtual files for all of the stuff that we need to compile the
  // shader make sure and prefix the file name so that it doesn't find it
  // locally.

  char files_to_copy_storage_path[1024];
  V_sprintf_safe(files_to_copy_storage_path, "%s\\uniquefilestocopy.txt",
                 shader_path);

  CUtlInplaceBuffer copy_buffer(0, 0, CUtlBuffer::TEXT_BUFFER);
  if (!file_system->ReadFile(files_to_copy_storage_path, nullptr,
                             copy_buffer)) {
    Warning("Can't open '%s' to read files to copy!\n",
            files_to_copy_storage_path);
    return ENOENT;
  }

  while (char *file_to_copy_name = copy_buffer.InplaceGetLinePtr()) {
    V_MakeAbsolutePath(files_to_copy_storage_path, file_to_copy_name,
                       shader_path);

    DebugOut(is_verbose, "getting local copy of file: \"%s\" (\"%s\").\n",
             file_to_copy_name, files_to_copy_storage_path);

    CUtlBuffer file_buffer;
    if (!file_system->ReadFile(files_to_copy_storage_path, nullptr,
                               file_buffer)) {
      Warning("Can't read \"%s\" to create local copy.\n",
              files_to_copy_storage_path);
      continue;
    }

    // Grab just the filename.
    char copy_file_name[MAX_PATH];
    char *pLastSlash =
        max(strrchr(file_to_copy_name, '/'), strrchr(file_to_copy_name, '\\'));
    if (pLastSlash)
      V_strcpy_safe(copy_file_name, pLastSlash + 1);
    else
      V_strcpy_safe(copy_file_name, file_to_copy_name);

    V_StrTrim(copy_file_name);

    V_sprintf_safe(files_to_copy_storage_path, "%s%s", temp_path,
                   copy_file_name);
    DebugOut(is_verbose, "creating \"%s\".\n", files_to_copy_storage_path);

    {
      auto [fp, rc] = se::posix::posix_file_stream_factory::open(
          files_to_copy_storage_path, "wb");
      if (rc) {
        Error("Can't open '%s' to write a copy to (%d: %s).",
              files_to_copy_storage_path, rc.value(), rc.message().c_str());
        continue;
      }

      const intp bytes_write{file_buffer.GetBytesRemaining()};

      std::tie(std::ignore, rc) = fp.write(file_buffer.Base(), 1, bytes_write);
      if (rc) {
        Error("Can't write %zd bytes to '%s' as copy (%d: %s).", bytes_write,
              files_to_copy_storage_path, rc.value(), rc.message().c_str());
        continue;
      }
    }

    // SUPER EVIL, but if we don't do this, Windows will randomly nuke files of
    // ours while we're running since they're in the temp path.

    static CUtlVector<FILE *> s_arrHackedFiles;
    static struct X_s_arrHackedFiles {
      ~X_s_arrHackedFiles() {
        for (auto *f : s_arrHackedFiles) fclose(f);
      }
    } s_autoCloseHackedFiles;

    /* THIS IS THE EVIL LINE */
    FILE *fHack = fopen(files_to_copy_storage_path, "r");
    s_arrHackedFiles.AddToTail(fHack);
    // -- END of EVIL
  }

  return 0;
}

int GetLocalCopyOfBinary(const char *exe_dir, const char *file_name,
                         const char *temp_path, bool is_verbose) {
  int64_t size;
  CUtlBuffer buffer;

  {
    char source_file_path[MAX_PATH];
    V_sprintf_safe(source_file_path, "%s\\%s", exe_dir, file_name);

    DebugOut(is_verbose, "trying to open: %s.\n", source_file_path);

    auto [fp, rc] =
        se::posix::posix_file_stream_factory::open(source_file_path, "rb");
    if (rc) {
      Warning("Can't open '%s' to read (%d: %s)!\n", source_file_path,
              rc.value(), rc.message().c_str());
      return rc.value();
    }

    std::tie(size, rc) = fp.size();
    if (rc) {
      Warning("Can't read '%s' size (%d: %s)!\n", source_file_path, rc.value(),
              rc.message().c_str());
      return rc.value();
    }
    if (size > std::numeric_limits<int>::max()) {
      Warning("Can't read '%s' as it is too large!\n", source_file_path);
      return EINVAL;
    }

    const intp size_safe = static_cast<intp>(size);
    buffer.EnsureCapacity(size_safe);

    size_t read_bytes;
    std::tie(read_bytes, rc) = fp.read(buffer.Base(), size_safe, 1, size_safe);
    if (rc) {
      Warning("Can't read '%s' size (%d: %s)!\n", source_file_path, rc.value(),
              rc.message().c_str());
      return rc.value();
    }

    buffer.SeekPut(CUtlBuffer::SEEK_HEAD, read_bytes);
  }

  char dest_file_path[MAX_PATH];
  V_sprintf_safe(dest_file_path, "%s%s", temp_path, file_name);

  {
    // File may be already here by GetLocalCopyOfFiles and it is opened, so we
    // can't copy (EACCES).
    auto [fp, rc] =
        se::posix::posix_file_stream_factory::open(dest_file_path, "wb");
    if (rc) {
      if (rc.value() != EACCES) {
        Warning("Can't open '%s' to write copy to (%d: %s)!\n", dest_file_path,
                rc.value(), rc.message().c_str());
        return rc.value();
      }

      // Assume EACCES means file is copied and opened by GetLocalCopyOfFiles
      // and we can't rewrite it.
      return 0;
    }

    std::tie(std::ignore, rc) = fp.write(buffer.Base(), 1, size);
    if (rc) {
      Warning("Can't write %lld bytes to copy to '%s' (%d: %s)!\n", size,
              dest_file_path, rc.value(), rc.message().c_str());
      return rc.value();
    }
  }

  // SUPER EVIL, but if we don't do this, Windows will randomly nuke files of
  // ours while we're running since they're in the temp path.
  (void)fopen(dest_file_path, "r");

  return 0;
}

int GetLocalCopyOfBinaries(const char *exe_dir, const char *temp_path,
                           bool is_verbose) {
  // dimhotepus: As we drop vmpi it is not needed anymore.
  // This is necessary so VMPI doesn't run in SDK
  // int rc = Worker_GetLocalCopyOfBinary(exe_dir, "mysql_wrapper.dll",
  // temp_path, is_verbose);
  // if (rc) return rc;

  int rc = GetLocalCopyOfBinary(exe_dir, "vstdlib.dll", temp_path, is_verbose);
  if (rc) return rc;

  return GetLocalCopyOfBinary(exe_dir, "tier0.dll", temp_path, is_verbose);
}

struct ParseCompileCommandsResult {
  std::unique_ptr<se::shader_compile::shader_combo_processor::CfgEntryInfo[]>
      configs;

  uint64_t static_combos_num;
  uint64_t dynamic_combos_num;
  uint64_t compile_commands_num;
};

std::tuple<ParseCompileCommandsResult, int> ParseListOfCompileCommands(
    const char *shader_path, IBaseFileSystem *file_system) {
  const double parse_start_time = Plat_FloatTime();

  char file_list_path[MAX_PATH];
  sprintf_s(file_list_path, "%s\\filelist.txt", shader_path);

  CUtlInplaceBuffer buffer(0, 0, CUtlInplaceBuffer::TEXT_BUFFER);
  if (!file_system->ReadFile(file_list_path, nullptr, buffer)) {
    Warning("Can't open '%s' to read compilation configurations!\n",
            file_list_path);
    return {ParseCompileCommandsResult{nullptr, 0, 0, 0}, ENOENT};
  }

  auto configs =
      se::shader_compile::shader_combo_processor::ReadConfiguration(&buffer);

  uint64_t shaders_num = 0;
  uint64_t static_combos_num = 0, dynamic_combos_num = 0,
           compile_commands_num = 0;

  for (auto *info = configs.get(); info && info->m_szName; ++info) {
    ++shaders_num;

    static_combos_num += info->m_numStaticCombos;
    dynamic_combos_num += info->m_numDynamicCombos;
    compile_commands_num += info->m_iCommandEnd - info->m_iCommandStart;
  }

  const double parse_end_time{Plat_FloatTime()};

  Msg("Read %s shaders compilation configurations from '%s' in %.2fs.\n",
      PrettyPrintNumber(shaders_num), file_list_path,
      parse_end_time - parse_start_time);

  return {ParseCompileCommandsResult{std::move(configs), static_combos_num,
                                     dynamic_combos_num, compile_commands_num},
          0};
}

template <intp size>
void SetupExeDir(int argc, char **argv, char (&exe_dir)[size]) {
  V_strcpy_safe(exe_dir, argv[0]);
  V_StripFilename(exe_dir);

  if (Q_isempty(exe_dir)) V_strcpy_safe(exe_dir, ".\\");

  V_FixSlashes(exe_dir);
}

template <intp size>
int SetupTempPath(int argc, char **argv, char (&temp_path)[size]) {
  ::GetTempPath(sizeof(temp_path), temp_path);
  strcat_s(temp_path, "shadercompiletemp\\");

  char command[MAX_PATH];
  sprintf_s(command, "rd /s /q \"%s\"", temp_path);

  int rc = system(command);
  if (rc == -1) {
    rc = errno;
    Warning("'%s' execution failed (%d: %s).\n", command, rc,
            std::generic_category().message(rc).c_str());
    return rc;
  }

  // ENOENT when "The system cannot find the file specified."
  if (rc != 0 && rc != ENOENT) {
    Warning("'%s' execution failed w/e %d.\n", command, rc);
    return rc;
  }

  if (_mkdir(temp_path) == -1) {
    rc = errno;
    Warning("mkdir '%s' execution failed (%d: %s).\n", temp_path, rc,
            std::generic_category().message(rc).c_str());
    return rc;
  }

  return 0;
}

void CompileShaders(
    IThreadPool *thread_pool, const char *shader_path, const char *temp_path,
    const std::unique_ptr<
        se::shader_compile::shader_combo_processor::CfgEntryInfo[]> &configs,
    bool is_verbose, CompilerShaderStats &compiler_stats) {
  RangeProcessor processor{thread_pool};

  CUtlStringMap<ShaderInfo_t> shader_info_map;
  char chCommands[32], chStaticCombos[32], chDynamicCombos[32];

  //
  // We will iterate on the cfg entries and process them
  //
  for (auto *pEntry = configs.get(); pEntry && pEntry->m_szName; ++pEntry) {
    //
    // Stick the shader info
    //
    ShaderInfo_t siLastShaderInfo;
    ParseShaderInfoFromCompileCommands(pEntry, siLastShaderInfo);

    shader_info_map[pEntry->m_szName] = siLastShaderInfo;

    {
      V_sprintf_safe(
          chCommands, "%s",
          PrettyPrintNumber(pEntry->m_iCommandEnd - pEntry->m_iCommandStart));
      V_sprintf_safe(chStaticCombos, "%s",
                     PrettyPrintNumber(pEntry->m_numStaticCombos));
      V_sprintf_safe(chDynamicCombos, "%s",
                     PrettyPrintNumber(pEntry->m_numDynamicCombos));

      Msg("Compiling %s commands in %s static, %s dynamic combos in %s...\n",
          chCommands, chStaticCombos, chDynamicCombos, pEntry->m_szName);
    }

    CShaderMap byte_code;

    //
    // Compile stuff
    //
    ProcessCommandRange(processor, pEntry->m_iCommandStart,
                        pEntry->m_iCommandEnd, temp_path, is_verbose, byte_code,
                        compiler_stats);

    //
    // Now when the whole shader is finished we can write it
    //
    char const *shader_name = pEntry->m_szName;

    WriteShaderFiles(shader_path, shader_name, configs, shader_info_map,
                     byte_code, compiler_stats, pEntry->m_iCommandEnd,
                     pEntry->m_iCommandEnd, is_verbose);
  }

  // dimhotepus: Correctly rewrite long strings.
  Msg("\r                                                                \r");
}

class ScopedConsoleCtrlHandler {
 public:
  explicit ScopedConsoleCtrlHandler(PHANDLER_ROUTINE handler) noexcept
      : handler_{handler}, ok_{::SetConsoleCtrlHandler(handler, TRUE)} {
    AssertMsg(ok_, "Unable to set console ctrl handler for process.");
  }
  ~ScopedConsoleCtrlHandler() noexcept {
    ok_ = ::SetConsoleCtrlHandler(handler_, FALSE);
    AssertMsg(ok_, "Unable to reset console ctrl handler for process.");
  }

  ScopedConsoleCtrlHandler(ScopedConsoleCtrlHandler &) = delete;
  ScopedConsoleCtrlHandler &operator=(ScopedConsoleCtrlHandler &) = delete;

 private:
  const PHANDLER_ROUTINE handler_;
  BOOL ok_;
};

enum class ThreadExecutionState : unsigned long {
  None = 0x80000000,
  SystemRequired = 0x00000001,
  DisplayRequired = 0x00000002,
  AwayModeRequired = 0x00000040
};

class ScopedThreadExecutionState {
 public:
  explicit ScopedThreadExecutionState(ThreadExecutionState new_state)
      : old_state_{static_cast<ThreadExecutionState>(::SetThreadExecutionState(
            ES_CONTINUOUS | to_underlying(new_state)))},
        new_state_{new_state} {}
  ~ScopedThreadExecutionState() {
    const auto old_state = ::SetThreadExecutionState(
        ES_CONTINUOUS | ((old_state_ == ThreadExecutionState::None)
                             ? 0
                             : to_underlying(old_state_)));
    AssertMsg(old_state == to_underlying(new_state_),
              "Unbalanced SetThreadExecutionState, smth changed it from "
              "expected 0x%lu to actual 0x%lu.",
              to_underlying(new_state_), old_state);
  }

  ScopedThreadExecutionState(ScopedThreadExecutionState &) = delete;
  ScopedThreadExecutionState &operator=(ScopedThreadExecutionState &) = delete;
  ScopedThreadExecutionState(ScopedThreadExecutionState &&) = delete;
  ScopedThreadExecutionState &operator=(ScopedThreadExecutionState &&) = delete;

 private:
  const ThreadExecutionState old_state_, new_state_;
};

IThreadPool *StartThreadPool(const CPUInformation *cpu) {
  ThreadPoolStartParams_t args;
  args.bIOThreads = false;
  args.nThreads = cpu->m_nLogicalProcessors - 1;

  auto pool = g_pThreadPool;
  if (pool->Start(args)) {
    // Make sure that our mutex is in multi-threaded mode
    threading::g_mtxGlobal.SetThreadedMode(threading::Mode::MultiThreaded);

    // Thread pools threads # + main thread.
    Msg("Using %zd threads to compile shaders.\n", pool->NumThreads() + 1);
    return pool;
  }

  Warning("Unable to start thread pool with %d threads.\n", args.nThreads);
  return nullptr;
}

BOOL WINAPI OnCtrlBreak(DWORD ctrl_type) {
  Warning("Stopping compilation due to Ctrl+C.\n");
  return FALSE;
}

int ShaderCompileMain(int argc, char *argv[]) {
#ifdef PLATFORM_64BITS
  Msg("Valve Software - shadercompile [64 bit] (" __DATE__ ")\n");
#else
  Msg("Valve Software - shadercompile (" __DATE__ ")\n");
#endif

  const double compile_start_time{Plat_FloatTime()};

  // Setting up the minidump handler.
  const se::utils::common::ScopedDefaultMinidumpHandler scoped_minidump_handler;
  const ScopedConsoleCtrlHandler scoped_ctrl_handler{OnCtrlBreak};

  EnableCrashingOnCrashes();
  InstallSpewFunction();

  ThreadSetDebugName("ShaderCompile_Main");

  // Do not go to sleep when compiling.
  const ScopedThreadExecutionState scoped_execution_state{
      ThreadExecutionState::SystemRequired};

  ICommandLine *cmd_line{CommandLine()};
  cmd_line->CreateCmdLine(argc, argv);

  const CPUInformation *cpu = GetCPUInformation();

  {
    Msg("\nCmd line: ");
    for (int i{0}, args_count{cmd_line->ParmCount()}; i < args_count; ++i) {
      Msg("%s ", cmd_line->GetParm(i));
    }
    Msg("\n\n");

    constexpr char kThreadsArg[]{"-threads"};

    if (!cmd_line->HasParm(kThreadsArg)) {
      char threads_arg[12];
      V_to_chars(threads_arg, cpu->m_nLogicalProcessors);

      // Ensure thread pool does not cap threads count to default.
      cmd_line->AppendParm(kThreadsArg, threads_arg);
    }
  }

  IThreadPool *thread_pool = StartThreadPool(cpu);
  if (!thread_pool) return EINVAL;

  char exe_dir[MAX_PATH];
  SetupExeDir(argc, argv, exe_dir);

  char temp_path[MAX_PATH];
  int rc = SetupTempPath(argc, argv, temp_path);
  if (rc) return rc;

  const char *shader_path{cmd_line->ParmValue("-shaderpath", "")};
  const bool is_verbose{cmd_line->FindParm("-verbose") != 0};
  const bool has_game_arg{cmd_line->HasParm("-game")};

  if (!FileSystem_Init(
          nullptr, 0,
          !has_game_arg
              // Used with filesystem_stdio.dll
              ? FS_INIT_COMPATIBILITY_MODE
              // SDK uses this since it only has filesystem_steam.dll.
              : FS_INIT_FULL)) {
    Warning("Unable to init filesystem in %s mode.\n",
            !has_game_arg ? "FS_INIT_COMPATIBILITY_MODE" : "FS_INIT_FULL");
    return EINVAL;
  }

  IBaseFileSystem *file_system = g_pFileSystem;
  DebugOut(is_verbose, "after FileSystem_Init.\n");

  const auto [parseResult, rc2] =
      ParseListOfCompileCommands(shader_path, file_system);
  if (rc2) return rc2;

  DebugOut(is_verbose, "after ParseListOfCompileCommands.\n");

  rc = GetLocalCopyOfFiles(shader_path, temp_path, file_system, is_verbose);
  if (rc) return rc;

  rc = GetLocalCopyOfBinaries(exe_dir, temp_path, is_verbose);
  if (rc) return rc;

  if (_chdir(temp_path) == -1) {
    rc = errno;
    Warning("chdir '%s' failed (%d: %s).\n", temp_path, rc,
            std::generic_category().message(rc).c_str());
    return rc;
  }

  {
    char commands_no[32], static_combos_no[32], dynamic_combos_no[32];

    V_sprintf_safe(commands_no, "%s",
                   PrettyPrintNumber(parseResult.compile_commands_num));
    V_sprintf_safe(static_combos_no, "%s",
                   PrettyPrintNumber(parseResult.static_combos_num));
    V_sprintf_safe(dynamic_combos_no, "%s",
                   PrettyPrintNumber(parseResult.dynamic_combos_num));

    Msg("Compiling %s commands in %s static, %s dynamic combos...\n",
        commands_no, static_combos_no, dynamic_combos_no);
  }

  CompilerShaderStats compiler_stats;
  CompileShaders(thread_pool, shader_path, temp_path, parseResult.configs,
                 is_verbose, compiler_stats);

  Msg("\r                                                                \r");

  // Write all the errors
  //////////////////////////////////////////////////////////////////////////
  //
  // Now deliver all our accumulated spew to the output
  //
  //////////////////////////////////////////////////////////////////////////
  const char *verbose_combo_errors_env{getenv("VALVE_VERBOSE_COMBO_ERRORS")};
  const bool is_verbose_combo_errors{
      verbose_combo_errors_env &&
      strtol(verbose_combo_errors_env, nullptr, 10)};

  char command[4096];
  const auto &shader_message_info_map = compiler_stats.shader_message_info_map;

  // Compiler spew
  for (unsigned short k = 0, kEnd = shader_message_info_map.GetNumStrings();
       k < kEnd; ++k) {
    const char *szMsg = shader_message_info_map.String(k);
    const CompilerMsgInfo &cmi = shader_message_info_map[ushort_as_symid(k)];

    const char *szFirstCmd = cmi.GetFirstCommand();
    const uint64_t numReported = cmi.GetNumTimesReported();

    uint64_t iFirstCommand = strtoull(szFirstCmd, nullptr, 10);

    se::shader_compile::shader_combo_processor::ComboHandle hCombo = nullptr;
    se::shader_compile::shader_combo_processor::CfgEntryInfo const
        *pComboEntryInfo = nullptr;

    if (se::shader_compile::shader_combo_processor::Combo_GetNext(
            iFirstCommand, hCombo, parseResult.compile_commands_num)) {
      Combo_FormatCommand(hCombo, command);
      pComboEntryInfo = Combo_GetEntryInfo(hCombo);
      Combo_Free(hCombo);
    } else {
      sprintf_s(command, "cmd # %s", szFirstCmd);
    }

    Msg("\n%s\n", szMsg);
    Msg("    Reported %llu time(s), example command:\n", numReported);

    if (is_verbose_combo_errors) {
      Msg("    Verbose Description:\n");
      if (pComboEntryInfo) {
        Msg("        Src File: %s\n", pComboEntryInfo->m_szShaderFileName);
        Msg("        Tgt File: %s\n", pComboEntryInfo->m_szName);
      }

      // Between     /DSHADERCOMBO=   and    /Dmain
      char const *pBegin = strstr(command, "/DSHADERCOMBO=");
      char const *pEnd = strstr(command, "/Dmain");
      if (pBegin) {
        pBegin += std::size("/DSHADERCOMBO=") - 1;
        char const *pSpace = strchr(pBegin, ' ');
        if (pSpace)
          Msg("        Combo # : %.*s\n",
              static_cast<unsigned>(pSpace - pBegin), pBegin);
      }

      if (!pEnd) pEnd = command + strlen(command);
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

        Msg("                  %.*s %.*s\n",
            static_cast<unsigned>(pSpace - pEqSign - 1), pEqSign + 1,
            static_cast<unsigned>(pEqSign - pDefine - 2), pDefine + 2);
      }
    }
    Msg("    %s\n", command);
  }

  const auto &shader_had_error_map = compiler_stats.shader_had_error_map;

  // Failed shaders summary
  for (unsigned short k = 0, kEnd = shader_had_error_map.GetNumStrings();
       k < kEnd; ++k) {
    const char *shader_name = shader_had_error_map.String(k);
    if (!shader_had_error_map[ushort_as_symid(k)]) continue;

    Msg("FAILED:    %s\n", shader_name);
  }

  // End
  const double compile_end_time{Plat_FloatTime()};

  if (is_verbose) {
    GetHourMinuteSecondsString(
        static_cast<int>(compile_end_time - compile_start_time), command);

    DebugOut(is_verbose, "%s elapsed.\n", command);
    DebugOut(is_verbose, "precise timing = %.5fs\n",
             (compile_end_time - compile_start_time));
  }

  return shader_had_error_map.GetNumStrings();
}

struct ShaderCompileDLL final : public IShaderCompileDLL {
  int main(int argc, char **argv) override {
    return ShaderCompileMain(argc, argv);
  }
};

struct LaunchableDLL final : public ILaunchableDLL {
  int main(int argc, char **argv) override {
    return ShaderCompileMain(argc, argv);
  }
};

}  // namespace

EXPOSE_SINGLE_INTERFACE(ShaderCompileDLL, IShaderCompileDLL,
                        SHADER_COMPILE_INTERFACE_VERSION);
EXPOSE_SINGLE_INTERFACE(LaunchableDLL, ILaunchableDLL,
                        LAUNCHABLE_DLL_INTERFACE_VERSION);
