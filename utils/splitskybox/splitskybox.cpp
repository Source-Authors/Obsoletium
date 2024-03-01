// Copyright Valve Corporation, All rights reserved.
//
// Command line utility that splits a composite .PFM Skybox into separate .PFM
// images.
//
// See https://developer.valvesoftware.com/wiki/Splitskybox

#include "filesystem.h"
#include "filesystem_tools.h"
#include "cmdlib.h"

#include "tier1/utlbuffer.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"

#include "posix_file_stream.h"
#include "tools_minidump.h"

#include <memory>

namespace {

[[noreturn]] void Usage() {
  printf("Usage: splitskybox blah.pfm\n");

  exit(EINVAL);
}

int ReadIntFromUtlBuffer(CUtlBuffer &buffer) {
  int val = 0;

  while (1) {
    char c = buffer.GetChar();

    if (c >= '0' && c <= '9') {
      val = val * 10 + (c - '0');
    } else {
      return val;
    }
  }
}

// Loads a file into a UTLBuffer
bool LoadFile(const char *file_path, CUtlBuffer &buf) {
  auto [f, errc] = se::posix::posix_file_stream_factory::open(file_path, "rb");
  if (errc) {
    Warning("Unable to open '%s': %s.\n", file_path, errc.message().c_str());
    return false;
  }

  int64_t size;
  std::tie(size, errc) = f.size();
  if (errc) {
    Warning("Unable to get size of '%s': %s.\n", file_path,
            errc.message().c_str());
    return false;
  }

  buf.EnsureCapacity(size);

  std::tie(std::ignore, errc) = f.read(buf.Base(), size, 1, size);
  if (errc) {
    Warning("Unable to read '%s': %s.\n", file_path, errc.message().c_str());
    return false;
  }

  buf.SeekPut(CUtlBuffer::SEEK_HEAD, size);

  return true;
}

bool PFMRead(const char *file_path, CUtlBuffer &fileBuffer, int &nWidth,
             int &nHeight, float **ppImage) {
  fileBuffer.SeekGet(CUtlBuffer::SEEK_HEAD, 0);
  if (fileBuffer.GetChar() != 'P' || fileBuffer.GetChar() != 'F' ||
      fileBuffer.GetChar() != 0xa) {
    Warning("'%s' is not PFM. Expected PF<new_line> signature.\n", file_path);
    return false;
  }

  nWidth = ReadIntFromUtlBuffer(fileBuffer);
  nHeight = ReadIntFromUtlBuffer(fileBuffer);

  // eat crap until the next newline
  while (fileBuffer.GetChar() != 0xa) {
  }

  *ppImage = new float[nWidth * nHeight * 3];

  for (int y = nHeight - 1; y >= 0; y--) {
    fileBuffer.Get(*ppImage + nWidth * y * 3, nWidth * 3u * sizeof(float));
  }

  return true;
}

bool PFMWrite(float *img, const char *file_path, int width, int height) {
  auto [f, errc] = se::posix::posix_file_stream_factory::open(file_path, "wb");
  if (errc) {
    Warning("Unable to open '%s' for writing: %s.\n", file_path,
            errc.message().c_str());
    return false;
  }

  std::tie(std::ignore, errc) =
      f.print("PF\n%d %d\n-1.000000\n", width, height);
  if (errc) {
    Warning("Unable to write header to '%s': %s.\n", file_path,
            errc.message().c_str());
    return false;
  }

  for (int i = height - 1; i >= 0; i--) {
    float *row = &img[3 * width * i];

    std::tie(std::ignore, errc) = f.write(row, width * sizeof(float) * 3, 1);
    if (errc) {
      Warning("Unable to write row to '%s': %s.\n", file_path,
              errc.message().c_str());
      return false;
    }
  }

  return true;
}

void WriteSubRect(int src_width, int src_height, const float *src_image,
                  int src_offset_x, int src_offset_y, int dst_width,
                  int dst_height, const char *src_file_name,
                  const char *dst_file_extension,
                  std::unique_ptr<float[]> &dst_image) {
  for (int y = 0; y < dst_height; y++) {
    for (int x = 0; x < dst_width; x++) {
      const intp src_offset =
          ((static_cast<intp>(src_offset_x) + x) +
           (src_offset_y + y) * static_cast<intp>(src_width)) *
          3;
      const intp dst_offset = (static_cast<intp>(x) + (dst_width * y)) * 3;

      dst_image[dst_offset + 0] = src_image[src_offset + 0];
      dst_image[dst_offset + 1] = src_image[src_offset + 1];
      dst_image[dst_offset + 2] = src_image[src_offset + 2];
    }
  }

  char dstFileName[MAX_PATH];
  V_strcpy_safe(dstFileName, src_file_name);
  V_StripExtension(src_file_name, dstFileName, ssize(dstFileName));
  V_strcat_safe(dstFileName, dst_file_extension);
  V_strcat_safe(dstFileName, ".pfm");

  PFMWrite(dst_image.get(), dstFileName, dst_width, dst_height);
}

}  // namespace

int main(int argc, char **argv) {
  // Install an exception handler.
  const se::utils::common::ScopedDefaultMinidumpHandler
      scoped_default_minidumps;

  CommandLine()->CreateCmdLine(argc, argv);

#ifdef PLATFORM_64BITS
  Msg("\nValve Software - splitskybox [64 bit] (%s)\n", __DATE__);
#else
  Msg("\nValve Software - splitskybox (%s)\n", __DATE__);
#endif

  if (argc != 2) Usage();

  const ScopedFileSystem scoped_file_system{argv[argc - 1]};

  // Expand the path so that it can run correctly from a shortcut.
  char source[1024];
  V_strcpy_safe(source, ExpandPath(argv[1]));

  CUtlBuffer src_data;
  if (!LoadFile(argv[1], src_data)) return 2;

  float *src_image = nullptr;
  int src_width, src_height;
  if (!PFMRead(argv[1], src_data, src_width, src_height, &src_image)) {
    return 3;
  }

  const int dst_width = src_width / 4;
  const int dst_height = src_height / 3;

  Assert(dst_width == dst_height);

  // dimhotepus: Allocate dst_image buffer once to speed up.
  std::unique_ptr<float[]> dst_image = std::make_unique<float[]>(
      static_cast<size_t>(dst_width) * dst_height * 3);

  WriteSubRect(src_width, src_height, src_image, dst_width * 0, dst_height * 1,
               dst_width, dst_height, argv[1], "ft", dst_image);
  WriteSubRect(src_width, src_height, src_image, dst_width * 1, dst_height * 1,
               dst_width, dst_height, argv[1], "lf", dst_image);
  WriteSubRect(src_width, src_height, src_image, dst_width * 2, dst_height * 1,
               dst_width, dst_height, argv[1], "bk", dst_image);
  WriteSubRect(src_width, src_height, src_image, dst_width * 3, dst_height * 1,
               dst_width, dst_height, argv[1], "rt", dst_image);
  WriteSubRect(src_width, src_height, src_image, dst_width * 3, dst_height * 0,
               dst_width, dst_height, argv[1], "up", dst_image);
  WriteSubRect(src_width, src_height, src_image, dst_width * 3, dst_height * 2,
               dst_width, dst_height, argv[1], "dn", dst_image);

  CmdLib_Cleanup();

  return 0;
}
