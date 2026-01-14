// Copyright Valve Corporation, All rights reserved.
//
// Utility to generate an SSBump from a normal bump map (as opposed to a height
// or displacement map).
//
// See https://developer.valvesoftware.com/wiki/$ssbump

#include "tier0/platform.h"
#include "tier0/progressbar.h"
#include "tier1/strtools.h"
#include "tier2/tier2.h"

#include "bitmap/float_bm.h"
#include "mathlib/vector.h"
#include "mathlib/bumpvects.h"

#include "tools_minidump.h"

#include "tier0/memdbgon.h"

namespace {

[[nodiscard]] constexpr inline float RangeAdjust(float x) {
  return 2 * (x - .5f);
}

[[nodiscard]] constexpr inline float SaturateAndSquare(float x) {
  x = max(0.f, min(1.f, x));
  return x * x;
}

const Vector g_bump_basis[]{
    Vector(0.81649661064147949f, 0.0f, OO_SQRT_3),
    Vector(-0.40824833512306213f, 0.70710676908493042f, OO_SQRT_3),
    Vector(-0.40824821591377258f, -0.7071068286895752f, OO_SQRT_3)};

}  // namespace

int main(int argc, char **argv) {
  // Install an exception handler.
  const se::utils::common::ScopedDefaultMinidumpHandler
      scoped_default_minidumps;

  const ScopedCommandLineProgram scoped_command_line_program(argc, argv);

  if (argc != 2) {
    fprintf(stderr, "Usage: normal2ssbump filename.tga\n");
    return EINVAL;
  }

  ReportProgress("reading src texture", 0, 0);

  const FloatBitMap_t src_texture{argv[1]};
  if (!src_texture.IsValid()) {
    fprintf(stderr, "Unable to load '%s'.\n", argv[1]);
    return EIO;
  }

  for (int y = 0; y < src_texture.Height; y++) {
    ReportProgress("Converting to ssbump format", src_texture.Height, y);

    for (int x = 0; x < src_texture.Width; x++) {
      const Vector n(RangeAdjust(src_texture.Pixel(x, y, 0)),
                     RangeAdjust(src_texture.Pixel(x, y, 1)),
                     RangeAdjust(src_texture.Pixel(x, y, 2)));

      Vector dp(SaturateAndSquare(DotProduct(n, g_bump_basis[0])),
                SaturateAndSquare(DotProduct(n, g_bump_basis[1])),
                SaturateAndSquare(DotProduct(n, g_bump_basis[2])));

      const float sum{DotProduct(dp, Vector(1, 1, 1))};

      dp *= 1.0f / sum;

      src_texture.Pixel(x, y, 0) = dp.x;
      src_texture.Pixel(x, y, 1) = dp.y;
      src_texture.Pixel(x, y, 2) = dp.z;
    }
  }

  char oname[MAX_PATH];
  V_strcpy_safe(oname, argv[1]);

  char *dot = Q_stristr(oname, "_normal");
  if (!dot) dot = strchr(oname, '.');
  if (!dot) dot = oname + strlen(oname);

  V_strncpy(dot, "_ssbump.tga", ssize(oname) - (dot - oname));

  if (src_texture.WriteTGAFile(oname)) {
    printf("\nWriting TGA to '%s': success", oname);
    return 0;
  }

  fprintf(stderr, "Unable to write TGA to '%s'.\n", oname);
  return EIO;
}
