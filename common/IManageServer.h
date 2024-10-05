// Copyright Valve Corporation, All rights reserved.
//
// Basic callback interface for the manage server list, to update status text et
// al.

#ifndef SE_IMANAGESERVER_H_
#define SE_IMANAGESERVER_H_

#include "tier1/interface.h"

// Basic callback interface for the manage server list, to update status text et
// al.
abstract_class IManageServer : public IBaseInterface {
 public:
  // activates the manage page
  virtual void ShowPage() = 0;

  // sets whether or not the server is remote
  virtual void SetAsRemoteServer(bool remote) = 0;

  // prints text to the console
  virtual void AddToConsole(const char *msg) = 0;
};

constexpr inline char IMANAGESERVER_INTERFACE_VERSION[]{"IManageServer002"};

#endif  // !SE_IMANAGESERVER_H_
