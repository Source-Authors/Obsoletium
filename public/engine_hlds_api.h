// Copyright Valve Corporation, All rights reserved.
//
// Interface exported by the engine to allow a dedicated server front end
// application to host it.

#ifndef SE_ENGINE_HLDS_API_H_
#define SE_ENGINE_HLDS_API_H_

#include "tier1/interface.h"
#include "appframework/IAppSystem.h"
#include "appframework/IAppSystemGroup.h"

struct ModInfo_t {
  void *m_pInstance;
  // Executable directory (ex. "c:/program files/half-life 2").
  const char *m_pBaseDirectory;
  // Mod name (ex. "cstrike")
  const char *m_pInitialMod;
  // Root game name ("hl2", for example, in the case of cstrike)
  const char *m_pInitialGame;
  CAppSystemGroup *m_pParentAppSystemGroup;
  bool m_bTextMode;
};

// Interface exported by the engine to allow a dedicated server front end
// application to host it.
abstract_class IDedicatedServerAPI : public IAppSystem {
 public:
  // Initialize the engine with the specified base directory and interface
  // factories.
  virtual bool ModInit(ModInfo_t & info) = 0;

  // Shutdown the engine.
  virtual void ModShutdown() = 0;

  // Run a frame
  virtual bool RunFrame() = 0;

  // Insert text into console
  virtual void AddConsoleText(char *text) = 0;

  // Get current status to display in the dedicated UI (console window title
  // bar, e.g.)
  virtual void UpdateStatus(float *fps, int *active_players_count,
                            int *max_players_count, char *map_name,
                            int map_size) = 0;

  // Get current Hostname to display in the hlds UI (console window title bar,
  // e.g.)
  virtual void UpdateHostname(char *hostn_name, int host_size) = 0;
};

constexpr inline char VENGINE_HLDS_API_VERSION[]{"VENGINE_HLDS_API_VERSION002"};

#endif  // !SE_ENGINE_HLDS_API_H_
