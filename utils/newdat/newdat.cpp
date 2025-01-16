// Copyright Valve Corporation, All rights reserved.
//
// Purpose: Makes .md5 files

#include <iostream>
#include <tuple>

#include "tier0/basetypes.h"
#include "tier1/checksum_md5.h"
#include "tier1/strtools.h"
#include "posix_file_stream.h"

#undef min

namespace {

/**
 * @brief MD5 hashing file contents.
 * @param digest Result MD5.
 * @param file_name File name.
 * @param err Error.
 * @return true in success, false on failure.
 */
[[nodiscard]] bool MD5HashFile(byte (&digest)[MD5_DIGEST_LENGTH],
                               const char *file_name,
                               std::ostream &err) noexcept {
  auto [f, errc] = se::posix::posix_file_stream_factory::open(file_name, "rb");
  if (errc) {
    err << "Can't open file " << file_name << " to md5: " << errc.message()
        << ".\n";
    return false;
  }

  std::tie(std::ignore, errc) = f.seek(0, SEEK_END);
  if (errc) {
    err << "Can't seek file " << file_name << " end: " << errc.message()
        << ".\n";
    return false;
  }

  int64_t size;
  std::tie(size, errc) = f.tell();
  if (errc) {
    err << "Can't read file " << file_name << " size to md5: " << errc.message()
        << ".\n";
    return false;
  }

  std::tie(std::ignore, errc) = f.seek(0, SEEK_SET);
  if (errc) {
    err << "Can't seek file " << file_name << " start: " << errc.message()
        << ".\n ";
    return false;
  }

  MD5Context_t ctx = {};
  MD5Init(&ctx);

  byte chunk[1024];
  // Now read in chunks.
  while (size > 0) {
    size_t bytes_read;

    std::tie(bytes_read, errc) =
        f.read(chunk, std::min(std::size(chunk), static_cast<size_t>(size)));
    if (errc) {
      err << "Can't read file " << file_name << " chunk: " << errc.message()
          << ".\n ";
      return false;
    }

    // If any data was received, CRC it.
    if (bytes_read > 0) {
      size -= bytes_read;

      MD5Update(&ctx, chunk, bytes_read);
    }

    // We are at the end of file, break loop and return.
    bool eof;
    std::tie(eof, errc) = f.eof();
    if (errc) {
      err << "Can't read file " << file_name << " chunk: " << errc.message()
          << ".\n ";
      return false;
    }

    if (eof) break;
  }

  MD5Final(digest, &ctx);

  return true;
}

}  // namespace

// Purpose: newdat - makes the .md5 signature for file / virus checking.
// Output : int 0 == success, other == failure
int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Writes md5 signature for file to <filename>.md5\nUSAGE: newdat <filename>\n";
    return 1;
  }

  char file_name[MAX_PATH];
  // Get the filename without the extension
  Q_StripExtension(argv[1], file_name, std::size(file_name));

  char md5_file_name[MAX_PATH];
  V_sprintf_safe(md5_file_name, "%s.md5", file_name);

  byte digest[MD5_DIGEST_LENGTH];
  // Build the MD5 hash for the .EXE file
  if (!MD5HashFile(digest, argv[1], std::cerr)) {
    std::cerr << "Can't md5 file " << argv[1] << ".\n";
    return 2;
  }

  // Write MD5 hash as the signature ".md5" file
  auto [f, errc] =
      se::posix::posix_file_stream_factory::open(md5_file_name, "wb");
  if (!errc) {
    for (byte b : digest) {
      std::tie(std::ignore, errc) = f.print("%hhx", b);
      if (errc) {
        std::cerr << "Can't write md5 " << sizeof(digest) << " bytes to "
                  << md5_file_name << ": " << errc.message() << ".\n";
        return 3;
      }
    }

    std::tie(std::ignore, errc) = f.print(" *%s\r\n", V_GetFileName(argv[1]));
    if (errc) {
      std::cerr << "Can't write md5 file name to " << md5_file_name << ": "
                << errc.message() << ".\n";
      return 3;
    }

    std::cout << "Wrote md5 of " << argv[1] << " to " << md5_file_name << ".\n";
    return 0;
  }

  std::cerr << "Can't open " << md5_file_name << " to write md5 of " << argv[1]
            << " to: " << errc.message() << ".\n";
  return 4;
}
