// Copyright Valve Corporation, All rights reserved.

#ifndef SE_UTILS_VPROJTOMAKE_MAKEFILECREATOR_H_
#define SE_UTILS_VPROJTOMAKE_MAKEFILECREATOR_H_

#include "tier1/utlvector.h"
#include "tier1/utlsymbol.h"
#include "tier1/utldict.h"
#include "tier1/utlmap.h"
#include "vcprojconvert.h"
#include "filesystem.h"

namespace se::vprojtomake {

class CMakefileCreator {
 public:
  CMakefileCreator();
  ~CMakefileCreator();

  bool CreateMakefiles(CVCProjConvert &proj);

 private:
  void CleanupFileName(char *name);
  bool OutputDirs(FileHandle_t f);
  bool OutputBuildTarget(FileHandle_t f);
  bool OutputObjLists(CVCProjConvert::CConfiguration &config, FileHandle_t f);
  bool OutputIncludes(CVCProjConvert::CConfiguration &config, FileHandle_t f);
  bool OutputMainBuilder(FileHandle_t f);

  void CreateBaseDirs(CVCProjConvert::CConfiguration &config);
  void CreateMakefileName(const char *projectName,
                          CVCProjConvert::CConfiguration &config);
  void CreateDirectoryFriendlyName(const char *dirName, char *friendlyDirName,
                                   int friendlyDirNameSize);
  void CreateObjDirectoryFriendlyName(char *name);
  bool FileWrite(FileHandle_t f, PRINTF_FORMAT_STRING const char *fmt, ...);

  CUtlDict<CUtlSymbol, int> m_BaseDirs;
  CUtlMap<intp, int> m_FileToBaseDirMapping;

  struct OutputDirMapping_t {
    CUtlSymbol m_SrcDir;
    CUtlSymbol m_ObjDir;
    CUtlSymbol m_ObjName;
    CUtlSymbol m_ObjOutputDir;
    int m_iBaseDirIndex;
  };

  CUtlVector<struct OutputDirMapping_t> m_BuildDirectories;
  CUtlSymbol m_MakefileName;
  CUtlSymbol m_ProjName;
  CUtlSymbol m_BaseDir;
};

}  // namespace se::vprojtomake

#endif  // !SE_UTILS_VPROJTOMAKE_MAKEFILECREATOR_H_
