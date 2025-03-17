// Copyright Valve Corporation, All rights reserved.
//
// Convert a heightmap texture into a normal map texture.
//
// See https://developer.valvesoftware.com/wiki/Height2Normal

#include "mathlib/mathlib.h"
#include "bitmap/imageformat.h"
#include "bitmap/TGAWriter.h"
#include "bitmap/TGALoader.h"
#include "tier1/strtools.h"
#include "tier1/utlbuffer.h"
#include "tier2/tier2.h"
#include "filesystem.h"
#include "tools_minidump.h"

#include <direct.h>
#include <conio.h>

#include "tier0/memdbgon.h"

namespace {

bool g_NoPause = false;
bool g_Quiet = false;

void Pause() {
  if (!g_NoPause) {
    printf("Hit a key to continue\n");
    getch();
  }
}

bool ImageRGBA8888HasAlpha(unsigned char *pImage, int numTexels) {
  int i;
  for (i = 0; i < numTexels; i++) {
    if (pImage[i * 4 + 3] != 255) {
      return true;
    }
  }
  return false;
}

bool GetKeyValueFromBuffer(CUtlBuffer &buf, char **key, char **val) {
  char stringBuf[2048];
  while (buf.IsValid()) {
    buf.GetLine(stringBuf);
    char *scan = stringBuf;
    // search for the first quote for the key.
    while (1) {
      if (*scan == '\"') {
        *key = ++scan;
        break;
      }
      if (*scan == '#') {
        goto next_line;  // comment
      }
      if (*scan == '\0') {
        goto next_line;  // end of line.
      }
      scan++;
    }
    // read the key until another quote.
    while (1) {
      if (*scan == '\"') {
        *scan = '\0';
        scan++;
        break;
      }
      if (*scan == '\0') {
        goto next_line;
      }
      scan++;
    }
    // search for the first quote for the value.
    while (1) {
      if (*scan == '\"') {
        *val = ++scan;
        break;
      }
      if (*scan == '#') {
        goto next_line;  // comment
      }
      if (*scan == '\0') {
        goto next_line;  // end of line.
      }
      scan++;
    }
    // read the value until another quote.
    while (1) {
      if (*scan == '\"') {
        *scan = '\0';
        scan++;
        // got a key and a value, so get the hell out of here.
        return true;
      }
      if (*scan == '\0') {
        goto next_line;
      }
      scan++;
    }
  next_line:;
  }
  return false;
}

void LoadConfigFile(const char *pFileName, float *bumpScale, int *startFrame,
                    int *endFrame) {
  CUtlBuffer buf((intp)0, 0, CUtlBuffer::TEXT_BUFFER);
  if (!g_pFullFileSystem->ReadFile(pFileName, NULL, buf)) {
    fprintf(stderr, "Can't open %s.\n", pFileName);
    Pause();
    exit(EIO);
  }

  char *key = NULL;
  char *val = NULL;
  while (GetKeyValueFromBuffer(buf, &key, &val)) {
    if (stricmp(key, "bumpscale") == 0) {
      // dimhotepus: atof -> strtof.
      *bumpScale = strtof(val, nullptr);
    }
    if (stricmp(key, "startframe") == 0) {
      *startFrame = atoi(val);
    } else if (stricmp(key, "endframe") == 0) {
      *endFrame = atoi(val);
    }
  }
}

[[noreturn]] void Usage() {
  fprintf(stderr,
          "Usage: height2normal [-nopause] [-quiet] tex1_normal.txt "
          "tex2_normal.txt . . .\n");
  fprintf(stderr,
          "-quiet   : don't print anything out, don't pause for input\n");
  fprintf(stderr, "-nopause : don't pause for input\n");
  Pause();
  exit(EINVAL);
}

void ProcessFiles(const char *pNormalFileNameWithoutExtension, int startFrame,
                  int endFrame, float bumpScale) {
  static char heightTGAFileName[1024];
  static char normalTGAFileName[1024];
  static char buffer[1024];
  bool animated = !(startFrame == -1 || endFrame == -1);
  int numFrames = endFrame - startFrame + 1;
  int frameID;

  for (frameID = 0; frameID < numFrames; frameID++) {
    if (animated) {
      V_sprintf_safe(normalTGAFileName, "%s%03d.tga",
                     pNormalFileNameWithoutExtension, frameID + startFrame);
    } else {
      V_sprintf_safe(normalTGAFileName, "%s.tga",
                     pNormalFileNameWithoutExtension);
    }
    if (!Q_stristr(pNormalFileNameWithoutExtension, "_normal")) {
      fprintf(stderr, "ERROR: config file name must end in _normal.txt\n");
      return;
    }

    V_strcpy_safe(buffer, pNormalFileNameWithoutExtension);

    // Strip '_normal' off the end because we're looking for '_height'
    char *pcUnderscore = Q_stristr(buffer, "_normal");
    *pcUnderscore = NULL;

    if (animated) {
      V_sprintf_safe(heightTGAFileName, "%s_height%03d.tga", buffer,
                     frameID + startFrame);
    } else {
      V_sprintf_safe(heightTGAFileName, "%s_height.tga", buffer);
    }

    enum ImageFormat imageFormat;
    int width, height;
    float sourceGamma;
    CUtlBuffer buf;
    if (!g_pFullFileSystem->ReadFile(heightTGAFileName, NULL, buf)) {
      fprintf(stderr, "%s not found.\n", heightTGAFileName);
      return;
    }

    if (!TGALoader::GetInfo(buf, &width, &height, &imageFormat, &sourceGamma)) {
      fprintf(stderr, "error in %s.\n", heightTGAFileName);
      return;
    }

    intp memRequired =
        ImageLoader::GetMemRequired(width, height, 1, IMAGE_FORMAT_IA88, false);
    unsigned char *pImageIA88 = new unsigned char[memRequired];

    buf.SeekGet(CUtlBuffer::SEEK_HEAD, 0);
    if (!TGALoader::Load(pImageIA88, buf, width, height, IMAGE_FORMAT_IA88,
                         sourceGamma, false)) {
      fprintf(stderr, "error in %s.\n", heightTGAFileName);
      return;
    }

    memRequired = ImageLoader::GetMemRequired(width, height, 1,
                                              IMAGE_FORMAT_RGBA8888, false);
    unsigned char *pImageRGBA8888 = new unsigned char[memRequired];
    ImageLoader::ConvertIA88ImageToNormalMapRGBA8888(pImageIA88, width, height,
                                                     pImageRGBA8888, bumpScale);

    CUtlBuffer normalBuf;
    ImageLoader::NormalizeNormalMapRGBA8888(pImageRGBA8888, width * height);
    if (ImageRGBA8888HasAlpha(pImageRGBA8888, width * height)) {
      if (!TGAWriter::WriteToBuffer(pImageRGBA8888, normalBuf, width, height,
                                    IMAGE_FORMAT_RGBA8888,
                                    IMAGE_FORMAT_RGBA8888))
        Error("Unable to convert alpha RGBA8888 image to RGBA8888.\n");
    } else {
      memRequired = ImageLoader::GetMemRequired(width, height, 1,
                                                IMAGE_FORMAT_RGB888, false);
      unsigned char *pImageRGB888 = new unsigned char[memRequired];
      ImageLoader::ConvertImageFormat(pImageRGBA8888, IMAGE_FORMAT_RGBA8888,
                                      pImageRGB888, IMAGE_FORMAT_RGB888, width,
                                      height, 0, 0);
      if (!TGAWriter::WriteToBuffer(pImageRGB888, normalBuf, width, height,
                                    IMAGE_FORMAT_RGB888, IMAGE_FORMAT_RGB888))
        Error("Unable to convert alpha RGB888 image to RGB888.\n");

      delete[] pImageRGB888;
    }
    if (!g_pFullFileSystem->WriteFile(normalTGAFileName, NULL, normalBuf)) {
      fprintf(stderr, "unable to write %s.\n", normalTGAFileName);
      return;
    }

    delete[] pImageIA88;
    delete[] pImageRGBA8888;
  }
}

}  // namespace

int main(int argc, char **argv) {
  // Install an exception handler.
  const se::utils::common::ScopedDefaultMinidumpHandler
      scoped_default_minidumps;

#ifdef PLATFORM_64BITS
  Msg("\nValve Software - height2normal [64 bit] (%s)\n", __DATE__);
#else
  Msg("\nValve Software - height2normal (%s)\n", __DATE__);
#endif

  if (argc < 2) Usage();

  MathLib_Init(2.2f, 2.2f, 0.0f, 2);

  const ScopedDefaultFileSystem scoped_default_file_system;

  int i = 1;
  while (i < argc) {
    if (stricmp(argv[i], "-quiet") == 0) {
      i++;
      g_Quiet = true;
      g_NoPause = true;  // no point in pausing if we aren't going to print
                         // anything out.
    }
    if (stricmp(argv[i], "-nopause") == 0) {
      i++;
      g_NoPause = true;
    } else {
      break;
    }
  }

  char pCurrentDirectory[MAX_PATH];
  if (_getcwd(pCurrentDirectory, sizeof(pCurrentDirectory)) == NULL) {
    fprintf(stderr, "Unable to get the current directory.\n");
    return -1;
  }

  Q_FixSlashes(pCurrentDirectory);
  Q_StripTrailingSlash(pCurrentDirectory);

  for (; i < argc; i++) {
    static char normalFileNameWithoutExtension[1024];
    char *pFileName;
    if (!Q_IsAbsolutePath(argv[i])) {
      V_sprintf_safe(normalFileNameWithoutExtension, "%s\\%s",
                     pCurrentDirectory, argv[i]);
      pFileName = normalFileNameWithoutExtension;
    } else {
      pFileName = argv[i];
    }

    if (!g_Quiet) {
      printf("file: %s\n", pFileName);
    }

    float bumpScale = -1.0f;
    int startFrame = -1;
    int endFrame = -1;
    LoadConfigFile(pFileName, &bumpScale, &startFrame, &endFrame);
    if (bumpScale == -1.0f) {
      fprintf(stderr, "Must specify \"bumpscale\" in config file.\n");
      Pause();
      continue;
    }
    if ((startFrame == -1 && endFrame != -1) ||
        (startFrame != -1 && endFrame == -1)) {
      fprintf(stderr,
              "ERROR: If you use startframe, you must use endframe, and vice "
              "versa.\n");
      Pause();
      continue;
    }
    if (!g_Quiet) {
      printf("\tbumpscale: %f\n", bumpScale);
    }

    Q_StripExtension(pFileName, normalFileNameWithoutExtension);
    ProcessFiles(normalFileNameWithoutExtension, startFrame, endFrame,
                 bumpScale);
  }

  return 0;
}
