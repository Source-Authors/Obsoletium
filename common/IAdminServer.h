// Copyright Valve Corporation, All rights reserved.
//
// Interface to server administration functions

#ifndef SE_IADMINSERVER_H_
#define SE_IADMINSERVER_H_

#include "tier1/interface.h"

// handle to a game window
using ManageServerUIHandle_t = uintp;
class IManageServer;

// Interface to server administration functions
abstract_class IAdminServer : public IBaseInterface {
 public:
  // opens a manage server dialog for a local server
  virtual ManageServerUIHandle_t OpenManageServerDialog(
      const char *serverName, const char *gameDir) = 0;

  // opens a manage server dialog to a remote server
  virtual ManageServerUIHandle_t OpenManageServerDialog(
      unsigned int gameIP, unsigned int gamePort, const char *password) = 0;

  // forces the game info dialog closed
  virtual void CloseManageServerDialog(ManageServerUIHandle_t gameDialog) = 0;

  // Gets a handle to the interface
  virtual IManageServer *GetManageServerInterface(
      ManageServerUIHandle_t handle) = 0;
};

constexpr inline char ADMINSERVER_INTERFACE_VERSION[]{"AdminServer002"};

#endif  // !SE_IADMINSERVER_H_
