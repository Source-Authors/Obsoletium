// Copyright Valve Corporation, All rights reserved.
//
// Purpose: Makes .DAT files

#include <iostream>
#include <tuple>

#include "scoped_file.h"

#include "tier0/basetypes.h"
#include "tier1/checksum_md5.h"
#include "tier1/strtools.h"

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
  const nd::ScopedFile f{file_name, "rb"};
  if (!f) {
    err << "Can't open file " << file_name
        << " to md5: " << strerror(f.open_error()) << ".\n";
    return false;
  }

  if (f.seek(0, SEEK_END)) {
    const auto rc = errno;
    err << "Can't seek file " << file_name << " end: " << strerror(rc) << ".\n";
    return false;
  }

  long long size{f.tell()};
  if (size <= 0) {
    const auto rc = errno;
    err << "Can't read file " << file_name << " size to md5: " << strerror(rc)
        << ".\n";
    return false;
  }

  if (f.seek(0, SEEK_SET)) {
    const auto rc = errno;
    err << "Can't seek file " << file_name << " start: " << strerror(rc)
        << ".\n ";
    return false;
  }

  MD5Context_t ctx = {};
  MD5Init(&ctx);

  byte chunk[1024];
  // Now read in chunks.
  while (size > 0) {
    const size_t bytes_read{
        f.read(chunk, std::min(std::size(chunk), static_cast<size_t>(size)))};
    // If any data was received, CRC it.
    if (bytes_read > 0) {
      size -= bytes_read;

      MD5Update(&ctx, chunk, bytes_read);
    }

    // We are at the end of file, break loop and return.
    if (f.eof()) break;

    if (f.last_error()) {
      err << "Can't read file " << file_name
          << " to md5: read error occured: " << strerror(f.last_error())
          << ".\n";
      return false;
    }
  }

  MD5Final(digest, &ctx);

  return true;
}

}  // namespace

// Purpose: newdat - makes the .DAT signature for file / virus checking.
// Output : int 0 == success, other == failure
int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "USAGE: newdat <filename>\n";
    return 1;
  }

  char file_name[MAX_PATH];
  // Get the filename without the extension
  Q_StripExtension(argv[1], file_name, std::size(file_name));

  char dat_file_name[MAX_PATH];
  sprintf(dat_file_name, "%s.dat", file_name);

  byte digest[MD5_DIGEST_LENGTH];
  // Build the MD5 hash for the .EXE file
  if (!MD5HashFile(digest, argv[1], std::cerr)) {
    std::cerr << "Can't md5 file " << argv[1] << ".\n";
    return 2;
  }

  // Write the first 4 bytes of the MD5 hash as the signature ".dat" file
  nd::ScopedFile f{dat_file_name, "wb"};
  if (f) {
    if (f.write(digest) != sizeof(digest)) {
      std::cerr << "Can't write md5 " << sizeof(digest) << " bytes to "
                << dat_file_name << ": " << strerror(f.last_error()) << ".\n";
      return 3;
    }

    if (f.close() != EOF) {
      std::cout << "Wrote md5 of " << argv[1] << " to " << dat_file_name
                << ".\n";
      return 0;
    }

    std::cerr << "Can't close " << dat_file_name << " to write md5 of "
              << argv[1] << " to: file may be not written.\n";
    return 4;
  }

  std::cerr << "Can't open " << dat_file_name << " to write md5 of " << argv[1]
            << " to: " << strerror(f.open_error()) << ".\n";
  return 5;
}
