// Copyright Valve Corporation, All rights reserved.
//
// Command-line tool used to convert files from the VTF format to the TGA
// format.  The tool can also decompile uncompressed HDR VTFs into PFM files,
// despite the name.
//
// See https://developer.valvesoftware.com/wiki/VTF2TGA

#ifdef _WIN32
#include <direct.h>
#endif

#include <memory>

#include "mathlib/mathlib.h"
#include "bitmap/float_bm.h"
#include "bitmap/tgawriter.h"
#include "vtf/vtf.h"

#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "tier1/strtools.h"
#include "tier1/utlbuffer.h"
#include "tier2/tier2.h"

#include "filesystem.h"
#include "../common/tools_minidump.h"
#include "posix_file_stream.h"
#include "scoped_app_locale.h"

#include "tier0/memdbgon.h"

namespace {

SpewRetval_t VTF2TGAOutputFunc(SpewType_t spewType, char const *pMsg) {
  if (spewType == SPEW_ERROR || spewType == SPEW_WARNING) {
    fprintf(stderr, "%s", pMsg);
  } else {
    printf("%s", pMsg);
    fflush(stdout);
  }

  Plat_DebugString(pMsg);

  if (spewType == SPEW_ERROR) return SPEW_ABORT;

  return (spewType == SPEW_ASSERT) ? SPEW_DEBUGGER : SPEW_CONTINUE;
}

[[noreturn]] void Usage() {
  Warning("usage: vtf2tga -i <input vtf> [-o <output tga>] [-mip]\n");
  exit(EINVAL);
}

}  // namespace

int main(int argc, char **argv) {
  // Install an exception handler.
  const se::utils::common::ScopedDefaultMinidumpHandler
      scoped_default_minidumps;

  const ScopedSpewOutputFunc scoped_spew_output{VTF2TGAOutputFunc};

  // dimhotepus: Apply en_US UTF8 locale for printf/scanf.
  //
  // Printf/sscanf functions expect en_US UTF8 localization.
  //
  // Starting in Windows 10 version 1803 (10.0.17134.0), the Universal C Runtime
  // supports using a UTF-8 code page.
  constexpr char kEnUsUtf8Locale[]{"en_US.UTF-8"};

  const se::ScopedAppLocale scoped_app_locale{kEnUsUtf8Locale};
  if (V_stricmp(se::ScopedAppLocale::GetCurrentLocale(), kEnUsUtf8Locale)) {
    fprintf(stderr, "setlocale('%s') failed, current locale is '%s'.\n",
            kEnUsUtf8Locale, se::ScopedAppLocale::GetCurrentLocale());
  }

  CommandLine()->CreateCmdLine(argc, argv);
  MathLib_Init(2.2f, 2.2f, 0.0f, 1, false, false, false, false);
  const ScopedDefaultFileSystem scoped_default_file_system;

  const char *vtf_file_name{CommandLine()->ParmValue("-i")};
  const char *tga_file_name{CommandLine()->ParmValue("-o")};
  const bool should_generate_mpi_levels{CommandLine()->HasParm("-mip")};

  if (!vtf_file_name) Usage();
  if (!tga_file_name) tga_file_name = vtf_file_name;

  char cwd[MAX_PATH];
  if (_getcwd(cwd, sizeof(cwd)) == nullptr) {
    const int err = errno;
    fprintf(stderr, "unable to get the current directory: %s.\n",
            std::generic_category().message(err).c_str());
    return err;
  }
  V_StripTrailingSlash(cwd);

  char buffer[MAX_PATH];
  if (!V_IsAbsolutePath(tga_file_name)) {
    V_sprintf_safe(buffer, "%s\\%s", cwd, tga_file_name);
  } else {
    V_strcpy_safe(buffer, tga_file_name);
  }
  V_FixSlashes(buffer);

  char out_file_base[MAX_PATH];
  V_StripExtension(buffer, out_file_base);

  char actual_vtf_file_name[MAX_PATH];
  V_strcpy_safe(actual_vtf_file_name, vtf_file_name);
  if (!Q_strstr(actual_vtf_file_name, ".vtf")) {
    V_strcat_safe(actual_vtf_file_name, ".vtf");
  }

  auto [f, errc] =
      se::posix::posix_file_stream_factory::open(actual_vtf_file_name, "rb");
  if (errc) {
    Warning("unable to open '%s' for reading: %s.\n", actual_vtf_file_name,
            errc.message().c_str());
    return errc.value();
  }

  int64 size;
  std::tie(size, errc) = f.size();
  if (errc || size > std::numeric_limits<intp>::max()) {
    Warning("unable to get size of '%s': %s.\n", actual_vtf_file_name,
            errc.message().c_str());
    return errc.value();
  }

  intp correct_size = static_cast<intp>(size);

  CUtlBuffer vtf_buffer;
  vtf_buffer.EnsureCapacity(correct_size);

  size_t bytes_read;
  std::tie(bytes_read, errc) =
      f.read(vtf_buffer.Base(), correct_size, 1, correct_size);
  if (errc) {
    Warning("unable to read '%s': %s.\n", actual_vtf_file_name,
            errc.message().c_str());
    return errc.value();
  }

  vtf_buffer.SeekPut(CUtlBuffer::SEEK_HEAD, bytes_read);

  IVTFTexture *pTex = CreateVTFTexture();
  if (!pTex) {
    Warning("error allocating .VTF for file '%s'.\n", actual_vtf_file_name);
    return ENOMEM;
  }

  if (!pTex->Unserialize(vtf_buffer)) {
    Warning("error deserializing .VTF file '%s'.\n", actual_vtf_file_name);
    return EINVAL;
  }

  Msg("vtf width: %d\n", pTex->Width());
  Msg("vtf height: %d\n", pTex->Height());
  Msg("vtf numFrames: %d\n", pTex->FrameCount());

  // dimhotepus: Extract duplicated code to lambda.
  const auto BoolFlag = [pTex](int flag) {
    return (pTex->Flags() & flag) ? "true" : "false";
  };

  Msg("TEXTUREFLAGS_POINTSAMPLE=%s\n", BoolFlag(TEXTUREFLAGS_POINTSAMPLE));
  Msg("TEXTUREFLAGS_TRILINEAR=%s\n", BoolFlag(TEXTUREFLAGS_TRILINEAR));
  Msg("TEXTUREFLAGS_CLAMPS=%s\n", BoolFlag(TEXTUREFLAGS_CLAMPS));
  Msg("TEXTUREFLAGS_CLAMPT=%s\n", BoolFlag(TEXTUREFLAGS_CLAMPT));
  Msg("TEXTUREFLAGS_CLAMPU=%s\n", BoolFlag(TEXTUREFLAGS_CLAMPU));
  Msg("TEXTUREFLAGS_BORDER=%s\n", BoolFlag(TEXTUREFLAGS_BORDER));
  Msg("TEXTUREFLAGS_ANISOTROPIC=%s\n", BoolFlag(TEXTUREFLAGS_ANISOTROPIC));
  Msg("TEXTUREFLAGS_HINT_DXT5=%s\n", BoolFlag(TEXTUREFLAGS_HINT_DXT5));
  Msg("TEXTUREFLAGS_SRGB=%s\n", BoolFlag(TEXTUREFLAGS_SRGB));
  Msg("TEXTUREFLAGS_NORMAL=%s\n", BoolFlag(TEXTUREFLAGS_NORMAL));
  Msg("TEXTUREFLAGS_NOMIP=%s\n", BoolFlag(TEXTUREFLAGS_NOMIP));
  Msg("TEXTUREFLAGS_NOLOD=%s\n", BoolFlag(TEXTUREFLAGS_NOLOD));
  Msg("TEXTUREFLAGS_ALL_MIPS=%s\n", BoolFlag(TEXTUREFLAGS_ALL_MIPS));
  Msg("TEXTUREFLAGS_PROCEDURAL=%s\n", BoolFlag(TEXTUREFLAGS_PROCEDURAL));
  Msg("TEXTUREFLAGS_ONEBITALPHA=%s\n", BoolFlag(TEXTUREFLAGS_ONEBITALPHA));
  Msg("TEXTUREFLAGS_EIGHTBITALPHA=%s\n", BoolFlag(TEXTUREFLAGS_EIGHTBITALPHA));
  Msg("TEXTUREFLAGS_ENVMAP=%s\n", BoolFlag(TEXTUREFLAGS_ENVMAP));
  Msg("TEXTUREFLAGS_RENDERTARGET=%s\n", BoolFlag(TEXTUREFLAGS_RENDERTARGET));
  Msg("TEXTUREFLAGS_DEPTHRENDERTARGET=%s\n",
      BoolFlag(TEXTUREFLAGS_DEPTHRENDERTARGET));
  Msg("TEXTUREFLAGS_NODEBUGOVERRIDE=%s\n",
      BoolFlag(TEXTUREFLAGS_NODEBUGOVERRIDE));
  Msg("TEXTUREFLAGS_SINGLECOPY=%s\n", BoolFlag(TEXTUREFLAGS_SINGLECOPY));

  const Vector reflectivity = pTex->Reflectivity();
  Msg("vtf reflectivity: %f %f %f\n", reflectivity[0], reflectivity[1],
      reflectivity[2]);
  Msg("transparency: ");
  // dimhotepus: Unify transparency handling with other flags.
  Msg("TEXTUREFLAGS_EIGHTBITALPHA=%s\n", BoolFlag(TEXTUREFLAGS_EIGHTBITALPHA));
  Msg("TEXTUREFLAGS_ONEBITALPHA=%s\n", BoolFlag(TEXTUREFLAGS_ONEBITALPHA));
  if (!(pTex->Flags() &
        (TEXTUREFLAGS_EIGHTBITALPHA & TEXTUREFLAGS_ONEBITALPHA))) {
    Msg("noalpha\n");
  }

  const ImageFormat src_format = pTex->Format();
  Msg("vtf format: %s\n", ImageLoader::GetName(src_format));

  const intp tga_name_size = V_strlen(out_file_base);

  const int src_face_count = pTex->FaceCount();
  const int src_frame_count = pTex->FrameCount();
  const bool src_is_cubemap = pTex->IsCubeMap();

  const char *cube_face_names[7] = {"rt", "lf", "bk", "ft", "up", "dn", "sph"};

  const int last_mip_level =
      should_generate_mpi_levels ? pTex->MipCount() - 1 : 0;
  for (int frame_no = 0; frame_no < src_frame_count; ++frame_no) {
    for (int mip_level_no = 0; mip_level_no <= last_mip_level; ++mip_level_no) {
      int mip_width, mip_height, mip_depth;
      pTex->ComputeMipLevelDimensions(mip_level_no, &mip_width, &mip_height,
                                      &mip_depth);

      for (int cube_face_no = 0; cube_face_no < src_face_count;
           ++cube_face_no) {
        for (int z = 0; z < mip_depth; ++z) {
          // Construct output filename
          std::unique_ptr<char[]> temp_name =
              std::make_unique<char[]>(tga_name_size + 13);
          V_strncpy(temp_name.get(), out_file_base, tga_name_size + 1);

          char *ext = Q_strrchr(temp_name.get(), '.');
          if (ext) ext = 0;

          if (src_is_cubemap) {
            Assert(pTex->Depth() == 1);  // shouldn't this be 1 instead of 0?

            V_strcat(temp_name.get(), cube_face_names[cube_face_no],
                     tga_name_size + 13);
          }

          if (src_frame_count > 1) {
            char pTemp[4];  //-V112
            V_sprintf_safe(pTemp, "%03d", frame_no);
            V_strcat(temp_name.get(), pTemp, tga_name_size + 13);
          }

          if (last_mip_level != 0) {
            char pTemp[8];
            V_sprintf_safe(pTemp, "_mip%d", mip_level_no);
            V_strcat(temp_name.get(), pTemp, tga_name_size + 13);
          }

          if (pTex->Depth() > 1) {
            char pTemp[6];
            V_sprintf_safe(pTemp, "_z%03d", z);
            V_strcat(temp_name.get(), pTemp, tga_name_size + 13);
          }

          V_strcat(temp_name.get(),
                   src_format == IMAGE_FORMAT_RGBA16161616F ? ".pfm" : ".tga",
                   tga_name_size + 13);

          unsigned char *src_data =
              pTex->ImageData(frame_no, cube_face_no, mip_level_no, 0, 0, z);

          ImageFormat dst_format;
          if (src_format == IMAGE_FORMAT_RGBA16161616F) {
            dst_format = IMAGE_FORMAT_RGB323232F;
          } else {
            if (ImageLoader::IsTransparent(src_format) ||
                src_format == IMAGE_FORMAT_ATI1N ||
                src_format == IMAGE_FORMAT_ATI2N) {
              dst_format = IMAGE_FORMAT_BGRA8888;
            } else {
              dst_format = IMAGE_FORMAT_BGR888;
            }
          }
          //	dstFormat = IMAGE_FORMAT_RGBA8888;
          //	dstFormat = IMAGE_FORMAT_RGB888;
          //	dstFormat = IMAGE_FORMAT_BGRA8888;
          //	dstFormat = IMAGE_FORMAT_BGR888;
          //	dstFormat = IMAGE_FORMAT_BGRA5551;
          //	dstFormat = IMAGE_FORMAT_BGR565;
          //	dstFormat = IMAGE_FORMAT_BGRA4444;
          //	printf( "dstFormat: %s\n", ImageLoader::GetName( dstFormat ) );
          std::unique_ptr<unsigned char[]> dst_data =
              std::make_unique<unsigned char[]>(ImageLoader::GetMemRequired(
                  mip_width, mip_height, 1, dst_format, false));
          if (!ImageLoader::ConvertImageFormat(src_data, src_format,  //-V1051
                                               dst_data.get(), dst_format,
                                               mip_width, mip_height, 0, 0)) {
            Warning("error converting '%s' from '%s' to '%s'.\n",
                    actual_vtf_file_name, ImageLoader::GetName(src_format),
                    ImageLoader::GetName(dst_format));
            return EINVAL;
          }

          if (dst_format != IMAGE_FORMAT_RGB323232F) {
            if (ImageLoader::IsTransparent(dst_format) &&
                dst_format != IMAGE_FORMAT_RGBA8888) {
              std::unique_ptr<unsigned char[]> tmp_data{std::move(dst_data)};
              dst_data =
                  std::make_unique<unsigned char[]>(ImageLoader::GetMemRequired(
                      mip_width, mip_height, 1, IMAGE_FORMAT_RGBA8888, false));

              if (!ImageLoader::ConvertImageFormat(
                      tmp_data.get(), dst_format, dst_data.get(),
                      IMAGE_FORMAT_RGBA8888, mip_width, mip_height, 0, 0)) {
                Warning("error converting '%s' from '%s' to '%s'.\n",
                        actual_vtf_file_name, ImageLoader::GetName(dst_format),
                        ImageLoader::GetName(IMAGE_FORMAT_RGBA8888));
                return EINVAL;
              }

              dst_format = IMAGE_FORMAT_RGBA8888;
            } else if (!ImageLoader::IsTransparent(dst_format) &&
                       dst_format != IMAGE_FORMAT_RGB888) {
              std::unique_ptr<unsigned char[]> tmp_data{std::move(dst_data)};
              dst_data =
                  std::make_unique<unsigned char[]>(ImageLoader::GetMemRequired(
                      mip_width, mip_height, 1, IMAGE_FORMAT_RGB888, false));

              if (!ImageLoader::ConvertImageFormat(
                      tmp_data.get(), dst_format, dst_data.get(),
                      IMAGE_FORMAT_RGB888, mip_width, mip_height, 0, 0)) {
                Warning("error converting '%s' from '%s' to '%s'.\n",
                        actual_vtf_file_name, ImageLoader::GetName(dst_format),
                        ImageLoader::GetName(IMAGE_FORMAT_RGB888));
                return EINVAL;
              }

              dst_format = IMAGE_FORMAT_RGB888;
            }

            CUtlBuffer outBuffer;
            if (!TGAWriter::WriteToBuffer(dst_data.get(), outBuffer, mip_width,
                                          mip_height, dst_format, dst_format)) {
              Warning("unable to write TGA in format '%s'.\n",
                      ImageLoader::GetName(dst_format));
              return EIO;
            }

            if (!g_pFullFileSystem->WriteFile(temp_name.get(), nullptr,
                                              outBuffer)) {
              Warning("unable to write result '%s'.\n", temp_name.get());
              return EIO;
            }
          } else {
            if (!PFMWrite((float *)dst_data.get(), temp_name.get(), mip_width,
                          mip_height)) {
              return EIO;
            }
          }
        }
      }
    }
  }

  return 0;
}
