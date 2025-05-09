﻿// Copyright Valve Corporation, All rights reserved.
//
// Command-line tool that allows arbitrary files to be embedded within a BSP.
// When the map is being loaded the files will be mounted as if they were
// present in the game's real content folders.
//
// See https://developer.valvesoftware.com/wiki/BSPZIP

#include "bsplib.h"
#include "cmdlib.h"
#include "tier0/icommandline.h"
#include "tier1/utlbuffer.h"
#include "tools_minidump.h"

namespace {

template <intp bufLength>
void StripPath(const char *pPath, char (&pBuf)[bufLength]) {
  // want filename only
  const char *pSrc = pPath + V_strlen(pPath) - 1;

  while ((pSrc != pPath) && (*(pSrc - 1) != '\\') && (*(pSrc - 1) != '/') &&
         (*(pSrc - 1) != ':'))
    pSrc--;

  V_strcpy_safe(pBuf, pSrc);
}

bool RepackBSP(const char *pszMapFile, bool bCompress) {
  Msg("Repacking %s.\n", pszMapFile);

  if (!g_pFullFileSystem->FileExists(pszMapFile)) {
    Warning("Couldn't open input file %s - BSP recompress failed.\n",
            pszMapFile);
    return false;
  }

  CUtlBuffer inputBuffer;
  if (!g_pFullFileSystem->ReadFile(pszMapFile, NULL, inputBuffer)) {
    Warning("Couldn't read file %s - BSP compression failed.\n", pszMapFile);
    return false;
  }

  CUtlBuffer outputBuffer;
  if (!RepackBSP(inputBuffer, outputBuffer,
                 bCompress ? RepackBSPCallback_LZMA : NULL,
                 bCompress ? IZip::eCompressionType_LZMA
                           : IZip::eCompressionType_None)) {
    Warning("Internal error compressing BSP '%s'.\n", pszMapFile);
    return false;
  }

  if (g_pFullFileSystem->WriteFile(pszMapFile, NULL, outputBuffer)) {
    Msg("Successfully repacked %s -- %zi -> %zi bytes\n", pszMapFile,
        inputBuffer.TellPut(), outputBuffer.TellPut());
    return true;
  }

  Warning("Internal error writing BSP '%s'.\n", pszMapFile);
  return false;
}

[[nodiscard]] int Usage() {
  fprintf(stderr, "usage: \n");
  fprintf(stderr, "bspzip -extract <bspfile> <blah.zip>\n");
  fprintf(stderr, "bspzip -extractfiles <bspfile> <targetpath>\n");
  fprintf(stderr, "bspzip -dir <bspfile>\n");
  fprintf(stderr,
          "bspzip -addfile <bspfile> <relativepathname> <fullpathname> "
          "<newbspfile>\n");
  fprintf(stderr, "bspzip -addlist <bspfile> <listfile> <newbspfile>\n");
  fprintf(stderr,
          "bspzip -addorupdatelist <bspfile> <listfile> <newbspfile>\n");
  fprintf(stderr, "bspzip -extractcubemaps <bspfile> <targetPath>\n");
  fprintf(stderr, "  Extracts the cubemaps to <targetPath>.\n");
  fprintf(stderr, "bspzip -deletecubemaps <bspfile>\n");
  fprintf(stderr, "  Deletes the cubemaps from <bspFile>.\n");
  fprintf(stderr,
          "bspzip -addfiles <bspfile> <relativePathPrefix> <listfile> "
          "<newbspfile>\n");
  fprintf(stderr, "  Adds files to <newbspfile>.\n");
  fprintf(stderr, "bspzip -repack [ -compress ] <bspfile>\n");
  fprintf(stderr,
          "  Optimally repacks a BSP file, optionally using compressed BSP "
          "format.\n");
  fprintf(stderr,
          "  Using on a compressed BSP without -compress will effectively "
          "decompress\n");
  fprintf(stderr, "  a compressed BSP.\n");

  return 1;
}

}  // namespace

int main(int argc, char **argv) {
  // Install an exception handler.
  const se::utils::common::ScopedDefaultMinidumpHandler
      scoped_default_minidumps;

  const ScopedHDRMode scoped_hdr_mode{false};

#ifdef PLATFORM_64BITS
  Msg("\nValve Software - bspzip [64 bit] (%s)\n", __DATE__);
#else
  Msg("\nValve Software - bspzip (%s)\n", __DATE__);
#endif

  int curArg = 1;

  // Options parsing assumes
  // [ -game foo ] -<action> <action specific args>

  // Skip -game foo
  if (argc >= curArg + 2 && stricmp(argv[curArg], "-game") == 0) {
    // Handled by filesystem code
    curArg += 2;
  }

  // Should have at least action
  // End of args
  if (curArg >= argc) return Usage();

  // Pointers to the action, the file, and any action args so I can remove all
  // the messy argc pointer math this was using.
  char *pAction = argv[curArg];
  curArg++;
  char **pActionArgs = &argv[curArg];
  int nActionArgs = argc - curArg;

  CommandLine()->CreateCmdLine(argc, argv);
  MathLib_Init(2.2f, 2.2f, 0.0f, 2);

  if ((stricmp(pAction, "-extract") == 0) && nActionArgs == 2) {
    // bspzip -extract <bspfile> <blah.zip>
    const ScopedFileSystem scopedFileSystem(pActionArgs[0]);

    char bspName[MAX_PATH] = {0};
    V_MakeAbsolutePath(bspName, pActionArgs[0]);
    Q_DefaultExtension(bspName, ".bsp");

    char zipName[MAX_PATH] = {0};
    V_strcpy_safe(zipName, pActionArgs[1]);
    Q_DefaultExtension(zipName, ".zip");

    ExtractZipFileFromBSP(bspName, zipName);
  } else if ((stricmp(pAction, "-extractfiles") == 0) && nActionArgs == 2) {
    // bsipzip -extractfiles <bspfile> <targetpath>
    const ScopedFileSystem scopedFileSystem(pActionArgs[0]);

    // necessary for xbox process
    // only the .vtf are extracted as necessary for streaming and not the .vmt
    // the .vmt are non-streamed and therefore remain, referenced normally as
    // part of the bsp search path
    char bspName[MAX_PATH] = {0};
    V_MakeAbsolutePath(bspName, pActionArgs[0]);
    V_DefaultExtension(bspName, ".bsp");

    char targetPathName[MAX_PATH] = {0};
    V_strcpy_safe(targetPathName, pActionArgs[1]);
    V_AppendSlash(targetPathName);

    printf("\nOpening bsp file: %s.\n", bspName);
    LoadBSPFile(bspName);

    CUtlBuffer buf;
    char relativeName[MAX_PATH] = {0};
    char targetName[MAX_PATH] = {0};
    int fileSize = 0;
    int id = -1;
    intp numFilesExtracted = 0;
    while (true) {
      id = GetNextFilename(GetPakFile(), id, relativeName, sizeof(relativeName),
                           fileSize);
      if (id == -1) break;

      {
        buf.EnsureCapacity(fileSize);
        buf.SeekPut(CUtlBuffer::SEEK_HEAD, 0);

        if (!ReadFileFromPak(GetPakFile(), relativeName, false, buf)) {
          fprintf(stderr, "Unable to read '%s' from '%s'.\n", relativeName,
                  bspName);
          return 1;
        }

        V_strcpy_safe(targetName, targetPathName);
        V_strcat_safe(targetName, relativeName);
        V_FixSlashes(targetName, '\\');

        SafeCreatePath(targetName);

        printf("Writing file: %s.\n", targetName);
        FILE *fp = fopen(targetName, "wb");
        if (!fp) {
          fprintf(stderr, "Could not write '%s'.\n", targetName);
          return 2;
        }

        fwrite(buf.Base(), fileSize, 1, fp);
        fclose(fp);

        numFilesExtracted++;
      }
    }

    printf("%zi files extracted.\n", numFilesExtracted);
  } else if ((stricmp(pAction, "-extractcubemaps") == 0) && nActionArgs == 2) {
    // bspzip -extractcubemaps <bspfile> <targetPath>
    const ScopedFileSystem scopedFileSystem(pActionArgs[0]);

    // necessary for xbox process
    // only the .vtf are extracted as necessary for streaming and not the .vmt
    // the .vmt are non-streamed and therefore remain, referenced normally as
    // part of the bsp search path
    char bspName[MAX_PATH] = {0};
    V_MakeAbsolutePath(bspName, pActionArgs[0]);
    V_DefaultExtension(bspName, ".bsp");

    char targetPathName[MAX_PATH] = {0};
    V_strcpy_safe(targetPathName, pActionArgs[1]);
    V_AppendSlash(targetPathName);

    printf("\nOpening bsp file: %s.\n", bspName);
    LoadBSPFile(bspName);

    CUtlBuffer buf;
    char relativeName[MAX_PATH] = {0};
    char targetName[MAX_PATH] = {0};
    int fileSize = 0;
    int id = -1;
    intp numFilesExtracted = 0;
    while (1) {
      id = GetNextFilename(GetPakFile(), id, relativeName, sizeof(relativeName),
                           fileSize);
      if (id == -1) break;

      if (Q_stristr(relativeName, ".vtf")) {
        buf.EnsureCapacity(fileSize);
        buf.SeekPut(CUtlBuffer::SEEK_HEAD, 0);

        if (!ReadFileFromPak(GetPakFile(), relativeName, false, buf)) {
          fprintf(stderr, "Unable to read '%s' from '%s'.\n", relativeName,
                  bspName);
          return 3;
        }

        V_strcpy_safe(targetName, targetPathName);
        V_strcat_safe(targetName, relativeName);
        V_FixSlashes(targetName, '\\');

        SafeCreatePath(targetName);

        printf("Writing vtf file: %s.\n", targetName);
        FILE *fp = fopen(targetName, "wb");
        if (!fp) {
          fprintf(stderr, "Could not write '%s'.\n", targetName);
          return 4;
        }

        fwrite(buf.Base(), fileSize, 1, fp);
        fclose(fp);

        numFilesExtracted++;
      }
    }

    printf("%zi cubemaps extracted.\n", numFilesExtracted);
  } else if ((stricmp(pAction, "-deletecubemaps") == 0) && nActionArgs == 1) {
    // bspzip -deletecubemaps <bspfile>
    const ScopedFileSystem scopedFileSystem(pActionArgs[0]);

    // necessary for xbox process
    // the cubemaps are deleted as they cannot yet be streamed out of the bsp
    char bspName[MAX_PATH] = {0};
    V_MakeAbsolutePath(bspName, pActionArgs[0]);
    V_DefaultExtension(bspName, ".bsp");

    printf("\nOpening bsp file: %s.\n", bspName);
    LoadBSPFile(bspName);

    CUtlBuffer buf;
    char relativeName[MAX_PATH] = {0};
    int fileSize = 0;
    int id = -1;
    intp numFilesDeleted = 0;
    while (1) {
      id = GetNextFilename(GetPakFile(), id, relativeName, sizeof(relativeName),
                           fileSize);
      if (id == -1) break;

      if (Q_stristr(relativeName, ".vtf")) {
        RemoveFileFromPak(GetPakFile(), relativeName);

        numFilesDeleted++;

        // start over
        id = -1;
      }
    }

    printf("%zi cubemaps deleted.\n", numFilesDeleted);
    if (numFilesDeleted) {
      // save out bsp file
      printf("Updating bsp file: %s.\n", bspName);
      WriteBSPFile(bspName);
    }
  } else if ((stricmp(pAction, "-addfiles") == 0) && nActionArgs == 4) {
    // bspzip -addfiles <bspfile> <relativePathPrefix> <listfile> <newbspfile>
    const ScopedFileSystem scopedFileSystem(pActionArgs[0]);

    char bspName[MAX_PATH] = {0};
    V_MakeAbsolutePath(bspName, pActionArgs[0]);
    Q_DefaultExtension(bspName, ".bsp");

    char relativePrefixName[MAX_PATH] = {0};
    V_strcpy_safe(relativePrefixName, pActionArgs[1]);

    char filelistName[MAX_PATH] = {0};
    V_strcpy_safe(filelistName, pActionArgs[2]);

    char newbspName[MAX_PATH] = {0};
    V_MakeAbsolutePath(newbspName, pActionArgs[3]);
    Q_DefaultExtension(newbspName, ".bsp");

    char fullpathName[MAX_PATH] = {0};
    FILE *fp = fopen(filelistName, "r");
    if (fp) {
      printf("Opening bsp file: %s.\n", bspName);
      LoadBSPFile(bspName);

      while (!feof(fp)) {
        if ((fgets(fullpathName, sizeof(fullpathName), fp) != NULL)) {
          intp fullpathNameLen = V_strlen(fullpathName);
          if (*fullpathName && fullpathName[fullpathNameLen - 1] == '\n') {
            fullpathName[fullpathNameLen - 1] = '\0';
          }

          // strip the path and add relative prefix
          char fileName[MAX_PATH] = {0};
          char relativeName[MAX_PATH] = {0};
          StripPath(fullpathName, fileName);
          V_strcpy_safe(relativeName, relativePrefixName);
          V_strcat_safe(relativeName, fileName);

          printf("Adding file: %s as %s.\n", fullpathName, relativeName);

          AddFileToPak(GetPakFile(), relativeName, fullpathName);
        } else if (!feof(fp)) {
          fprintf(stderr, "Missing full path names.\n");
          fclose(fp);
          return 5;
        }
      }

      printf("Writing new bsp file: %s.\n", newbspName);
      WriteBSPFile(newbspName);
      fclose(fp);
    }
  } else if ((stricmp(pAction, "-dir") == 0) && nActionArgs == 1) {
    // bspzip -dir <bspfile>
    const ScopedFileSystem scopedFileSystem(pActionArgs[0]);

    char bspName[MAX_PATH] = {0};
    V_MakeAbsolutePath(bspName, pActionArgs[0]);
    Q_DefaultExtension(bspName, ".bsp");

    LoadBSPFile(bspName);
    PrintBSPPackDirectory();
  } else if ((stricmp(pAction, "-addfile") == 0) && nActionArgs == 4) {
    // bspzip -addfile <bspfile> <relativepathname> <fullpathname> <newbspfile>
    const ScopedFileSystem scopedFileSystem(pActionArgs[0]);

    char bspName[MAX_PATH] = {0};
    V_MakeAbsolutePath(bspName, pActionArgs[0]);
    Q_DefaultExtension(bspName, ".bsp");

    char relativeName[MAX_PATH] = {0};
    V_strcpy_safe(relativeName, pActionArgs[1]);

    char fullpathName[MAX_PATH] = {0};
    V_strcpy_safe(fullpathName, pActionArgs[2]);

    char newbspName[MAX_PATH] = {0};
    V_MakeAbsolutePath(newbspName, pActionArgs[3]);
    Q_DefaultExtension(newbspName, ".bsp");

    // read it in, add pack file, write it back out
    LoadBSPFile(bspName);
    AddFileToPak(GetPakFile(), relativeName, fullpathName);
    WriteBSPFile(newbspName);
  } else if ((stricmp(pAction, "-addlist") == 0) && nActionArgs == 3) {
    // bspzip -addlist <bspfile> <listfile> <newbspfile>
    const ScopedFileSystem scopedFileSystem(pActionArgs[0]);

    char bspName[MAX_PATH] = {0};
    V_MakeAbsolutePath(bspName, pActionArgs[0]);
    Q_DefaultExtension(bspName, ".bsp");

    char filelistName[MAX_PATH] = {0};
    V_strcpy_safe(filelistName, pActionArgs[1]);

    char newbspName[MAX_PATH] = {0};
    V_MakeAbsolutePath(newbspName, pActionArgs[2]);
    Q_DefaultExtension(newbspName, ".bsp");

    // read it in, add pack file, write it back out

    char relativeName[MAX_PATH] = {0};
    char fullpathName[MAX_PATH] = {0};
    FILE *fp = fopen(filelistName, "r");
    if (fp) {
      printf("Opening bsp file: %s.\n", bspName);
      LoadBSPFile(bspName);

      while (!feof(fp)) {
        relativeName[0] = 0;
        fullpathName[0] = 0;
        if ((fgets(relativeName, sizeof(relativeName), fp) != NULL) &&
            (fgets(fullpathName, sizeof(fullpathName), fp) != NULL)) {
          intp l1 = V_strlen(relativeName);
          intp l2 = V_strlen(fullpathName);
          if (l1 > 0) {
            if (relativeName[l1 - 1] == '\n') {
              relativeName[l1 - 1] = 0;
            }
          }
          if (l2 > 0) {
            if (fullpathName[l2 - 1] == '\n') {
              fullpathName[l2 - 1] = 0;
            }
          }
          printf("Adding file: %s.\n", fullpathName);
          AddFileToPak(GetPakFile(), relativeName, fullpathName);
        } else if (!feof(fp) || (relativeName[0] && !fullpathName[0])) {
          fprintf(stderr,
                  "Missing paired relative '%s'/full path '%s' names.\n",
                  relativeName, fullpathName);
          fclose(fp);
          return 6;
        }
      }

      printf("Writing new bsp file: %s.\n", newbspName);
      WriteBSPFile(newbspName);
      fclose(fp);
    }
  } else if ((stricmp(pAction, "-addorupdatelist") == 0) && nActionArgs == 3) {
    // bspzip -addorupdatelist <bspfile> <listfile> <newbspfile>
    const ScopedFileSystem scopedFileSystem(pActionArgs[0]);

    char bspName[MAX_PATH] = {0};
    V_MakeAbsolutePath(bspName, pActionArgs[0]);
    Q_DefaultExtension(bspName, ".bsp");

    char filelistName[MAX_PATH] = {0};
    V_strcpy_safe(filelistName, pActionArgs[1]);

    char newbspName[MAX_PATH] = {0};
    V_MakeAbsolutePath(newbspName, pActionArgs[2]);
    Q_DefaultExtension(newbspName, ".bsp");

    // read it in, add pack file, write it back out

    char relativeName[MAX_PATH] = {0};
    char fullpathName[MAX_PATH] = {0};
    FILE *fp = fopen(filelistName, "r");
    if (fp) {
      printf("Opening bsp file: %s.\n", bspName);
      LoadBSPFile(bspName);

      while (!feof(fp)) {
        relativeName[0] = 0;
        fullpathName[0] = 0;
        if ((fgets(relativeName, sizeof(relativeName), fp) != NULL) &&
            (fgets(fullpathName, sizeof(fullpathName), fp) != NULL)) {
          intp l1 = V_strlen(relativeName);
          intp l2 = V_strlen(fullpathName);
          if (l1 > 0) {
            if (relativeName[l1 - 1] == '\n') {
              relativeName[l1 - 1] = 0;
            }
          }
          if (l2 > 0) {
            if (fullpathName[l2 - 1] == '\n') {
              fullpathName[l2 - 1] = 0;
            }
          }

          bool bUpdating = FileExistsInPak(GetPakFile(), relativeName);
          printf("%s file: %s.\n", bUpdating ? "Updating" : "Adding",
                 fullpathName);
          if (bUpdating) {
            RemoveFileFromPak(GetPakFile(), relativeName);
          }
          AddFileToPak(GetPakFile(), relativeName, fullpathName);
        } else if (!feof(fp) || (relativeName[0] && !fullpathName[0])) {
          fprintf(stderr,
                  "Missing paired relative '%s'/full path '%s' names.\n",
                  relativeName, fullpathName);
          fclose(fp);
          return 7;
        }
      }

      printf("Writing new bsp file: %s.\n", newbspName);
      WriteBSPFile(newbspName);
      fclose(fp);
    }
  } else if ((stricmp(pAction, "-repack") == 0) &&
             (nActionArgs == 1 || nActionArgs == 2)) {
    // bspzip -repack [ -compress ] <bspfile>
    bool bCompress = false;
    const char *pFile = pActionArgs[0];

    if (nActionArgs == 2 && stricmp(pActionArgs[0], "-compress") == 0) {
      pFile = pActionArgs[1];
      bCompress = true;
    } else if (nActionArgs == 2) {
      return Usage();
    }

    const ScopedFileSystem scopedFileSystem(pFile);

    char szAbsBSPPath[MAX_PATH] = {0};
    V_MakeAbsolutePath(szAbsBSPPath, pFile);
    Q_DefaultExtension(szAbsBSPPath, ".bsp");

    return RepackBSP(szAbsBSPPath, bCompress) ? 0 : -1;
  } else {
    return Usage();
  }

  return 0;
}
