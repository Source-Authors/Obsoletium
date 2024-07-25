// Copyright Valve Corporation, All rights reserved.

#ifndef NEWDAT_SCOPED_FILE_H_
#define NEWDAT_SCOPED_FILE_H_

#define _FILE_OFFSET_BITS 64
#include <type_traits>
#include <cstdio>

#include "tier0/basetypes.h"
#include "tier0/dbg.h"

namespace nd {

/**
 * @brief FILE wrapper.
 */
class ScopedFile {
 public:
  /**
   * @brief Ctor.
   * @param path File path.
   * @param options File options.
   */
  ScopedFile(const char *path, const char *options) noexcept
      : handle_{fopen(path, options)}, errc_{handle_ ? 0 : errno} {}

  ScopedFile(ScopedFile &) = delete;
  ScopedFile(ScopedFile &&) = delete;
  ScopedFile &operator=(ScopedFile &) = delete;
  ScopedFile &operator=(ScopedFile &&) = delete;

  ~ScopedFile() noexcept {
    [[maybe_unused]] const int rc{close()};
    Assert(rc == 0);
  }

  /**
   * @brief bool operator to check either file opened or not.
   */
  [[nodiscard]] explicit operator bool() const noexcept {
    return handle_ != nullptr;
  }
  /**
   * @brief ! operator to check either file opened or not.
   * @return false if opened, true otherwise.
   */
  [[nodiscard]] bool operator!() const noexcept { return handle_ == nullptr; }

  /**
   * @brief Arithmetic operator overloading to prevent type promotions in
   * expecting bool conditions.
   * @tparam T Type parameter.
   * @tparam SFINAE parameter, do not use.
   */
  template <typename T, typename = typename std::enable_if<
                            std::is_arithmetic<T>::value>::type>
  operator T() const noexcept = delete;

  /**
   * @brief Open error if any.
   * @return 0 if no error, non-0 otherwise.
   */
  [[nodiscard]] int open_error() const noexcept { return errc_; }

  /**
   * @brief Close file.
   * @return 0 if success, non-0 otherwise.
   */
  [[nodiscard]] int close() noexcept {
    if (handle_) {
      const int rc{fclose(handle_)};
      handle_ = nullptr;

      return rc;
    }

    return 0;
  }

  /**
   * @brief Seek file position.
   * @param off New position.
   * @param orig Seek type.
   * @return New position, -1 if error.
   */
  [[nodiscard]] long long seek(long long off, int orig) const noexcept {
#ifdef _WIN32
    return _fseeki64(handle_, off, orig);
#else
    return fseeko(handle_, off, orig);
#endif
  }

  /**
   * @brief Get file position.
   * @return File position, -1 if error.
   */
  [[nodiscard]] long long tell() const noexcept {
#ifdef _WIN32
    return _ftelli64(handle_);
#else
    return ftello(handle_);
#endif
  }

  /**
   * @brief Read content from file.
   * @tparam T Type to read from file.
   * @param buffer Buffer to set data to.
   * @param count Count of elements.
   * @return Read items count.
   */
  template <typename T>
  [[nodiscard]] size_t read(T *buffer, size_t count) const noexcept {
    return fread(buffer, sizeof(T), count, handle_);
  }

  /**
   * @brief Write content to file.
   * @tparam T Type to write.
   * @tparam count Count of elements.
   * @param buffer Buffer.
   * @return Written items count.
   */
  template <typename T, size_t count>
  [[nodiscard]] size_t write(const T (&buffer)[count]) const noexcept {
    return fwrite(buffer, sizeof(T), count, handle_);
  }

  /**
   * @brief Checks EOF.
   * @return 0 if not EOF, non-zero otherwise.
   */
  [[nodiscard]] int eof() const noexcept { return feof(handle_); }

  /**
   * @brief Get last stream error.
   * @return If no error has occurred on stream, returns 0. Otherwise, it
   * returns a nonzero value.
   */
  [[nodiscard]] int last_error() const noexcept { return ferror(handle_); }

 private:
  FILE *handle_;
  const int errc_;
};

}  // namespace nd

#endif  // !NEWDAT_SCOPED_FILE_H_
