// Copyright Valve Corporation, All rights reserved.

#ifndef SRC_LAUNCHER_SCOPED_APP_MULTIRUN_H
#define SRC_LAUNCHER_SCOPED_APP_MULTIRUN_H

#ifdef _WIN32
#include <sal.h>

using BOOL = int;
using HANDLE = void *;

using SECURITY_ATTRIBUTES = struct _SECURITY_ATTRIBUTES;
using LPSECURITY_ATTRIBUTES = SECURITY_ATTRIBUTES*;

extern "C" {
__declspec(dllimport) _Ret_maybenull_ HANDLE
    __stdcall CreateMutexW(_In_opt_ LPSECURITY_ATTRIBUTES lpMutexAttributes,
                           _In_ BOOL bInitialOwner,
                           _In_opt_ const wchar_t *lpName);
__declspec(dllimport) BOOL
    __stdcall CloseHandle(_In_ _Post_ptr_invalid_ HANDLE hObject);

__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(
    _In_ HANDLE hHandle, _In_ unsigned long dwMilliseconds);
}

constexpr inline unsigned long kWaitObject0{0};
constexpr inline unsigned long kWaitAbandoned{0x00000080L};

#else
#include "tier1/checksum_crc.h"
#include "tier1/strtools.h"

#define O_EXLOCK 0
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

namespace se::launcher {

// App multirun.
class ScopedAppMultiRun {
 public:
  ScopedAppMultiRun() noexcept
#if defined(WIN32)
      // don't allow more than one instance to run
      : mutex_{::CreateMutexW(nullptr, 0, L"hl2_singleton_mutex")}
#endif
  {
#if defined(WIN32)
    if (mutex_) {
      const unsigned long wait_result{::WaitForSingleObject(mutex_, 0)};

      // Here, we have the mutex
      if (wait_result == kWaitObject0 || wait_result == kWaitAbandoned) return;

      // couldn't get the mutex, we must be running another instance
      ::CloseHandle(mutex_);
      mutex_ = nullptr;
    }
#else
    // Under OSX use flock in /tmp/source_engine_<game>.lock, create
    // the file if it doesn't exist
    const char *game_arg{command_line->ParmValue("-game", DEFAULT_HL2_GAMEDIR)};

    CRC32_t gameCRC;
    CRC32_Init(&gameCRC);
    CRC32_ProcessBuffer(&gameCRC, game_arg, Q_strlen(game_arg));
    CRC32_Final(&gameCRC);

#if defined(LINUX)
    // Check TMPDIR environment variable for temp directory.
    const char *tmp_directory{getenv("TMPDIR")};

    // If it's NULL, or it doesn't exist, or it isn't a directory,
    // fallback to /tmp.
    struct stat buf;
    if (!tmp_directory || stat(tmp_directory, &buf) || !S_ISDIR(buf.st_mode)) {
      tmp_directory = "/tmp";
    }

    V_snprintf(lock_file_name_, sizeof(lock_file_name_),
               "%s/source_engine_%u.lock", tmp_directory, gameCRC);

    lock_handle_ = open(lock_file_name_, O_WRONLY | O_CREAT, 0666);
    if (lock_handle_ == -1) {
      fprintf(stderr, "open(%s) failed: %s\n", lock_file_name_,
              std::generic_category().message(errno));
      return;
    }

    // dimhotepus: Looks like CS:GO do this.
    // In case we have a umask setting creation to something other
    // than 0666, force it to 0666 so we don't lock other users out
    // of the game if the game dies etc.
    if (fchmod(lock_handle_, 0666) == -1) {
      fprintf(stderr, "fchmod(%s, %d) failed: %s\n", lock_file_name_, 0666,
              std::generic_category().message(errno));
      close(lock_handle_);
      lock_handle_ = -1;
      return;
    }

    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1;

    if (fcntl(lock_handle_, F_SETLK, &fl) == -1) {
      fprintf(stderr, "fcntl(%s) failed: %s\n", lock_file_name_,
              std::generic_category().message(errno));
      close(lock_handle_);
      lock_handle_ = -1;
      return;
    }
#else
    V_snprintf(lock_file_name_, sizeof(lock_file_name_),
               "/tmp/source_engine_%u.lock", gameCRC);

    lock_handle_ =
        open(lock_file_name_,
             O_CREAT | O_WRONLY | O_EXLOCK | O_NONBLOCK | O_TRUNC, 0777);
    if (lock_handle_ >= 0) {
      // make sure we give full perms to the file, we only one
      // instance per machine
      if (fchmod(lock_handle_, 0777) < 0) {
        fprintf(stderr, "fchmod(%s, %d) failed: %s\n", lock_file_name_, 0777,
                std::generic_category().message(errno));
        close(lock_handle_);
        lock_handle_ = -1;
        return;
      }

      // we leave the file open, under unix rules when we die we'll
      // automatically close and remove the locks
      return;
    }

    // We were unable to open the file, it should be because we are
    // unable to retain a lock
    if (errno != EWOULDBLOCK) {
      fprintf(stderr, "open(%s) failed: %s\n", lock_file_name_,
              std::generic_category().message(errno));
    }
#endif  // OSX
#endif  // !WIN32
  }

  ScopedAppMultiRun(const ScopedAppMultiRun &) = delete;
  ScopedAppMultiRun(ScopedAppMultiRun &&) = delete;
  ScopedAppMultiRun &operator=(const ScopedAppMultiRun &) = delete;
  ScopedAppMultiRun &operator=(ScopedAppMultiRun &&) = delete;

  bool is_single_run() const noexcept {
#if defined(WIN32)
    return !!mutex_;
#else
    return lock_handle_ != -1
#endif
  }

  ~ScopedAppMultiRun() noexcept {
#if defined(WIN32)
    if (mutex_) {
      ::ReleaseMutex(mutex_);
      ::CloseHandle(mutex_);
    }
#else
    if (lock_handle_ != -1) {
      close(lock_handle_);
      lock_handle_ = -1;
      unlink(lock_file_name_);
    }
#endif
  }

 private:
#ifdef WIN32
  HANDLE mutex_;
#else
  int lock_handle_ = -1;
  char lock_file_name_[MAX_PATH];
#endif
};

}  // namespace se::launcher

#endif  // SRC_LAUNCHER_SCOPED_APP_MULTIRUN_H
