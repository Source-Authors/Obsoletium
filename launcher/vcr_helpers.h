// Copyright Valve Corporation, All rights reserved.

#ifndef SE_LAUNCHER_VCR_HELPERS_H
#define SE_LAUNCHER_VCR_HELPERS_H

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
    const ICommandLine *command_line);

}  // namespace se::launcher

#endif  // SE_LAUNCHER_VCR_HELPERS_H
