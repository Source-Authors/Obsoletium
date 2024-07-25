// Copyright Valve Corporation, All rights reserved.

#ifndef SRC_LAUNCHER_VCR_HELPERS_H
#define SRC_LAUNCHER_VCR_HELPERS_H

#include <tuple>

#include "tier0/icommandline.h"
#include "tier0/vcrmode.h"

namespace se::launcher {

// Implementation of VCRHelpers.
struct VCRHelpers : public IVCRHelpers {
  void ErrorMessage(const char *message) override;
  void *GetMainWindow() override;
};

[[nodiscard]] std::tuple<VCRHelpers, int> CreateVcrHelpers(
    ICommandLine *command_line);

}  // namespace se::launcher

#endif  // SRC_LAUNCHER_VCR_HELPERS_H
