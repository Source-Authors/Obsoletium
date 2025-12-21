// Copyright Valve Corporation, All rights reserved.

#ifdef _WIN32
#include "winlite.h"
#elif POSIX
#include <unistd.h>
#endif

#include <tuple>

#include "isystem.h"
#include "dedicated.h"
#include "engine_hlds_api.h"
#include "filesystem.h"
#include "tier0/dbg.h"
#include "tier0/vcrmode.h"
#include "tier0/icommandline.h"
#include "tier1/strtools.h"
#include "idedicatedexports.h"
#include "vgui/vguihelpers.h"

namespace {

CSysModule *s_hMatSystemModule = nullptr;
CSysModule *s_hEngineModule = nullptr;
CSysModule *s_hSoundEmitterModule = nullptr;

CreateInterfaceFn s_MaterialSystemFactory;
CreateInterfaceFn s_EngineFactory;
CreateInterfaceFn s_SoundEmitterFactory;

}  // namespace

namespace se::dedicated {

int ProcessConsoleInput(ISystem *system, IDedicatedServerAPI *api) {
  int count = 0;

  if (api) {
    char buffer[256];
    char *s;
    do {
      s = system->ConsoleInput(count++, buffer, sizeof(buffer));

      if (s && s[0]) {
        V_strcat_safe(buffer, "\n");

        api->AddConsoleText(buffer);
      }
    } while (s);
  }

  return count;
}

void RunServer(ISystem *system, IDedicatedServerAPI *api, bool is_console_mode);

class DedicatedExports : public CBaseAppSystem<IDedicatedExports> {
 public:
  void Sys_Printf(char *text) override {
    Assert(system_);
    system_->Printf("%s", text);
  }

  void RunServer() override {
    Assert(system_);
    Assert(api_);
    se::dedicated::RunServer(system_, api_, is_console_mode_);
  }

  void Startup(void *ctx) override {
    std::tie(system_, api_, is_console_mode_) =
        *static_cast<std::tuple<ISystem *, IDedicatedServerAPI *, bool> *>(ctx);
    Assert(system_);
    Assert(api_);
  }

 private:
  ISystem *system_;
  IDedicatedServerAPI *api_;
  bool is_console_mode_;
};

EXPOSE_SINGLE_INTERFACE(DedicatedExports, IDedicatedExports,
                        VENGINE_DEDICATEDEXPORTS_API_VERSION);

const char *get_consolelog_filename(ICommandLine *command_line) {
  static bool is_initialized{false};
  static char console_log[MAX_PATH];

  if (!is_initialized) {
    is_initialized = true;

    // Don't do the -consolelog thing if -consoledebug is present.
    //  CTextConsoleUnix::Print() looks for -consoledebug.
    const char *file_name = nullptr;
    if (!command_line->FindParm("-consoledebug") &&
        command_line->CheckParm("-consolelog", &file_name) && file_name) {
      V_strcpy_safe(console_log, file_name);
    }
  }

  return console_log;
}

template <size_t out_size>
const char *PrefixMessageGroup(char (&out)[out_size], const char *group,
                               const char *message) {
  const char *out_group{GetSpewOutputGroup()};

  out_group = !Q_isempty(out_group) ? out_group : group;

  const size_t length{strlen(message)};
  if (length > 1 && message[length - 1] == '\n') {
    V_sprintf_safe(out, "[%.3f][%s] %s", Plat_FloatTime(), out_group, message);
  } else {
    V_sprintf_safe(out, "%s", message);
  }

  return out;
}

extern ISystem *g_system;
extern bool g_is_console_mode;

SpewRetval_t DedicatedSpewOutputFunc(SpewType_t spewType, char const *pMsg) {
  char message[4096];
  PrefixMessageGroup(message, "swds", pMsg);

  if (g_system) {
    g_system->Printf("%s", message);

    // If they have specified -consolelog, log this message there.  Otherwise
    // these wind up being lost because Sys_InitGame hasn't been called yet, and
    // Sys_SpewFunc is the thing that logs stuff to -consolelog, etc.
    const char *filename = get_consolelog_filename(CommandLine());
    if (!Q_isempty(filename)) {
      FileHandle_t fh = g_pFullFileSystem->Open(filename, "a");
      if (fh) {
        RunCodeAtScopeExit(g_pFullFileSystem->Close(fh));

        g_pFullFileSystem->Write(message, V_strlen(message), fh);
      }
    }
  }

#ifdef _WIN32
  Plat_DebugString(message);
#endif

  if (spewType == SPEW_ERROR) {
#ifdef _WIN32
    // In Windows vgui mode, make a message box or they won't ever see the
    // error.
    if (!g_is_console_mode) {
      MessageBoxA(nullptr, message, "SRCDS - Fatal Error",
                  MB_OK | MB_TASKMODAL | MB_ICONERROR);
    }
    // dimhotepus: 1 -> ENOTRECOVERABLE
    ::TerminateProcess(::GetCurrentProcess(), ENOTRECOVERABLE);
#elif POSIX
    fflush(stdout);
    // dimhotepus: Try to run destructors! 1 -> ENOTRECOVERABLE
    exit(ENOTRECOVERABLE);
#endif

    return SPEW_ABORT;
  }

  if (spewType == SPEW_ASSERT) {
    return CommandLine()->FindParm("-noassert") == 0 ? SPEW_DEBUGGER
                                                     : SPEW_CONTINUE;
  }

  return SPEW_CONTINUE;
}

}  // namespace se::dedicated
