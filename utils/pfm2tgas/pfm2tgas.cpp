// Copyright Valve Corporation, All rights reserved.
//
// PFM to TGAs converter.

#include "bitmap/float_bm.h"
#include "mathlib/mathlib.h"

#include "tier0/platform.h"
#include "tier2/tier2.h"

#include "tools_minidump.h"

#include "tier0/memdbgon.h"

namespace {

[[nodiscard]] constexpr inline float Normalize2Byte(float f) {
  const uint8 px = static_cast<uint8>(std::min(255.0f, f * 255.0f));
  return px * (1.0f / 255.0f);
}

}  // namespace

int main(int argc, char **argv) {
  // Install an exception handler.
  const se::utils::common::ScopedDefaultMinidumpHandler
      scoped_default_minidumps;

  constexpr size_t kOutTexturesCount{3};
  constexpr float kTexture2TextureScale{8.0f};
  constexpr float kBaseScale{0.25f};

  constexpr float kErrorWeights[]{0.3f, 0.59f, 0.11f};

  const ScopedCommandLineProgram scoped_command_line_program(argc, argv);

  if (argc != 3) {
    fprintf(stderr, "Usage: pfm2tgas xxx.pfm scalefactor\n");
    return EINVAL;
  }

  char *end{nullptr};
  const float scale_factor{strtof(argv[2], &end)};
  if (*end != '\0') {
    fprintf(stderr, "Usage: pfm2tgas xxx.pfm <float scalefactor>\n");
    return EINVAL;
  }

  const char *pfm_name{argv[1]};

  FloatBitMap_t src_texture;
  if (!src_texture.LoadFromPFM(pfm_name)) {
    fprintf(stderr, "Unable to load PFM '%s'.\n", pfm_name);
    return EIO;
  }

  // we will split the float texture into 2 textures.
  FloatBitMap_t out_textures[kOutTexturesCount];
  for (auto &out : out_textures) {
    if (!out.AllocateRGB(src_texture.Width, src_texture.Height)) {
      fprintf(stderr, "Unable to allocate %zu memory bytes for '%s'.\n",
              // 4 for RGBA bytes.
              static_cast<uintp>(4) * src_texture.Width * src_texture.Height,
              pfm_name);
      return ENOMEM;
    }
  }

  for (int y = 0; y < src_texture.Height; y++) {
    for (int x = 0; x < src_texture.Width; x++) {
      for (int c = 0; c < 3; c++) {
        src_texture.Pixel(x, y, c) *= scale_factor;
      }

      float current_scale{kBaseScale};
      float px_error[kOutTexturesCount];
      float sum_error{0};

      static_assert(ssize(px_error) == ssize(out_textures));

      for (intp o = 0; o < ssize(px_error); o++) {
        px_error[o] = 0.f;

        for (int c = 0; c < 3; c++) {
          const float pixel{
              Normalize2Byte(src_texture.Pixel(x, y, c) / current_scale)};

          out_textures[o].Pixel(x, y, c) = pixel;

          const float scaled_pixel{current_scale * pixel};
          float error{fabs(src_texture.Pixel(x, y, c) - scaled_pixel)};

          if (src_texture.Pixel(x, y, c) > current_scale) error *= 2;

          px_error[o] += kErrorWeights[o] * error;
        }

        sum_error += px_error[o];
        current_scale *= kTexture2TextureScale;
      }

      // now, set the weight for each map based upon closeness of fit between
      // the scales
      for (intp o = 0; o < ssize(out_textures); o++) {
        // possible, I guess, for black
        if (sum_error == 0) {
          src_texture.Pixel(x, y, 3) = o == 0 ? 1.0f : 0.0f;
        } else {
          out_textures[o].Pixel(x, y, 3) = 1.0f - (px_error[o] / sum_error);
        }
      }
    }
  }

  char name[MAX_PATH];
  // now, output results
  for (int o = 0; o < kOutTexturesCount; o++) {
    V_strcpy_safe(name, pfm_name);

    char *dot = strchr(name, '.');
    if (!dot) dot = name + strlen(name);

    V_snprintf(dot, ssize(name) - (dot - name), "_%d.tga", o);

    if (!out_textures[o].WriteTGAFile(name)) {
      fprintf(stderr,
              "Unable to write #%d TGA texture '%s' for PFM '%s' one.\n", o,
              name, pfm_name);
      return EIO;
    }
  }

  return 0;
}
