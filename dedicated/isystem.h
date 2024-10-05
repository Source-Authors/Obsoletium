// Copyright Valve Corporation, All rights reserved.

#ifndef SE_DEDICATED_ISYSTEM_H_
#define SE_DEDICATED_ISYSTEM_H_

#include "tier0/platform.h"
#include "tier1/interface.h"

class IDedicatedServerAPI;

namespace se::dedicated {

/**
 * @brief Low-level operating system abstractions.
 */
abstract_class ISystem {
 public:
  virtual ~ISystem() {}

  /**
   * @brief Load modules for group.
   */
  virtual IDedicatedServerAPI *LoadModules(class DedicatedAppSystemGroup *
                                           group) = 0;

  /**
   * @brief Dumps error message and exit.
   * @param level Error level.
   * @param message Error message.
   */
  virtual void ErrorMessage(int level, const char *message) = 0;

  virtual void WriteStatusText(char *status) = 0;
  virtual void UpdateStatus(int force) = 0;

  virtual bool CreateConsoleWindow() = 0;
  virtual void DestroyConsoleWindow() = 0;

  virtual void ConsoleOutput(char *message) = 0;
  virtual char *ConsoleInput(int index, char *buffer, size_t size) = 0;
  virtual void Printf(PRINTF_FORMAT_STRING const char *fmt, ...)
      FMTFUNCTION(2, 3) = 0;
};

}  // namespace se::dedicated

#endif  // SE_DEDICATED_ISYSTEM_H_