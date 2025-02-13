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
  float flFilterRadius = 10.0f;
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
        // dimhotepus: atof -> strtof.
        flFilterRadius = strtof(argv[++nCurArg], nullptr);
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

  if (argc != 3) {
    PrintArgSummaryAndExit();
  }
  ReportProgress("reading src texture", 0, 0);
  FloatBitMap_t src_texture(argv[1]);
  src_texture.TileableBilateralFilter(flFilterRadius, 2.0f / 255.0f);
  if (bWriteFiltered) src_texture.WriteTGAFile("filtered.tga");

  FloatBitMap_t *out;

  if (bNormalOnly)
    // dimhotepus: atof -> strtof.
    out = src_texture.ComputeBumpmapFromHeightInAlphaChannel(
        strtof(argv[2], nullptr));
  else
    // dimhotepus: atof -> strtof.
    out = src_texture.ComputeSelfShadowedBumpmapFromHeightInAlphaChannel(
        strtof(argv[2], nullptr), nNumRays, nSSBumpFlags);

  char oname[1024];
  V_strcpy_safe(oname, argv[1]);

  char *dot = strchr(oname, '.');
  if (!dot) dot = oname + strlen(oname);

  if (bNormalOnly)
    V_strncpy(dot, "-bump.tga", oname + ssize(oname) - dot);
  else
    V_strncpy(dot, "-ssbump.tga", oname + ssize(oname) - dot);

  out->WriteTGAFile(oname);
  delete out;

  return 0;
}
