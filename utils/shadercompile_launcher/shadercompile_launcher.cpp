// Copyright Valve Corporation, All rights reserved.
//
// HLSL shader compiler launcher.

#include <direct.h>
#include <cstdio>
#include <system_error>

#include "winlite.h"
#include "scoped_dll.h"
#include "posix_file_stream.h"

#include "tier1/interface.h"
#include "tier1/strtools.h"
#include "ishadercompiledll.h"

namespace {

template <ptrdiff_t size>
[[nodiscard]] std::error_code MakeFullPath(const char *in,
                                           char (&out)[size]) noexcept {
  if (in[0] == '/' || in[0] == '\\' || in[1] == ':') {
    // It's already a full path.
    strncpy_s(out, in, size);
  } else {
    if (!_getcwd(out, size)) {
      return std::error_code{errno, std::generic_category()};
    }

    strncat_s(out, "\\", _TRUNCATE);
    strncat_s(out, in, _TRUNCATE);
  }

  return {};
}

}  // namespace

int main(int argc, char *argv[]) {
  char exe_name[MAX_PATH];
  Q_FileBase(argv[0], exe_name, ssize(exe_name));

  char full_path[MAX_PATH];
  std::error_code errc0{MakeFullPath(argv[0], full_path)};
  if (errc0) {
    fprintf(stderr, "%s error: Unable to make full path to %s: %s.\n", exe_name,
            argv[0], errc0.message().c_str());
    return errc0.value();
  }

  Q_StripFilename(full_path);

  char redirect_path[MAX_PATH];
  sprintf_s(redirect_path, "%s\\%s", full_path, "vrad.redirect");

  source::ScopedDll module{"shadercompile_dll.dll",
                           LOAD_WITH_ALTERED_SEARCH_PATH};
  char dll_path[MAX_PATH];

  // Look for vrad.redirect and load the dll specified in there if possible.
  auto [stream, errc] =
      se::posix::posix_file_stream_factory::open(redirect_path, "rt");
  if (errc) {
    fprintf(stderr, "%s warning: Unable to open '%s', skipping: %s.\n",
            exe_name, redirect_path, errc.message().c_str());
  } else {
    auto [string, errc2] = stream.gets(dll_path);
    if (errc2) {
      fprintf(stderr, "%s warning: Unable to read '%s', skipping: %s.\n",
              exe_name, redirect_path, errc2.message().c_str());
    } else {
      char *end{strstr(dll_path, "\n")};

      if (end) *end = '\0';

      module = source::ScopedDll{dll_path, LOAD_WITH_ALTERED_SEARCH_PATH};
      if (!module)
        fprintf(stderr, "%s warning: vrad.redirect '%s' library not found.\n",
                exe_name, dll_path);
      else
        printf("%s info: vrad.redirect '%s' library loaded.\n", exe_name,
               dll_path);
    }
  }

  // If it didn't load the module above, then use default.
  if (!module) {
    strcpy_s(dll_path, "shadercompile_dll.dll");
    module = source::ScopedDll{dll_path, LOAD_WITH_ALTERED_SEARCH_PATH};
  }

  if (module.error_code()) {
    fprintf(stderr, "%s error: unable to load %s: %s.", exe_name, dll_path,
            module.error_code().message().c_str());
    return 1;
  }

  const auto factory =
      module.GetFunction<CreateInterfaceFnT<IShaderCompileDLL>>(
          CREATEINTERFACE_PROCNAME);
  if (!factory) {
    fprintf(stderr, "%s error: can't get '%s' factory from '%s'.\n", exe_name,
            CREATEINTERFACE_PROCNAME, dll_path);
    return 2;
  }

  IShaderCompileDLL *dll{factory(SHADER_COMPILE_INTERFACE_VERSION, nullptr)};
  if (!dll) {
    fprintf(stderr, "%s error: can't get '%s' interface from '%s'.\n", exe_name,
            SHADER_COMPILE_INTERFACE_VERSION, dll_path);
    return 3;
  }

  const int rc_main{dll->main(argc, argv)};

  // Prevent tail recursion call and broken stack trace.
  exit(rc_main);
}
