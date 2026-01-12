// Copyright Valve Corporation, All rights reserved.
//
// Closed captions, or subtitles, are text descriptions that accompany sound and
// dialogue.  They cue players who can't hear what's going on to what people are
// saying, and to a certain extent what's happening around them.  It can be also
// used for translation and/or localization.  With a bit of ingenuity they can
// also be used to display dialogue that has not been recorded yet.
//
// See https://developer.valvesoftware.com/wiki/Closed_Captions

#include "captioncompiler.h"

#include "appframework/AppFramework.h"
#include "appframework/tier3app.h"

#include "tier0/dbg.h"
#include "tier0/fasttimer.h"
#include "tier0/icommandline.h"

#include "tier1/utldict.h"
#include "tier1/checksum_crc.h"
#include "tier1/utlbuffer.h"
#include "tier1/UtlSortVector.h"
#include "tier1/utlmap.h"

#include "vstdlib/random.h"

#include "vgui/ILocalize.h"
#include "vgui/IVGui.h"
#include "vgui_controls/controls.h"

#include "filesystem.h"
#include "cmdlib.h"

#include "tools_minidump.h"
#include "scoped_app_locale.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

namespace {

SpewRetval_t SpewFunc(SpewType_t type, char const *pMsg) {
  if (type == SPEW_ERROR) {
    fprintf(stderr, "%s", pMsg);
  } else {
    printf("%s", pMsg);
  }

  Plat_DebugString(pMsg);

  if (type == SPEW_ERROR) {
    fprintf(stderr, "\n");
    Plat_DebugString("\n");
  }

  return SPEW_CONTINUE;
}

void vprint(bool uselogfile, int depth, const char *fmt, ...) {
  char string[8192];
  va_list va;
  va_start(va, fmt);
  V_vsprintf_safe(string, fmt, va);
  va_end(va);

  FILE *fp{nullptr};

  if (uselogfile) {
    // dimhotepus: Better log name.
    fp = fopen("caption-compiler-log.txt", "ab");
  }

  while (depth-- > 0) {
    printf("  ");
    Plat_DebugString("  ");

    if (fp) {
      fprintf(fp, "  ");
    }
  }

  printf("%s", string);
  Plat_DebugString(string);

  if (fp) {
    RunCodeAtScopeExit(fclose(fp));

    char *p = string;
    while (*p) {
      if (*p == '\n') {
        fputc('\r', fp);
      }
      fputc(*p, fp);
      p++;
    }
  }
}

void printusage(bool uselogfile) {
  vprint(uselogfile, 0,
         "usage:  captioncompiler closecaptionfile.txt\n \
        \t-v = verbose output\n \
        \t-l = log to file caption-compiler-log.txt\n \
        \ne.g.:  kvc -l u:/xbox/game/hl2x/resource/closecaption_english.txt");

  // Exit app
  exit(1);
}

void CheckLogFile(bool uselogfile) {
  if (uselogfile) {
    _unlink("caption-compiler-log.txt");
    vprint(uselogfile, 0, "    Outputting to caption-compiler-log.txt\n");
  }
}

void PrintHeader(bool uselogfile) {
#ifdef PLATFORM_64BITS
  vprint(uselogfile, 0, "Valve Software - captioncompiler (%s)\n", __DATE__);
#else
  vprint(uselogfile, 0, "Valve Software - captioncompiler [64 bit] (%s)\n",
         __DATE__);
#endif
  vprint(uselogfile, 0, "--- Close Caption File compiler ---\n");
}

// The application object
class CompileCaptionsApp : public CTier3SteamApp {
  typedef CTier3SteamApp BaseClass;

 public:
  CompileCaptionsApp() : scoped_spew_output_{SpewFunc}, m_UseLogFile{false} {}

  // Methods of IApplication
  bool Create() override;
  bool PreInit() override;
  int Main() override;
  void PostShutdown() override;
  void Destroy() override { SpewDeactivate(); }

 private:
  // Sets up the search paths
  bool SetupSearchPaths();

  void CompileCaptionFile(char const *infile, char const *outfile);
  void DescribeCaptions(char const *file) const;

  // Install an exception handler.
  const se::utils::common::ScopedDefaultMinidumpHandler
      scoped_default_minidumps_;
  const ScopedSpewOutputFunc scoped_spew_output_;

  // dimhotepus: Apply en_US UTF8 locale for printf/scanf.
  //
  // Printf/sscanf functions expect en_US UTF8 localization.
  //
  // Starting in Windows 10 version 1803 (10.0.17134.0), the Universal C Runtime
  // supports using a UTF-8 code page.
  static constexpr char kEnUsUtf8Locale[]{"en_US.UTF-8"};
  const se::ScopedAppLocale scoped_app_locale{kEnUsUtf8Locale};

  bool m_UseLogFile;
};

bool CompileCaptionsApp::Create() {
  SpewActivate("kvc", 2);

  // dimhotepus: Apply en_US UTF8 locale for printf/scanf.
  //
  // Printf/sscanf functions expect en_US UTF8 localization.
  //
  // Starting in Windows 10 version 1803 (10.0.17134.0), the Universal C Runtime
  // supports using a UTF-8 code page.
  if (V_stricmp(se::ScopedAppLocale::GetCurrentLocale(), kEnUsUtf8Locale)) {
    Warning("setlocale('%s') failed, current locale is '%s'.\n",
            kEnUsUtf8Locale, se::ScopedAppLocale::GetCurrentLocale());
  }

  AppSystemInfo_t appSystems[] = {
      {"vgui2" DLL_EXT_STRING, VGUI_IVGUI_INTERFACE_VERSION}, {"", ""}
      // Required to terminate the list
  };

  return AddSystems(appSystems);
}

// Sets up the game path
bool CompileCaptionsApp::SetupSearchPaths() {
  if (!BaseClass::SetupSearchPaths(NULL, false, true)) return false;

  // Set gamedir.
  V_MakeAbsolutePath(gamedir, GetGameInfoPath());
  V_AppendSlash(gamedir);

  return true;
}

// Init, shutdown
bool CompileCaptionsApp::PreInit() {
  if (!BaseClass::PreInit()) return false;

  g_pFileSystem = g_pFullFileSystem;
  if (!g_pFileSystem || !g_pVGui || !g_pVGuiLocalize) {
    Error("Unable to load required library interface!\n");
    return false;
  }

  // MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );
  g_pFullFileSystem->SetWarningFunc(Warning);

  // Add paths...
  if (!SetupSearchPaths()) return false;

  return true;
}

void CompileCaptionsApp::PostShutdown() {
  g_pFileSystem = nullptr;

  BaseClass::PostShutdown();
}

void CompileCaptionsApp::CompileCaptionFile(char const *infile,
                                            char const *outfile) {
  StringIndex_t maxindex = INVALID_LOCALIZE_STRING_INDEX;
  intp maxunicodesize = 0;
  intp totalsize = 0;

  intp c = 0;

  intp curblock = 0;
  intp usedBytes = 0;
  constexpr short blockSize = MAX_BLOCK_SIZE;

  intp freeSpace = 0;

  CUtlVector<CaptionLookup_t> directory;
  CUtlBuffer data;

  CUtlRBTree<unsigned int> hashcollision(0, 0, DefLessFunc(unsigned int));

  for (auto i = g_pVGuiLocalize->GetFirstStringIndex();
       i != INVALID_LOCALIZE_STRING_INDEX;
       i = g_pVGuiLocalize->GetNextStringIndex(i), ++c) {
    char const *entryName{g_pVGuiLocalize->GetNameByIndex(i)};

    CaptionLookup_t entry = {};
    entry.SetHash(entryName);

    // 	vprint( 0, "%d / %d:  %s == %u\n", c, i,
    // g_pVGuiLocalize->GetNameByIndex( i ), entry.hash );

    if (hashcollision.Find(entry.hash) != hashcollision.InvalidIndex()) {
      Error("Name hash collision detected for %s!!!\n",
            g_pVGuiLocalize->GetNameByIndex(i));
    }

    hashcollision.Insert(entry.hash);

    const wchar_t *text{g_pVGuiLocalize->GetValueByIndex(i)};
    if (verbose) {
      vprint(m_UseLogFile, 0, "Processing: '%30.30s' = '%S'\n", entryName,
             text);
    }

    intp len = text ? (wcslen(text) + 1) * sizeof(short) : 0;
    if (len > maxunicodesize) {
      maxindex = i;
      maxunicodesize = len;
    }

    if (len > blockSize) {
      Error(
          "Caption text file '%s' contains a single caption '%s' of %zi bytes "
          "(%hhd is max), change MAX_BLOCK_SIZE in captioncompiler.h to "
          "fix!!!\n",
          g_pVGuiLocalize->GetNameByIndex(i), entryName, len, blockSize);
    }

    totalsize += len;

    if (usedBytes + len >= blockSize) {
      ++curblock;

      intp leftover = (blockSize - usedBytes);

      totalsize += leftover;
      freeSpace += leftover;

      while (--leftover >= 0) {
        data.PutChar(0);
      }

      usedBytes = len;
      entry.offset = 0;

      data.Put(text, len);
    } else {
      entry.offset = usedBytes;
      usedBytes += len;
      data.Put(text, len);
    }

    entry.length = len;
    entry.blockNum = curblock;

    directory.AddToTail(entry);
  }

  intp leftover = (blockSize - usedBytes);
  totalsize += leftover;
  freeSpace += leftover;

  while (--leftover >= 0) {
    data.PutChar(0);
  }

  vprint(m_UseLogFile, 0, "Found %zi strings in '%s'\n", c, infile);

  if (maxindex != INVALID_LOCALIZE_STRING_INDEX) {
    vprint(m_UseLogFile, 0,
           "Longest string '%s' = (%zi) bytes average(%.3f)\n%",
           g_pVGuiLocalize->GetNameByIndex(maxindex), maxunicodesize,
           (float)totalsize / c);
  }

  vprint(
      m_UseLogFile, 0,
      "%zi blocks (%zi bytes each), %zi bytes wasted (%.3f per block average), "
      "total bytes %zi\n",
      curblock + 1, blockSize, freeSpace, (float)freeSpace / (curblock + 1),
      totalsize);

  vprint(m_UseLogFile, 0,
         "directory size %zi entries, %zu bytes, data size %zi bytes\n",
         directory.Count(), directory.Count() * sizeof(CaptionLookup_t),
         data.TellPut());

  CompiledCaptionHeader_t header = {};
  header.magic = COMPILED_CAPTION_FILEID;
  header.version = COMPILED_CAPTION_VERSION;
  header.numblocks = curblock + 1;
  header.blocksize = blockSize;
  header.directorysize = directory.Count();
  header.dataoffset = 0;

  // Now write the outfile
  CUtlBuffer out;
  out.Put(&header, sizeof(header));
  out.Put(directory.Base(), directory.Count() * sizeof(CaptionLookup_t));
  intp curOffset = out.TellPut();
  // Round it up to the next 512 byte boundary
  intp nBytesDestBuffer = AlignValue(curOffset, 512);  // align to HD sector
  intp nPadding = nBytesDestBuffer - curOffset;
  while (--nPadding >= 0) {
    out.PutChar(0);
  }
  out.Put(data.Base(), data.TellPut());

  // Write out a corrected header
  header.dataoffset = nBytesDestBuffer;
  intp savePos = out.TellPut();
  out.SeekPut(CUtlBuffer::SEEK_HEAD, 0);
  out.Put(&header, sizeof(header));
  out.SeekPut(CUtlBuffer::SEEK_HEAD, savePos);

  g_pFullFileSystem->WriteFile(outfile, NULL, out);
}

void CompileCaptionsApp::DescribeCaptions(char const *file) const {
  CUtlBuffer buf;
  if (!g_pFullFileSystem->ReadFile(file, NULL, buf)) {
    Error("Unable to read '%s' into buffer\n", file);
  }

  CompiledCaptionHeader_t header;
  buf.Get(&header, sizeof(header));

  if (header.magic != COMPILED_CAPTION_FILEID)
    Error("Invalid file id 0x%x != 0x%x for %s\n", header.magic,
          COMPILED_CAPTION_FILEID, file);

  if (header.version != COMPILED_CAPTION_VERSION)
    Error("Invalid file version 0x%x != 0x%x for %s\n", header.version,
          COMPILED_CAPTION_VERSION, file);

  // Read the directory
  CUtlSortVector<CaptionLookup_t, CCaptionLookupLess> directory;
  directory.EnsureCapacity(header.directorysize);
  directory.CopyArray(static_cast<const CaptionLookup_t *>(buf.PeekGet()),
                      header.directorysize);
  directory.RedoSort(true);
  buf.SeekGet(CUtlBuffer::SEEK_HEAD, header.dataoffset);

  CUtlVector<CaptionBlock_t> blocks;
  for (int i = 0; i < header.numblocks; ++i) {
    CaptionBlock_t &newBlock = blocks[blocks.AddToTail()];
    Q_memset(newBlock.data, 0, sizeof(newBlock.data));
    buf.Get(newBlock.data, header.blocksize);
  }

  CUtlMap<unsigned int, StringIndex_t> inverseMap(0, 0,
                                                  DefLessFunc(unsigned int));
  for (auto idx = g_pVGuiLocalize->GetFirstStringIndex();
       idx != INVALID_LOCALIZE_STRING_INDEX;
       idx = g_pVGuiLocalize->GetNextStringIndex(idx)) {
    const char *name = g_pVGuiLocalize->GetNameByIndex(idx);

    CaptionLookup_t dummy = {};
    dummy.SetHash(name);

    inverseMap.Insert(dummy.hash, idx);
  }

  // Now print everything out...
  for (int i = 0; i < header.directorysize; ++i) {
    const CaptionLookup_t &entry = directory[i];
    char const *name = g_pVGuiLocalize->GetNameByIndex(
        inverseMap.Element(inverseMap.Find(entry.hash)));
    const CaptionBlock_t &block = blocks[entry.blockNum];

    const wchar_t *data = (const wchar_t *)&block.data[entry.offset];
    wchar_t *temp = (wchar_t *)_alloca(entry.length * sizeof(short));
    wcsncpy(temp, data, (entry.length / sizeof(short)) - 1);

    vprint(m_UseLogFile, 0,
           "%3.3d:  (%40.40s) hash(%15.15u), block(%4.4d), offset(%4.4d), "
           "len(%4.4d) %S\n",
           i, name, entry.hash, entry.blockNum, entry.offset, entry.length,
           temp);
  }
}

// main application
int CompileCaptionsApp::Main() {
  CUtlVector<CUtlSymbol> worklist;

  int i{0};
  for (; i < CommandLine()->ParmCount(); i++) {
    const char *arg{CommandLine()->GetParm(i)};

    if (arg[0] == '-') {
      switch (arg[1]) {
        case 'l':
          m_UseLogFile = true;
          break;
        case 'v':
          verbose = true;
          break;
        // dimhotepus: Drop XBOX support.
        // case 'x':
        //   bX360 = true;
        //   break;
        case 'g':  // -game
          ++i;
          break;
        default:
          printusage(m_UseLogFile);
          break;
      }
    } else if (i != 0) {
      char fn[MAX_PATH];
      V_strcpy_safe(fn, CommandLine()->GetParm(i));
      Q_FixSlashes(fn);
      Q_strlower(fn);

      worklist.AddToTail(CUtlSymbol{fn});
    }
  }

  if (CommandLine()->ParmCount() < 2 || (i != CommandLine()->ParmCount()) ||
      worklist.Count() != 1) {
    PrintHeader(m_UseLogFile);
    printusage(m_UseLogFile);
  }

  CheckLogFile(m_UseLogFile);

  PrintHeader(m_UseLogFile);

  char binaries[MAX_PATH];
  V_strcpy_safe(binaries, gamedir);
  Q_StripTrailingSlash(binaries);
  // dimhotepus: x86-64 support.
  V_strcat_safe(binaries, CORRECT_PATH_SEPARATOR_S ".." CORRECT_PATH_SEPARATOR_S PLATFORM_BIN_DIR );

  char outfile[MAX_PATH];
  const char *lastCaption = worklist[worklist.Count() - 1].String();

  if (Q_stristr(lastCaption, gamedir)) {
    V_strcpy_safe(outfile, &lastCaption[V_strlen(gamedir)]);
  } else {
    V_sprintf_safe(outfile, "resource\\%s", lastCaption);
  }

  char infile[MAX_PATH];
  V_strcpy_safe(infile, outfile);

  Q_SetExtension(outfile, ".dat");

  vprint(m_UseLogFile, 0, "gamedir[ %s ]\n", gamedir);
  vprint(m_UseLogFile, 0, "infile[ %s ]\n", infile);
  vprint(m_UseLogFile, 0, "outfile[ %s ]\n", outfile);

  g_pFullFileSystem->AddSearchPath(binaries, "EXECUTABLE_PATH");

  if (!g_pVGuiLocalize->AddFile(infile, "MOD", false)) {
    Error("Unable to add localization file '%s'\n", infile);
  }

  vprint(m_UseLogFile, 0, "    Compiling Captions for '%s'...\n", infile);

  CompileCaptionFile(infile, outfile);

  if (verbose) {
    DescribeCaptions(outfile);
  }

  g_pVGuiLocalize->RemoveAll();

  return 0;
}

}  // namespace

DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT(CompileCaptionsApp)
