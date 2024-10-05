// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SE_IDEDICATEDEXPORTS_H_
#define SE_IDEDICATEDEXPORTS_H_

#include "tier1/interface.h"
#include "appframework/IAppSystem.h"

abstract_class IDedicatedExports : public IAppSystem {
 public:
  virtual void Sys_Printf(char *text) = 0;
  virtual void RunServer() = 0;
  virtual void Startup(void *ctx) = 0;
};

constexpr inline char VENGINE_DEDICATEDEXPORTS_API_VERSION[]{
    "VENGINE_DEDICATEDEXPORTS_API_VERSION004"};

#endif  // SE_IDEDICATEDEXPORTS_H_
