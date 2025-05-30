// Copyright Valve Corporation, All rights reserved.
//
// Command-line tool used to compile shaders in the Source engine.
//
// See
// https://developer.valvesoftware.com/wiki/Shader_authoring/Compiling_Shaders

#include <filesystem>
#include <system_error>

#include "tier1/interface.h"
#include "posix_file_stream.h"
#include "scoped_dll.h"

#include "tier0/icommandline.h"
#include "tier1/strtools.h"
#include "ishadercompiledll.h"

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

}  // namespace

int main(int argc, char *argv[]) {
  // Need command line first.
  CommandLine()->CreateCmdLine(argc, argv);

  constexpr char kRedirectFileName[]{"shadercompile.redirect"};
  constexpr char kModuleName[]{"shadercompile_dll" DLL_EXT_STRING};

  char full_path[MAX_FILEPATH], redirect_path[MAX_FILEPATH];
  std::error_code err{MakeFullPath(argv[0], full_path)};
  if (err) {
    fprintf(stderr, "shadercompile err: Can't get full path to '%s'.\n",
            argv[0]);
    return err.value();
  }

  V_StripFilename(full_path);
  V_sprintf_safe(redirect_path, "%s" CORRECT_PATH_SEPARATOR_S "%s", full_path,
                 kRedirectFileName);

  source::ScopedDll module{nullptr, 0};
  char module_name[MAX_FILEPATH];

  // First, look for shadercompile.redirect and load the dll specified in there
  // if possible.
  if (auto [f, err2] =
          se::posix::posix_file_stream_factory::open(redirect_path, "rt");
      !err2) {
    char *name{nullptr};
    if (std::tie(name, err2) = f.gets(module_name); !err2) {
      char *end{strchr(name, '\n')};
      if (end) *end = '\0';

      module = source::ScopedDll{name, 0};
      if (!module) {
        fprintf(stderr,
                "shadercompile warn: Can't find '%s' specified in '%s'.\n",
                name, kRedirectFileName);
        return module.error_code().value();
      } else {
        printf(
            "shadercompile: Loaded alternate shadercompile module '%s' "
            "specified in '%s'.\n",
            name, kRedirectFileName);
      }
    }
  }

  // If it didn't load the module above, then use the
  if (!module) {
    V_strcpy_safe(module_name, kModuleName);

    module = source::ScopedDll{module_name, 0};
  }

  if (!module) {
    fprintf(stderr, "shadercompile error: Can't load '%s'.\n%s", module_name,
            std::system_category().message(::GetLastError()).c_str());
    return ENOENT;
  }

  const auto kFactoryName = CREATEINTERFACE_PROCNAME;
  CreateInterfaceFnT<IShaderCompileDLL> factory{nullptr};

  std::tie(factory, err) =
      module.GetFunction<CreateInterfaceFnT<IShaderCompileDLL>>(kFactoryName);
  if (!factory) {
    fprintf(stderr, "shadercompile error: Can't get '%s' factory from '%s'.\n",
            kFactoryName, kModuleName);
    return EINVAL;
  }

  int ret{0};
  const auto kInterfaceName = SHADER_COMPILE_INTERFACE_VERSION;

  IShaderCompileDLL *shader_compile{factory(kInterfaceName, &ret)};
  if (!shader_compile) {
    fprintf(stderr,
            "shadercompile error: Can't get '%s' interface from '%s'.\n",
            kModuleName, kInterfaceName);
    return EINVAL;
  }

  return shader_compile->main(argc, argv);
}
