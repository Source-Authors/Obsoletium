// Copyright Valve Corporation, All rights reserved.

#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <malloc.h>

#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "isystem.h"
#include "console/conproc.h"
#include "console/TextConsoleUnix.h"
#include "dedicated.h"
#include "idedicatedexports.h"

#include "engine_hlds_api.h"
#include "tier0/vcrmode.h"
#include "tier0/dbg.h"
#include "tier0/icommandline.h"
#include "tier1/checksum_md5.h"
#include "tier1/strtools.h"
#include "tier1/interface.h"
#include "mathlib/mathlib.h"
#include "materialsystem/imaterialsystem.h"
#include "istudiorender.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "datacache/idatacache.h"
#include "datacache/imdlcache.h"
#include "vphysics_interface.h"
#include "icvar.h"
#include "filesystem/IQueuedLoader.h"
#include "vscript/ivscript.h"

#include "scoped_app_locale.h"

extern CTextConsoleUnix console;

namespace se::dedicated {

// Loose implementation for operating system specific layer.
class UnixSystem : public ISystem {
 public:
  virtual ~UnixSystem();

  IDedicatedServerAPI *LoadModules(DedicatedAppSystemGroup *group) override;

  void ErrorMessage(int level, const char *msg) override;

  void WriteStatusText(char *szText) override;
  void UpdateStatus(int force) override;

  bool CreateConsoleWindow() override;
  void DestroyConsoleWindow() override;

  void ConsoleOutput(char *string) override;
  char *ConsoleInput(int index, char *buf, size_t buflen) override;
  void Printf(PRINTF_FORMAT_STRING const char *fmt, ...) override;
};

UnixSystem::~UnixSystem() {}

void UnixSystem::ErrorMessage(int level, const char *msg) {
  Error("%s\n", msg);
  // dimhotepus: 1 -> EOTHER.
  exit(EOTHER);
}

void UnixSystem::UpdateStatus(int force) {}

void UnixSystem::ConsoleOutput(char *string) { console.Print(string); }

void UnixSystem::Printf(PRINTF_FORMAT_STRING const char *fmt, ...) {
  // Dump text to debugging console.
  va_list argptr;
  char message[1024];

  va_start(argptr, fmt);
  V_vsprintf_safe(message, fmt, argptr);
  va_end(argptr);

  // Get Current text and append it.
  ConsoleOutput(message);
}

char *UnixSystem::ConsoleInput(int index, char *buf, size_t buflen) {
  return console.GetLine(index, buf, buflen);
}

void UnixSystem::WriteStatusText(char *szText) {}

bool UnixSystem::CreateConsoleWindow() { return true; }

void UnixSystem::DestroyConsoleWindow() {}

IDedicatedServerAPI *UnixSystem::LoadModules(
    DedicatedAppSystemGroup *pAppSystemGroup) {
  AppSystemInfo_t appSystems[] = {
      {"engine" DLL_EXT_STRING, CVAR_QUERY_INTERFACE_VERSION},
      // loaded for backwards compatability, prevents crash on exit for old game
      // dlls
      {"soundemittersystem" DLL_EXT_STRING,
       SOUNDEMITTERSYSTEM_INTERFACE_VERSION},
      {"materialsystem" DLL_EXT_STRING, MATERIAL_SYSTEM_INTERFACE_VERSION},
      {"studiorender" DLL_EXT_STRING, STUDIO_RENDER_INTERFACE_VERSION},
      {"vphysics" DLL_EXT_STRING, VPHYSICS_INTERFACE_VERSION},
      {"datacache" DLL_EXT_STRING, DATACACHE_INTERFACE_VERSION},
      {"datacache" DLL_EXT_STRING, MDLCACHE_INTERFACE_VERSION},
      {"datacache" DLL_EXT_STRING, STUDIO_DATA_CACHE_INTERFACE_VERSION},
      {"dedicated" DLL_EXT_STRING, QUEUEDLOADER_INTERFACE_VERSION},
      {"engine" DLL_EXT_STRING, VENGINE_HLDS_API_VERSION},
      {"vscript" DLL_EXT_STRING, VSCRIPT_INTERFACE_VERSION},
      {"", ""}  // Required to terminate the list
  };

  if (!pAppSystemGroup->AddSystems(appSystems)) return false;

  auto *api = pAppSystemGroup->FindSystem<IDedicatedServerAPI>(
      VENGINE_HLDS_API_VERSION);

  auto *material_system = pAppSystemGroup->FindSystem<IMaterialSystem>(
      MATERIAL_SYSTEM_INTERFACE_VERSION);
  material_system->SetShaderAPI("shaderapiempty" DLL_EXT_STRING);

  return api;
}

int BootMain(int argc, char **argv, bool is_console_mode, ISystem *system);

}  // namespace se::dedicated

/**
 * @brief Entry point.
 * @param argc Arguments count.
 * @param argv Arguments.
 * @return Exit code.
 */
DLL_EXPORT int DedicatedMain(int argc, char *argv[]) {
  // Printf/sscanf functions expect en_US UTF8 localization.  Mac OSX also sets
  // LANG to en_US.UTF-8 before starting up (in info.plist I believe).
  //
  // Starting in Windows 10 version 1803 (10.0.17134.0), the Universal C Runtime
  // supports using a UTF-8 code page.
  //
  // Need to double check that localization for libcef is handled correctly when
  // slam things to en_US.UTF-8.
  constexpr char kEnUsUtf8Locale[]{"en_US.UTF-8"};

  const se::ScopedAppLocale scoped_app_locale{kEnUsUtf8Locale};
  if (Q_stricmp(se::ScopedAppLocale::GetCurrentLocale(), kEnUsUtf8Locale)) {
    Warning("setlocale('%s') failed, current locale is '%s'.\n",
            kEnUsUtf8Locale, se::ScopedAppLocale::GetCurrentLocale());
  }

  se::dedicated::UnixSystem system;
  return se::dedicated::BootMain(argc, argv, &system);
}
