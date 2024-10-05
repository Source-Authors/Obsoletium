// Copyright Valve Corporation, All rights reserved.
//
// Defines a group of app systems that all have the same lifetime that need to
// be connected/initialized, etc. in a well-defined order.

#ifndef SE_DEDICATED_DEDICATED_H_
#define SE_DEDICATED_DEDICATED_H_

#include "appframework/tier3app.h"
#include "tier0/icommandline.h"
#include "isystem.h"

// Forward declarations
class IDedicatedServerAPI;

#ifdef POSIX
#define DEDICATED_BASECLASS CTier2SteamApp
#else
#define DEDICATED_BASECLASS CVguiSteamApp
#endif

namespace se::dedicated {

/**
 * @brief Initialize, shutdown main systems, load Steam to.
 */
class DedicatedAppSystemGroup : public DEDICATED_BASECLASS {
  using BaseClass = DEDICATED_BASECLASS;

 public:
  DedicatedAppSystemGroup(ISystem *system, ICommandLine *command_line,
                          bool is_console_mode)
      : system_{system},
        command_line_{command_line},
        api_{nullptr},
        is_console_mode_{is_console_mode} {}

  bool Create() override;
  bool PreInit() override;
  int Main() override;
  void PostShutdown() override;
  void Destroy() override;

  // Used to chain to base class
  AppModule_t LoadModule(CreateInterfaceFn factory) {
    return CSteamAppSystemGroup::LoadModule(factory);
  }

  // Method to add various global singleton systems
  bool AddSystems(AppSystemInfo_t *systems) {
    return CSteamAppSystemGroup::AddSystems(systems);
  }

  void *FindSystem(const char *interface_name) {
    return CSteamAppSystemGroup::FindSystem(interface_name);
  }

  template <typename TSystem>
  TSystem *FindSystem(const char *interface_name) {
    return CSteamAppSystemGroup::FindSystem<TSystem>(interface_name);
  }

 private:
  ISystem *system_;
  ICommandLine *command_line_;
  IDedicatedServerAPI *api_;
  const bool is_console_mode_;
};

}  // namespace se::dedicated

#endif  // SE_DEDICATED_DEDICATED_H_
