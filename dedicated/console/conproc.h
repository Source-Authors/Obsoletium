// Copyright Valve Corporation, All rights reserved.
//
// Support for external server monitoring programs.

#ifndef SE_DEDICATED_CONSOLE_CONPROC_H_
#define SE_DEDICATED_CONSOLE_CONPROC_H_

#include "tier0/icommandline.h"

#include "isystem.h"

/**
 * @brief Console commands.
 */
enum class ConsoleCommand : int {
  // Param1 : Text
  CCOM_WRITE_TEXT = 0x2,
  // Param1 : Begin line
  // Param2 : End line
  CCOM_GET_TEXT = 0x3,
  // No params
  CCOM_GET_SCR_LINES = 0x4,
  // Param1 : Number of lines
  CCOM_SET_SCR_LINES = 0x5,
};

namespace se::dedicated {

bool StartupRemoteConsole(ICommandLine *command_line, ISystem *system);
void ShutdownRemoteConsole();

}  // namespace se::dedicated

#endif  // !SE_DEDICATED_CONSOLE_CONPROC_H_