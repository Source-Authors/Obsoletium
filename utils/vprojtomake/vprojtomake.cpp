// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"

#include "tier1/KeyValues.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "tier0/dbg.h"
#include "cmdlib.h"
#include "vcprojconvert.h"
#include "makefilecreator.h"

#ifdef _WIN32
#include "winlite.h"
#endif

namespace {

SpewRetval_t SpewFunc(SpewType_t type, char const *pMsg) {
  if (type == SPEW_ERROR) {
    fprintf(stderr, "%s", pMsg);
  } else {
    printf("%s", pMsg);
  }

#ifdef _WIN32
  OutputDebugString(pMsg);
#endif

  if (type == SPEW_ERROR) {
    fprintf(stderr, "\n");

#ifdef _WIN32
    OutputDebugString("\n");
#endif
  } else if (type == SPEW_ASSERT) {
    return SPEW_DEBUGGER;
  }

  return SPEW_CONTINUE;
}

class MyFileSystem : public IBaseFileSystem {
 public:
  int Read(void *pOutput, int size, FileHandle_t file) override {
    return fread(pOutput, 1, size, (FILE *)file);
  }
  int Write(void const *pInput, int size, FileHandle_t file) override {
    return fwrite(pInput, 1, size, (FILE *)file);
  }
  FileHandle_t Open(const char *pFileName, const char *pOptions,
                    [[maybe_unused]] const char *pathID = 0) override {
    return (FileHandle_t)fopen(pFileName, pOptions);
  }
  void Close(FileHandle_t file) override { fclose((FILE *)file); }
  void Seek(FileHandle_t, int, FileSystemSeek_t) override {}
  unsigned Tell(FileHandle_t) override { return 0; }
  unsigned Size(FileHandle_t) override { return 0; }
  unsigned Size(const char *,
                [[maybe_unused]] const char *pPathID = nullptr) override {
    return 0;
  }
  void Flush(FileHandle_t file) override { fflush((FILE *)file); }
  bool Precache(const char *,
                [[maybe_unused]] const char *pPathID = 0) override {
    return false;
  }
  bool FileExists(const char *,
                  [[maybe_unused]] const char *pPathID = 0) override {
    return false;
  }
  bool IsFileWritable(char const *,
                      [[maybe_unused]] const char *pPathID = 0) override {
    return false;
  }
  bool SetFileWritable(char const *, bool,
                       [[maybe_unused]] const char *pPathID = 0) override {
    return false;
  }
  long GetFileTime(const char *,
                   [[maybe_unused]] const char *pPathID = 0) override {
    return 0;
  }
  bool ReadFile(const char *, const char *, CUtlBuffer &,
                [[maybe_unused]] int nMaxBytes = 0,
                [[maybe_unused]] int nStartingByte = 0,
                [[maybe_unused]] FSAllocFunc_t pfnAlloc = NULL) override {
    return false;
  }
  bool WriteFile(const char *, const char *, CUtlBuffer &) override {
    return false;
  }
  bool UnzipFile(const char *, const char *, const char *) override {
    return false;
  }
};

MyFileSystem g_MyFS;

// Purpose: help text
void printusage() { Warning("usage:  vcprojtomake <vcxproj filename> \n"); }

// Debug helper, spits out a human readable keyvalues version of the various
// configs
bool OutputKeyValuesVersion(se::vprojtomake::CVCProjConvert &proj,
                            const char *fileName) {
  auto kv = KeyValues::AutoDelete("project");
  if (!kv) return false;

  for (intp projIdx = 0; projIdx < proj.GetNumConfigurations(); projIdx++) {
    auto &config = proj.GetConfiguration(projIdx);
    KeyValues *configKv = kv->FindKey(config.GetName().String(), true);

    int fileCount = 0;
    char num[20];

    for (intp fileIdx = 0; fileIdx < config.GetNumFileNames(); fileIdx++) {
      const auto fileType = config.GetFileType(fileIdx);

      if (fileType ==
          se::vprojtomake::CVCProjConvert::CConfiguration::FILE_SOURCE) {
        V_sprintf_safe(num, "%i", fileCount);

        ++fileCount;

        configKv->SetString(num, config.GetFileName(fileIdx));
      }
    }
  }

  return kv->SaveToFile(g_pFileSystem, fileName);
}

}  // namespace

IBaseFileSystem *g_pFileSystem = &g_MyFS;

int main(int argc, char *argv[]) {
  SpewOutputFunc(SpewFunc);

  Msg("Valve Software - vcprojtomake.exe (%s)\n", __DATE__);

  ICommandLine *cmd{CommandLine()};
  cmd->CreateCmdLine(argc, argv);

  if (cmd->ParmCount() < 2) {
    printusage();
    return 0;
  }

  se::vprojtomake::CVCProjConvert converter;
  if (!converter.LoadProject(cmd->GetParm(1))) {
    Warning("Unable to load Visual Studio Project '%s'.\n", cmd->GetParm(1));
    return -1;
  }

  if (constexpr char fileName[] = "files.vdf";
      !OutputKeyValuesVersion(converter, fileName)) {
    Warning("Unable to dump Visual Studio Project '%s' data to '%s'.\n",
            cmd->GetParm(1), fileName);
  }

  int rc = 0;

  se::vprojtomake::CMakefileCreator makefile;
  if (!makefile.CreateMakefiles(converter)) {
    Warning(
        "Unable to create one or more Makefile(s) for Visual Studio Project "
        "'%s'.\n",
        cmd->GetParm(1));
    rc = 1;
  }

  SpewOutputFunc(nullptr);
  return rc;
}
