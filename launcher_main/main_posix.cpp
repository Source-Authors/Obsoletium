// Copyright Valve Corporation, All rights reserved.
//
// Purpose: A redirection tool that allows the DLLs to reside elsewhere.

#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstring>

#include "tier0/basetypes.h"
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
  const char kLauncherPath[] =
#ifndef
      "bin/x64/launcher" DLL_EXT_STRING;
#else
      "bin/launcher" DLL_EXT_STRING;
#endif
  const source::ScopedDll launcher_dll {
     kLauncherPath, RTLD_NOW
  };
  if (!launcher_dll) {
    fprintf(stderr, "Failed to load the %s: %s.\n",
            kLauncherPath, ::dlerror());
    return 1;
  }

  using LauncherMainFunction = int (*)(int argc, char **argv);

  const auto main =
      launcher_dll.GetFunction<LauncherMainFunction>("LauncherMain");
  if (!main) {
    fprintf(stderr, "Failed to get the launcher LauncherMain entry proc: %s.\n",
            ::dlerror());
    return 2;
  }

#if defined(__clang__) && !defined(OSX)
  // When building with clang we absolutely need the allocator to always
  // give us 16-byte aligned memory because if any objects are tagged as
  // being 16-byte aligned then clang will generate SSE instructions to move
  // and initialize them, and an allocator that does not respect the
  // contract will lead to crashes. On Linux we normally use the default
  // allocator which does not give us this alignment guarantee.
  // The google tcmalloc allocator gives us this guarantee.
  // Test the current allocator to make sure it gives us the required alignment.
  void *pointers[20];
  for (int i = 0; i < ARRAYSIZE(pointers); ++i) {
    void *p = malloc(16);
    pointers[i] = p;
    if (((size_t)p) & 0xF) {
      fprintf(stderr, "%p is not 16-byte aligned. Aborting.\n", p);
      fprintf(stderr, "Pass /define:CLANG to VPC to correct this.\n");
      return -10;
    }
  }

  for (int i = 0; i < ARRAYSIZE(pointers); ++i) {
    if (pointers[i]) free(pointers[i]);
  }

  if (__has_feature(address_sanitizer)) {
    printf("Address sanitizer is enabled.\n");
  } else {
    printf("No address sanitizer!\n");
  }
#endif

#ifdef LINUX
  WaitForDebuggerConnect(argc, argv, 30);
#endif

  const auto rc = main(argc, argv);

  // Prevent tail call optimization and incorrect stack traces.
  exit(rc);
}
