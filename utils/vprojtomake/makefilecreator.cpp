// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"
#include "makefilecreator.h"
#include "cmdlib.h"

#include <string_view>

namespace se::vprojtomake {

CMakefileCreator::CMakefileCreator() {
  m_FileToBaseDirMapping.SetLessFunc(DefLessFunc(intp));
}

CMakefileCreator::~CMakefileCreator() = default;

bool CMakefileCreator::CreateMakefiles(CVCProjConvert &proj) {
  m_ProjName = proj.GetName();
  m_BaseDir = proj.GetBaseDir();

  bool ok = true, isSkippedMakefile = false;

  for (intp i = 0; i < proj.GetNumConfigurations(); i++) {
    m_FileToBaseDirMapping.RemoveAll();
    m_BuildDirectories.RemoveAll();
    m_BaseDirs.RemoveAll();

    CreateMakefileName(proj.GetName().String(), proj.GetConfiguration(i));
    CreateBaseDirs(proj.GetConfiguration(i));

    FileHandle_t f = g_pFileSystem->Open(m_MakefileName.String(), "w+");
    if (!f) {
      Warning("Unable to open Makefile '%s' for writing.\n",
              m_MakefileName.String());
      isSkippedMakefile = true;
      continue;
    }

    ok = ok && OutputDirs(f);
    ok = ok && OutputIncludes(proj.GetConfiguration(i), f);
    ok = ok && OutputObjLists(proj.GetConfiguration(i), f);
    ok = ok && OutputMainBuilder(f);
    ok = ok && OutputBuildTarget(f);

    g_pFileSystem->Close(f);
  }

  return ok && !isSkippedMakefile;
}

void CMakefileCreator::CreateBaseDirs(CVCProjConvert::CConfiguration &config) {
  char basedir[MAX_PATH], fulldir[MAX_PATH];

  for (intp i = 0; i < config.GetNumFileNames(); i++) {
    if (config.GetFileType(i) == CVCProjConvert::CConfiguration::FILE_SOURCE) {
      V_sprintf_safe(fulldir, "%s/%s", m_BaseDir.String(),
                     config.GetFileName(i));

      if (Q_ExtractFilePath(fulldir, basedir, sizeof(basedir))) {
        Q_FixSlashes(basedir);
        Q_StripTrailingSlash(basedir);

        auto index = m_BaseDirs.Find(basedir);
        if (index == m_BaseDirs.InvalidIndex()) {
          index = m_BaseDirs.Insert(basedir);
        }

        m_FileToBaseDirMapping.Insert(i, index);
      } else {
        m_FileToBaseDirMapping.Insert(i, 0);
      }
    }
  }
}

void CMakefileCreator::CleanupFileName(char *name) {
  for (intp i = Q_strlen(name) - 1; i >= 0; --i) {
    if (name[i] == ' ' || name[i] == '|' || name[i] == '\\' || name[i] == '/' ||
        (name[i] == '.' && i >= 1 && name[i - 1] == '.')) {
      Q_memmove(&name[i], &name[i + 1], Q_strlen(name) - i - 1);

      name[Q_strlen(name) - 1] = 0;
    }
  }
}

void CMakefileCreator::CreateMakefileName(
    const char *projectName, CVCProjConvert::CConfiguration &config) {
  char makefileName[MAX_PATH];
  V_sprintf_safe(makefileName, "Makefile.%s_%s", projectName,
                 config.GetName().String());

  CleanupFileName(makefileName);

  m_MakefileName = makefileName;
}

void CMakefileCreator::CreateDirectoryFriendlyName(const char *dirName,
                                                   char *friendlyDirName,
                                                   int friendlyDirNameSize) {
  Q_strncpy(friendlyDirName, dirName, friendlyDirNameSize);

  intp i;
  for (i = Q_strlen(friendlyDirName) - 1; i >= 0; --i) {
    if (friendlyDirName[i] == '/' || friendlyDirName[i] == '\\') {
      friendlyDirName[i] = '_';
    }

    if (isalpha(friendlyDirName[i])) {
      friendlyDirName[i] = static_cast<unsigned char>(
          toupper(static_cast<unsigned char>(friendlyDirName[i])));
    }

    if (friendlyDirName[i] == '.') {
      Q_memmove(&friendlyDirName[i], &friendlyDirName[i + 1],
                Q_strlen(friendlyDirName) - i - 1);
      friendlyDirName[Q_strlen(friendlyDirName) - 1] = 0;
    }
  }

  // strip any leading/trailing underscores
  while (friendlyDirName[0] == '_' && Q_strlen(friendlyDirName) > 0) {
    Q_memmove(&friendlyDirName[0], &friendlyDirName[1],
              Q_strlen(friendlyDirName) - 1);
    friendlyDirName[Q_strlen(friendlyDirName) - 1] = 0;
  }

  while (Q_strlen(friendlyDirName) > 0 &&
         friendlyDirName[Q_strlen(friendlyDirName) - 1] == '_') {
    friendlyDirName[Q_strlen(friendlyDirName) - 1] = 0;
  }

  CleanupFileName(friendlyDirName);
}

void CMakefileCreator::CreateObjDirectoryFriendlyName(char *name) {
#ifdef _WIN32
  constexpr char updir[] = "..\\";
#else
  constexpr char updir[] = "../";
#endif

  constexpr size_t updirlen{std::size(updir) - 1};

  char *sep = Q_strstr(name, updir);
  while (sep) {
    Q_strcpy(sep, sep + updirlen);

    sep = Q_strstr(sep, updir);
  }
}

bool CMakefileCreator::FileWrite(FileHandle_t f,
                                 PRINTF_FORMAT_STRING const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char stringBuf[4096];
  V_sprintf_safe(stringBuf, fmt, args);
  va_end(args);

  const ptrdiff_t size = Q_strlen(stringBuf);
  const bool ok = g_pFileSystem->Write(stringBuf, size, f) == size;

  if (!ok) {
    Warning("Unable to write '%s' to Makefile '%s'.\n", stringBuf,
            m_MakefileName.String());
  }

  return ok;
}

bool CMakefileCreator::OutputIncludes(CVCProjConvert::CConfiguration &config,
                                      FileHandle_t f) {
  bool ok = FileWrite(f, "INCLUDES=");
  for (intp i = 0; i < config.GetNumIncludes(); i++) {
    ok = ok && FileWrite(f, "-I%s ", config.GetInclude(i));
  }

  for (intp i = 0; i < config.GetNumDefines(); i++) {
    ok = ok && FileWrite(f, "-D%s ", config.GetDefine(i));
  }

  return ok && FileWrite(f, "\n\n");
}

bool CMakefileCreator::OutputDirs(FileHandle_t f) {
  bool ok = true;

  for (auto i = m_BaseDirs.First(); i != m_BaseDirs.InvalidIndex();
       i = m_BaseDirs.Next(i)) {
    const char *dirName = m_BaseDirs.GetElementName(i);
    if (!dirName || !Q_strlen(dirName)) dirName = m_BaseDir.String();

    char friendlyDirName[MAX_PATH];
    CreateDirectoryFriendlyName(dirName, friendlyDirName,
                                sizeof(friendlyDirName));
    V_strcat_safe(friendlyDirName, "_SRC_DIR");

    OutputDirMapping_t dirs;
    dirs.m_SrcDir = friendlyDirName;
    dirs.m_iBaseDirIndex = i;

    V_strcat_safe(friendlyDirName, "_OBJ_DIR");
    dirs.m_ObjDir = friendlyDirName;

    V_strcat_safe(friendlyDirName, "_OBJS");
    dirs.m_ObjName = friendlyDirName;

    char objDirName[MAX_PATH];
    V_sprintf_safe(objDirName, "obj%c$(NAME)_$(ARCH)%c", CORRECT_PATH_SEPARATOR,
                   CORRECT_PATH_SEPARATOR);
    V_strcat_safe(objDirName, dirName);

    CreateObjDirectoryFriendlyName(objDirName);
    dirs.m_ObjOutputDir = objDirName;

    m_BuildDirectories.AddToTail(dirs);

    ok = ok && FileWrite(f, "%s=%s\n", dirs.m_SrcDir.String(), dirName);
    ok = ok && FileWrite(f, "%s=%s\n", dirs.m_ObjDir.String(), objDirName);
  }

  return ok && FileWrite(f, "\n\n");
}

bool CMakefileCreator::OutputMainBuilder(FileHandle_t f) {
  bool ok = true;

  intp i;
  ok = ok && FileWrite(f, "\n\nall: dirs $(NAME)_$(ARCH).$(SHLIBEXT)\n\n");
  ok = ok && FileWrite(f, "dirs:\n");
  for (i = 0; i < m_BuildDirectories.Count(); i++) {
    ok = ok && FileWrite(f, "\t-mkdir -p $(%s)\n",
                         m_BuildDirectories[i].m_ObjDir.String());
  }
  ok = ok && FileWrite(f, "\n\n");

  ok = ok && FileWrite(f, "\n\n$(NAME)_$(ARCH).$(SHLIBEXT): ");
  for (i = 0; i < m_BuildDirectories.Count(); i++) {
    ok = ok && FileWrite(f, "$(%s) ", m_BuildDirectories[i].m_ObjName.String());
  }

  ok =
      ok &&
      FileWrite(f, "\n\t$(CLINK) $(SHLIBLDFLAGS) $(DEBUG) -o $(BUILD_DIR)/$@ ");
  for (i = 0; i < m_BuildDirectories.Count(); i++) {
    ok = ok && FileWrite(f, "$(%s) ", m_BuildDirectories[i].m_ObjName.String());
  }

  return ok && FileWrite(f, "$(LDFLAGS) $(CPP_LIB)\n\n");
}

bool CMakefileCreator::OutputObjLists(CVCProjConvert::CConfiguration &config,
                                      FileHandle_t f) {
  bool ok = true;

  for (auto &dirs : m_BuildDirectories) {
    ok = ok && FileWrite(f, "%s= \\\n", dirs.m_ObjName.String());

    for (auto j = m_FileToBaseDirMapping.FirstInorder();
         j != m_FileToBaseDirMapping.InvalidIndex();
         j = m_FileToBaseDirMapping.NextInorder(j)) {
      if (dirs.m_iBaseDirIndex == m_FileToBaseDirMapping[j]) {
        char baseName[MAX_PATH];
        const char *fileName =
            config.GetFileName(m_FileToBaseDirMapping.Key(j));

        Q_FileBase(fileName, baseName, sizeof(baseName));
        Q_SetExtension(baseName, ".o", sizeof(baseName));

        ok = ok &&
             FileWrite(f, "\t$(%s)/%s \\\n", dirs.m_ObjDir.String(), baseName);
      }
    }

    ok = ok && FileWrite(f, "\n\n");
  }

  return ok;
}

bool CMakefileCreator::OutputBuildTarget(FileHandle_t f) {
  bool ok = true;

  for (auto &dirs : m_BuildDirectories) {
    ok = ok && FileWrite(f, "$(%s)/%%.o: $(%s)/%%.cpp\n",
                         dirs.m_ObjDir.String(), dirs.m_SrcDir.String());

    ok = ok && FileWrite(f, "\t$(DO_CC)\n\n");
  }

  return ok;
}

}  // namespace se::vprojtomake
