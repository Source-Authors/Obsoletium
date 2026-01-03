// Copyright Valve Corporation, All rights reserved.
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.

#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>
#include <cerrno>

#include "tier0/basetypes.h"
#include "tier0/platform.h"
#include "scoped_dll.h"

namespace {

#ifdef LINUX

#include <fcntl.h>

bool IsDebuggerPresent(int time) {
  // Need to get around the __wrap_open() stuff. Just find the open symbol
  // directly and use it...
  using open_func_t = int()(const char *pathname, int flags, mode_t mode);
  open_func_t *open_func = (open_func_t *)dlsym(RTLD_NEXT, "open");

  if (open_func) {
    for (int i = 0; i < time; i++) {
      int tracerpid = -1;

      int fd = (*open_func)("/proc/self/status", O_RDONLY, S_IRUSR);
      if (fd >= 0) {
        char buf[4096];
        static const char tracerpid_str[] = "TracerPid:";

        const int len = read(fd, buf, sizeof(buf) - 1);
        if (len > 0) {
          buf[len] = 0;

          const char *str = strstr(buf, tracerpid_str);
          tracerpid = str ? atoi(str + sizeof(tracerpid_str)) : -1;
        }

        close(fd);
      }

      if (tracerpid > 0) return true;

      sleep(1);
    }
  }

  return false;
}

void WaitForDebuggerConnect(int argc, char *argv[], int time) {
  for (int i = 1; i < argc; i++) {
    if (strstr(argv[i], "-wait_for_debugger")) {
      printf("\nArg -wait_for_debugger found.\nWaiting %dsec for debugger...\n",
             time);
      printf("  pid = %d\n", getpid());

      if (IsDebuggerPresent(time)) printf("Debugger connected...\n\n");

      break;
    }
  }
}

#endif  // LINUX

}  // namespace

int main(int argc, char *argv[]) {
  // Must add 'bin' to the path....
  const char *path_env{::getenv("LD_LIBRARY_PATH")};

  char cwd[PATH_MAX];
  if (!::getcwd(cwd, sizeof(cwd))) {
    const auto rc = errno;
    fprintf(stderr, "getcwd failed: %s", strerror(rc));
    return rc;
  }

  char new_path_env[4096];
  snprintf(new_path_env, sizeof(new_path_env) - 1,
           "LD_LIBRARY_PATH=%s/" PLATFORM_BIN_DIR ":%s", cwd, path_env);
  if (putenv(new_path_env)) {
    const auto rc = errno;
    fprintf(stderr, "putenv (%s) failed: %s\n", new_path_env, strerror(rc));
    return rc;
  }

  const source::ScopedDll tier0{"libtier0" DLL_EXT_STRING, RTLD_NOW};
  if (!tier0) {
    fprintf(stderr, "open %s failed: %s\n", "libtier0" DLL_EXT_STRING,
            ::dlerror());
    return 1;
  }

  const source::ScopedDll vstdlib{"libvstdlib" DLL_EXT_STRING, RTLD_NOW};
  if (!vstdlib) {
    fprintf(stderr, "open %s failed: %s\n", "libvstdlib" DLL_EXT_STRING,
            ::dlerror());
    return 2;
  }

  constexpr char dedicated_name[]{"dedicated" DLL_EXT_STRING};
  const source::ScopedDll dedicated{dedicated_name, RTLD_NOW};
  if (!dedicated) {
    fprintf(stderr, "open %s failed: %s\n", dedicated_name, ::dlerror());
    return 3;
  }

  using DedicatedMainFunction = int (*)(int argc, char *argv[]);
  const auto dedicated_main =
      dedicated.GetFunction<DedicatedMainFunction>("DedicatedMain");
  if (!dedicated_main) {
    fprintf(stderr, "Failed to find dedicated server entry point: %s\n",
            ::dlerror());
    return 4;
  }

#ifdef LINUX
  WaitForDebuggerConnect(argc, argv, 30);
#endif

  const int rc{dedicated_main(argc, argv)};

  // Prevent tail call optimization and incorrect stack traces.
  exit(rc);
}
