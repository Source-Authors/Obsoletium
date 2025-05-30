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

[[nodiscard]] bool ImageRGBA8888HasAlpha(unsigned char *pImage, int numTexels) {
  for (int i = 0; i < numTexels; i++) {
    if (pImage[i * 4 + 3] != 255) return true;
  }

  return false;
}

[[nodiscard]] bool GetKeyValueFromBuffer(CUtlBuffer &buf, char **key,
                                         char **val) {
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

[[nodiscard]] bool LoadConfigFile(const char *pFileName, float *bumpScale,
                                  int *startFrame, int *endFrame) {
  CUtlBuffer buf((intp)0, 0, CUtlBuffer::TEXT_BUFFER);
  if (!g_pFullFileSystem->ReadFile(pFileName, nullptr, buf)) return false;

  char *key = nullptr, *val = nullptr;

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

  return true;
}

[[noreturn]] void Usage() {
  fprintf(stderr,
          "Usage: height2normal [-nopause] [-quiet] tex1_normal.txt "
          "tex2_normal.txt . . .\n");
  fprintf(stderr,
          "-quiet   : don't print anything out, don't pause for input\n"
          "-nopause : don't pause for input\n");
  Pause();
  exit(EINVAL);
}

void ProcessFiles(const char *normalFileNameWithoutExtension, int startFrame,
                  int endFrame, float bumpScale) {
  static char heightTGAFileName[1024], normalTGAFileName[1024], buffer[1024];

  bool animated = !(startFrame == -1 || endFrame == -1);
  int numFrames = endFrame - startFrame + 1;

  for (int frameID = 0; frameID < numFrames; frameID++) {
    if (animated) {
      V_sprintf_safe(normalTGAFileName, "%s%03d.tga",
                     normalFileNameWithoutExtension, frameID + startFrame);
    } else {
      V_sprintf_safe(normalTGAFileName, "%s.tga",
                     normalFileNameWithoutExtension);
    }

    V_strcpy_safe(buffer, normalFileNameWithoutExtension);

    // Strip '_normal' off the end because we're looking for '_height'
    char *underscore = Q_stristr(buffer, "_normal");
    if (!underscore) {
      fprintf(stderr, "config file '%s' name must end in _normal.txt\n",
              buffer);
      return;
    }

    *underscore = '\0';

    if (animated) {
      V_sprintf_safe(heightTGAFileName, "%s_height%03d.tga", buffer,
                     frameID + startFrame);
    } else {
      V_sprintf_safe(heightTGAFileName, "%s_height.tga", buffer);
    }

    CUtlBuffer buf;
    if (!g_pFullFileSystem->ReadFile(heightTGAFileName, nullptr, buf)) {
      fprintf(stderr, "'%s' not found.\n", heightTGAFileName);
      return;
    }

    ImageFormat imageFormat;
    int width, height;
    float sourceGamma;
    if (!TGALoader::GetInfo(buf, &width, &height, &imageFormat, &sourceGamma)) {
      fprintf(stderr, "error in %s.\n", heightTGAFileName);
      return;
    }

    intp size =
        ImageLoader::GetMemRequired(width, height, 1, IMAGE_FORMAT_IA88, false);
    std::unique_ptr<byte[]> ia88 = std::make_unique<byte[]>(size);

    buf.SeekGet(CUtlBuffer::SEEK_HEAD, 0);
    if (!TGALoader::Load(ia88.get(), buf, width, height, IMAGE_FORMAT_IA88,
                         sourceGamma, false)) {
      fprintf(stderr, "error in %s.\n", heightTGAFileName);
      return;
    }

    size = ImageLoader::GetMemRequired(width, height, 1, IMAGE_FORMAT_RGBA8888,
                                       false);
    std::unique_ptr<byte[]> rgba8888 = std::make_unique<byte[]>(size);

    ImageLoader::ConvertIA88ImageToNormalMapRGBA8888(ia88.get(), width, height,
                                                     rgba8888.get(), bumpScale);

    CUtlBuffer normalBuf;
    ImageLoader::NormalizeNormalMapRGBA8888(rgba8888.get(), width * height);

    if (ImageRGBA8888HasAlpha(rgba8888.get(), width * height)) {
      if (!TGAWriter::WriteToBuffer(rgba8888.get(), normalBuf, width, height,
                                    IMAGE_FORMAT_RGBA8888,
                                    IMAGE_FORMAT_RGBA8888)) {
        fprintf(stderr, "unable to convert to alpha RGBA8888 image %s.\n",
                heightTGAFileName);
        return;
      }
    } else {
      size = ImageLoader::GetMemRequired(width, height, 1, IMAGE_FORMAT_RGB888,
                                         false);
      std::unique_ptr<byte[]> rgb888 = std::make_unique<byte[]>(size);

      ImageLoader::ConvertImageFormat(rgba8888.get(), IMAGE_FORMAT_RGBA8888,
                                      rgb888.get(), IMAGE_FORMAT_RGB888, width,
                                      height, 0, 0);

      if (!TGAWriter::WriteToBuffer(rgb888.get(), normalBuf, width, height,
                                    IMAGE_FORMAT_RGB888, IMAGE_FORMAT_RGB888)) {
        fprintf(stderr, "unable to convert to alpha RGB888 image %s.\n",
                heightTGAFileName);
        return;
      }
    }

    if (!g_pFullFileSystem->WriteFile(normalTGAFileName, NULL, normalBuf)) {
      fprintf(stderr, "unable to write %s.\n", normalTGAFileName);
      return;
    }
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
      // no point in pausing if we aren't going to print anything out.
      g_NoPause = true;
    }

    if (stricmp(argv[i], "-nopause") == 0) {
      i++;
      g_NoPause = true;
    } else {
      break;
    }
  }

  char cwd[MAX_PATH];
  if (!_getcwd(cwd, ssize(cwd))) {
    fprintf(stderr, "unable to get the current directory: %s.\n",
            std::generic_category().message(errno).c_str());
    return -1;
  }

  V_FixSlashes(cwd);
  V_StripTrailingSlash(cwd);

  for (; i < argc; i++) {
    static char normalFileNameWithoutExtension[1024];

    char *fileName;
    if (!V_IsAbsolutePath(argv[i])) {
      V_sprintf_safe(normalFileNameWithoutExtension, "%s\\%s", cwd, argv[i]);

      fileName = normalFileNameWithoutExtension;
    } else {
      fileName = argv[i];
    }

    if (!g_Quiet) printf("file: %s\n", fileName);

    float bumpScale = -1.0f;
    int startFrame = -1;
    int endFrame = -1;
    if (!LoadConfigFile(fileName, &bumpScale, &startFrame, &endFrame)) {
      fprintf(stderr, "unable to load '%s'.\n", fileName);
      Pause();
      return EIO;
    }

    if (bumpScale == -1.0f) {
      fprintf(stderr, "must specify \"bumpscale\" in config file.\n");
      Pause();
      continue;
    }

    if ((startFrame == -1 && endFrame != -1) ||
        (startFrame != -1 && endFrame == -1)) {
      fprintf(
          stderr,
          "if you use startframe, you must use endframe, and vice versa.\n");
      Pause();
      continue;
    }

    if (!g_Quiet) printf("\tbumpscale: %f\n", bumpScale);

    V_StripExtension(fileName, normalFileNameWithoutExtension);
    ProcessFiles(normalFileNameWithoutExtension, startFrame, endFrame,
                 bumpScale);
  }

  return 0;
}
