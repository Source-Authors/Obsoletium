// Copyright Valve Corporation, All rights reserved.
//
// Valve Radiosity launcher.

#include "stdafx.h"

#include <filesystem>
#include <system_error>

#include "posix_file_stream.h"
#include "scoped_dll.h"

#include "tier0/icommandline.h"
#include "tier1/strtools.h"

#include "ivraddll.h"

#include "scoped_app_locale.h"

#include "tier0/memdbgon.h"

namespace {

template <intp size>
[[nodiscard]] std::error_code MakeFullPath(const char *in, char (&out)[size]) {
  if (in[0] == CORRECT_PATH_SEPARATOR || in[0] == INCORRECT_PATH_SEPARATOR ||
      in[1] == ':') {
    // It's already a full path.
    V_strcpy_safe(out, in);
  } else {
    try {
      std::error_code rc;
      std::filesystem::path cwd{std::filesystem::current_path(rc)};

      if (rc) return rc;

      Q_WStringToUTF8(cwd.c_str(), out, size * sizeof(out[0]));
    } catch (const std::bad_alloc &) {
      return std::error_code{ENOMEM, std::generic_category()};
    }

    V_strcat_safe(out, CORRECT_PATH_SEPARATOR_S);
    V_strcat_safe(out, in);
  }

  return std::error_code{};
}

[[nodiscard]] int GetBothArgIndex(int argc, char *argv[]) {
  int both_arg{0};
  for (int arg{1}; arg < argc; arg++) {
    if (V_stricmp(argv[arg], "-both") == 0) {
      both_arg = arg;
    }
  }
  return both_arg;
}

}  // namespace

int main(int argc, char *argv[]) {
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

  // Need command line first.
  CommandLine()->CreateCmdLine(argc, argv);

  // Check whether they used the -both switch.  If this is specified, vrad will
  // be run twice, once with -hdr and once without.
  const int both_arg{GetBothArgIndex(argc, argv)};

  constexpr char kRedirectFileName[]{"vrad.redirect"};
  constexpr char kModuleName[]{"vrad_dll" DLL_EXT_STRING};

  char full_path[MAX_FILEPATH], redirect_path[MAX_FILEPATH];
  std::error_code err{MakeFullPath(argv[0], full_path)};
  if (err) {
    fprintf(stderr, "vrad err: Can't get full path to '%s'.\n", argv[0]);
    return err.value();
  }

  V_StripFilename(full_path);
  V_sprintf_safe(redirect_path, "%s" CORRECT_PATH_SEPARATOR_S "%s", full_path,
                 kRedirectFileName);

  source::ScopedDll module{nullptr, 0};
  char module_name[MAX_FILEPATH];

  // First, look for vrad.redirect and load the dll specified in there if
  // possible.
  if (auto [f, err2] =
          se::posix::posix_file_stream_factory::open(redirect_path, "rt");
      !err2) {
    char *name{nullptr};
    if (std::tie(name, err2) = f.gets(module_name); !err2) {
      char *end{strchr(name, '\n')};
      if (end) *end = '\0';

      module = source::ScopedDll{name, 0};
      if (!module) {
        fprintf(stderr, "vrad warn: Can't find '%s' specified in '%s'.\n", name,
                kRedirectFileName);
        return module.error_code().value();
      } else {
        printf("vrad: Loaded alternate vrad module '%s' specified in '%s'.\n",
               name, kRedirectFileName);
      }
    }
  }

  int rc{0};

  // Try ldr and hdr.
  for (int mode = 0; mode < 2; mode++) {
    if (mode && !both_arg) continue;

    // If it didn't load the module above, then use the
    if (!module) {
      V_strcpy_safe(module_name, kModuleName);

      module = source::ScopedDll{module_name, 0};
    }

    if (!module) {
      fprintf(stderr, "vrad error: Can't load '%s'.\n%s", module_name,
              std::system_category().message(::GetLastError()).c_str());
      return ENOENT;
    }

    const auto kFactoryName = CREATEINTERFACE_PROCNAME;
    CreateInterfaceFnT<IVRadDLL> factory{nullptr};

    std::tie(factory, err) =
        module.GetFunction<CreateInterfaceFnT<IVRadDLL>>(kFactoryName);
    if (!factory) {
      fprintf(stderr, "vrad error: Can't get '%s' factory from '%s'.\n",
              kFactoryName, kModuleName);
      return EINVAL;
    }

    int ret{0};
    const auto kInterfaceName = VRAD_INTERFACE_VERSION;

    IVRadDLL *vrad{factory(kInterfaceName, &ret)};
    if (!vrad) {
      fprintf(stderr, "vrad error: Can't get '%s' interface from '%s'.\n",
              kModuleName, kInterfaceName);
      return EINVAL;
    }

    // Replace -both with -hdr or -ldr.
    if (both_arg) V_strcpy(argv[both_arg], mode ? "-hdr" : "-ldr");

    rc = vrad->main(argc, argv);

    // VRAD failed, return first failure code.
    if (rc) return rc;
  }

  return rc;
}
