// Copyright Valve Corporation, All rights reserved.

#ifndef SRC_POSIX_FILE_STREAM_H_
#define SRC_POSIX_FILE_STREAM_H_

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdint>

#include <system_error>
#include <tuple>

#include "tier0/dbg.h"

namespace se::posix {

// Input/output result.
template <typename T>
using io_result = std::tuple<T, std::error_code>;

[[nodiscard]] inline std::error_code make_posix_error_from_errno(int errc) {
  return std::error_code{errc, std::generic_category()};
}

[[nodiscard]] inline std::error_code posix_error_last() {
  return make_posix_error_from_errno(errno);
}

const inline std::error_code posix_error_ok{0, std::generic_category()};

[[nodiscard]] inline std::error_code make_posix_error_from_result(int result) {
  return result ? posix_error_last() : posix_error_ok;
}

// stdio-based file stream.
class posix_file_stream {
  // Allows factory to open files.
  friend class posix_file_stream_factory;

 public:
  posix_file_stream(const posix_file_stream& f) = delete;
  posix_file_stream& operator=(const posix_file_stream& f) = delete;

  // Move constructor.
  posix_file_stream(posix_file_stream&& f) noexcept : fd_{f.fd_} {
    f.fd_ = nullptr;
  }

  // Move assignment operator.
  posix_file_stream& operator=(posix_file_stream&& f) noexcept {
    [[maybe_unused]] const std::error_code errno_code{close()};
    Assert(errno_code == posix_error_ok);

    std::swap(fd_, f.fd_);

    return *this;
  }

  // Like fscanf_s, read from file by |format|.
  [[nodiscard]] io_result<size_t> scan(
      _In_z_ _Scanf_s_format_string_ const char* format, ...) const noexcept {
    // fscanf_s analog.
    int fields_assigned_count;
    va_list arg_list;
    va_start(arg_list, format);
    fields_assigned_count = _vfscanf_s_l(fd_, format, nullptr, arg_list);
    va_end(arg_list);

    return {fields_assigned_count != EOF ? fields_assigned_count : 0,
            make_posix_error_from_result(ferror(fd_))};
  }

  // Like feof.
  [[nodiscard]] io_result<bool> eof() const noexcept {
    const int c{feof(fd_)};
    return {c != 0, posix_error_last()};
  }

  // Like fgetc, get char from file.
  [[nodiscard]] io_result<int> getc() const noexcept {
    const int c{fgetc(fd_)};
    return {c, c != EOF ? posix_error_ok
                        : make_posix_error_from_result(ferror(fd_))};
  }

  // Like fgets, get C string from file.
  template <size_t buffer_size>
  [[nodiscard]] io_result<char*> gets(
      _Out_writes_z_(buffer_size) char (&buffer)[buffer_size]) const noexcept {
    char* string{fgets(buffer, buffer_size, fd_)};
    return {string, string != nullptr
                        ? posix_error_ok
                        : make_posix_error_from_result(ferror(fd_))};
  }

  // Reads into |buffer| of size |buffer_size| |elements_count| elements.
  template <typename T, size_t buffer_size>
  [[nodiscard]] io_result<size_t> read(
      T (&buffer)[buffer_size], const size_t elements_count) const noexcept {
    return read(buffer, buffer_size * sizeof(T), sizeof(T), elements_count);
  }

  // Reads into |buffer|.
  template <typename T, size_t buffer_size>
  [[nodiscard]] io_result<size_t> read(
      T (&buffer)[buffer_size]) const noexcept {
    return read(buffer, buffer_size);
  }

  // Reads into |buffer| with size |buffer_size|, terminates read data with
  // '\0'.
  [[nodiscard]] io_result<size_t> read(
      char* buffer, const size_t buffer_size) const noexcept {
    auto [read_size, error_info] =
        read(buffer, buffer_size * sizeof(char), sizeof(char), buffer_size - 1);

    if (!error_info) {
      buffer[read_size] = '\0';
    }

    return {read_size, error_info};
  }

  // Reads into |buffer|, terminates read data with '\0'.
  template <size_t buffer_size>
  [[nodiscard]] io_result<size_t> read(
      char (&buffer)[buffer_size]) const noexcept {
    static_assert(buffer_size >= 1);

    return read(buffer, buffer_size);
  }

  // Reads into |buffer|with size |buffer_size|, terminates read data with
  // L'\0'.
  [[nodiscard]] io_result<size_t> read(
      wchar_t* buffer, const size_t buffer_size) const noexcept {
    auto [read_size, error_info] = read(buffer, buffer_size * sizeof(wchar_t),
                                        sizeof(wchar_t), buffer_size - 1);

    if (!error_info) {
      buffer[read_size] = L'\0';
    }

    return {read_size, error_info};
  }

  // Reads into |buffer|, terminates read data with L'\0'.
  template <size_t buffer_size>
  [[nodiscard]] io_result<size_t> read(
      wchar_t (&buffer)[buffer_size]) const noexcept {
    static_assert(buffer_size >= 1);

    return read(buffer, buffer_size);
  }

  // Like fprintf_s, write to file by |format|.
  [[nodiscard]] io_result<size_t> print(
      _In_z_ _Printf_format_string_ const char* format, ...) const noexcept {
    // fprintf_s analog.
    int bytes_written_count;
    va_list arg_list;
    va_start(arg_list, format);
    bytes_written_count = _vfprintf_s_l(fd_, format, nullptr, arg_list);
    va_end(arg_list);

    return {bytes_written_count >= 0 ? bytes_written_count : 0,
            make_posix_error_from_result(ferror(fd_))};
  }

  // Writes |elements_count| elements from |buffer| to file.
  template <typename T, size_t buffer_size>
  [[nodiscard]] io_result<size_t> write(
      T (&buffer)[buffer_size], const size_t elements_count) const noexcept {
    return write(&buffer, sizeof(T), elements_count);
  }

  // Writes all elements from |buffer| to file.
  template <typename T, size_t buffer_size>
  [[nodiscard]] io_result<size_t> write(
      T (&buffer)[buffer_size]) const noexcept {
    return write(&buffer, sizeof(T), buffer_size);
  }

  // Reads data from file into |buffer| of size |buffer_size_bytes| with element
  // of |element_size_bytes| and elements count |max_elements_count|.  Gets "the
  // number of (whole) items that were read into the buffer, which may be less
  // than count if a read error or the end of the file is encountered before
  // count is reached".  Uses fread_s internally, see
  // https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/fread-s.
  [[nodiscard]] io_result<size_t> read(
      void* buffer, const size_t buffer_size_bytes,
      const size_t element_size_bytes,
      const size_t max_elements_count) const noexcept {
    return {fread_s(buffer, buffer_size_bytes, element_size_bytes,
                    max_elements_count, fd_),
            make_posix_error_from_result(ferror(fd_))};
  }

  // Writes data from |buffer| of size |element_size_bytes| and count
  // |max_elements_count| to file. Gets "the number of full items actually
  // written, which may be less than count if an error occurs. Also, if an error
  // occurs, the file-position indicator cannot be determined".  Uses fwrite
  // internally, see
  // https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/fwrite.
  [[nodiscard]] io_result<size_t> write(
      const void* buffer, const size_t element_size_bytes,
      const size_t max_elements_count) const noexcept {
    return {std::fwrite(buffer, element_size_bytes, max_elements_count, fd_),
            make_posix_error_from_result(ferror(fd_))};
  }

  // Moves the file pointer to a specified location.
  [[nodiscard]] io_result<int> seek(const int64_t offset,
                                    const int origin) const noexcept {
    const int errno_code{_fseeki64(fd_, offset, origin)};
    return {errno_code, errno_code == 0 ? posix_error_ok : posix_error_last()};
  }

  // Gets the current position of a file pointer.
  [[nodiscard]] io_result<int64_t> tell() const noexcept {
    const int64_t file_pos{_ftelli64(fd_)};
    return {file_pos, file_pos != -1L ? posix_error_ok : posix_error_last()};
  }

  ~posix_file_stream() noexcept {
    [[maybe_unused]] const std::error_code errno_code{close()};
    Assert(errno_code == posix_error_ok);
  }

 private:
  // stdio file descriptor.
  FILE* fd_;

  // Closes file.
  [[nodiscard]] std::error_code close() noexcept {
    const std::error_code errno_code{
        fd_ ? make_posix_error_from_result(fclose(fd_)) : posix_error_ok};
    fd_ = nullptr;
    return errno_code;
  }

  // Opens file stream from file path |file_path| and mode |mode|.
  posix_file_stream(_In_opt_ FILE* fd) noexcept : fd_{fd} {}

  // Empty file stream, invalid.
  [[nodiscard]] static posix_file_stream null() noexcept {
    return posix_file_stream{nullptr};
  }
};

// Small file factory to simplify initial file open process.
class posix_file_stream_factory {
 public:
  // Opens file with path |file_path| and mode |mode|.
  [[nodiscard]] static io_result<posix_file_stream> open(
      const char* file_path, const char* mode) noexcept {
    FILE* fd{nullptr};
    const int errno_code{fopen_s(&fd, file_path, mode)};
    return errno_code == posix_error_ok.value()
               ? std::make_tuple(posix_file_stream{fd}, posix_error_ok)
               : std::make_tuple(posix_file_stream::null(),
                                 make_posix_error_from_errno(errno_code));
  }

  posix_file_stream_factory() = delete;
  ~posix_file_stream_factory() = delete;

  posix_file_stream_factory(const posix_file_stream_factory& f) = delete;
  posix_file_stream_factory(posix_file_stream_factory&& f) = delete;
  posix_file_stream_factory& operator=(const posix_file_stream_factory& f) =
      delete;
  posix_file_stream_factory& operator=(posix_file_stream_factory&& f) = delete;
};

}  // namespace se::posix

#endif  // !SRC_POSIX_FILE_STREAM_H_
