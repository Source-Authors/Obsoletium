// Copyright Valve Corporation, All rights reserved.
//
// Convert heightmap texture into selfshadow bump map texture.
//
// See https://developer.valvesoftware.com/wiki/$ssbump#Creation

#include "tier0/platform.h"
#include "tier0/progressbar.h"
#include "tier2/tier2.h"
#include "bitmap/float_bm.h"
#include "mathlib/mathlib.h"
#include "tools_minidump.h"

#include "tier0/memdbgon.h"

namespace {

void PrintArgSummaryAndExit() {
  fprintf(stderr, "format is 'height2ssbump filename.tga bumpscale'\n");
  fprintf(stderr,
          "options:\n-r NUM\tSet the number of rays (default = 250. more rays "
          "take more time).\n");
  fprintf(stderr, "-n\tGenerate a conventional normal map\n");
  fprintf(stderr, "-A\tGenerate ambient occlusion in the alpha channel\n");
  fprintf(stderr,
          "-f NUM\tSet smoothing filter radius (0=no filter, default = 10)\n");
  fprintf(stderr, "-D\tWrite out the filtered result as filterd.tga\n");
  exit(EINVAL);
}

}  // namespace

int main(int argc, char **argv) {
  // Install an exception handler.
  const se::utils::common::ScopedDefaultMinidumpHandler
      scoped_default_minidumps;

  const ScopedCommandLineProgram scoped_command_line_program(argc, argv);

#ifdef PLATFORM_64BITS
  Msg("\nValve Software - height2ssbump [64 bit] (%s)\n", __DATE__);
#else
  Msg("\nValve Software - height2ssbump (%s)\n", __DATE__);
#endif

  int nCurArg = 1;

  bool bNormalOnly = false;
  int filterRadius = 10;
  bool bWriteFiltered = false;

  uint32 nSSBumpFlags = 0;

  int nNumRays = 250;

  while ((nCurArg < argc) && (argv[nCurArg][0] == '-')) {
    switch (argv[nCurArg][1]) {
      case 'n':  // lighting from normal only
      {
        bNormalOnly = true;
      } break;

      case 'r':  // set # of rays
      {
        nNumRays = atoi(argv[++nCurArg]);
      } break;

      case 'f':  // filter radius
      {
        // dimhotepus: atof -> atoi as radius expected to be int.
        filterRadius = atoi(argv[++nCurArg]);
      } break;

      case 'd':  // detail texture
      {
        nSSBumpFlags |= SSBUMP_MOD2X_DETAIL_TEXTURE;
      } break;

      case 'A':  // ambeint occlusion
      {
        nSSBumpFlags |= SSBUMP_OPTION_NONDIRECTIONAL;
      } break;

      case 'D':  // debug - write filtered output
      {
        bWriteFiltered = true;
      } break;

      case '?':  // args
      {
        PrintArgSummaryAndExit();
      } break;

      default:
        fprintf(stderr, "unrecogized option %s.\n", argv[nCurArg]);
        PrintArgSummaryAndExit();
    }
    nCurArg++;
  }
  argc -= (nCurArg - 1);
  argv += (nCurArg - 1);

  if (argc != 3) PrintArgSummaryAndExit();

  ReportProgress("reading src texture", 0, 0);

  FloatBitMap_t src_texture(argv[1]);
  if (!src_texture.IsValid()) {
    fprintf(stderr, "Unable to load '%s'.\n", argv[1]);
    return EIO;
  }

  src_texture.TileableBilateralFilter(filterRadius, 2.0f / 255.0f);
  if (bWriteFiltered) {
    constexpr char filteredTgaName[]{"filtered.tga"};

    if (!src_texture.WriteTGAFile(filteredTgaName)) {
      fprintf(stderr, "Unable to write '%s' from '%s'.\n", filteredTgaName,
              argv[1]);
    }
  }

  FloatBitMap_t *out;

  if (bNormalOnly)
    // dimhotepus: atof -> strtof.
    out = src_texture.ComputeBumpmapFromHeightInAlphaChannel(
        strtof(argv[2], nullptr));
  else
    // dimhotepus: atof -> strtof.
    out = src_texture.ComputeSelfShadowedBumpmapFromHeightInAlphaChannel(
        strtof(argv[2], nullptr), nNumRays, nSSBumpFlags);

  if (!out) {
    fprintf(stderr, "Unable to compute %s from '%s'.\n",
            bNormalOnly ? "bumpmap" : "ss bumpmap", argv[1]);
    return ENOMEM;
  }

  char oname[1024];
  V_strcpy_safe(oname, argv[1]);

  char *dot = strchr(oname, '.');
  if (!dot) dot = oname + strlen(oname);

  V_strncpy(dot, bNormalOnly ? "-bump.tga" : "-ssbump.tga",
            oname + ssize(oname) - dot);

  int rc = 0;
  if (!out->WriteTGAFile(oname)) {
    fprintf(stderr, "Unable to write result '%s'.\n", oname);
    rc = EIO;
  }

  delete out;
  return rc;
}
