// Copyright Valve Corporation, All rights reserved.

#include "vcr_helpers.h"

#include "tier0/platform.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace s1::launcher {

void VCRHelpers::ErrorMessage(const char* message) {
  NOVCR(Plat_MessageBox(V_STRINGIFY(VPCGAME) " - VCR Error",
                        "VCR Error occurred", message,
                        Plat_MessageBoxButton::Ok, Plat_MessageBoxIcon::Error));
}

void* VCRHelpers::GetMainWindow() { return nullptr; }

std::tuple<VCRHelpers, int> CreateVcrHelpers(const ICommandLine* command_line) {
  const char* vcr_file_path;
  VCRHelpers vcr_helpers;

  // Start VCR mode?
  if (command_line->CheckParm("-vcrrecord", &vcr_file_path)) {
    if (!VCRStart(vcr_file_path, true, &vcr_helpers)) {
      Warning("-vcrrecord: can't open '%s' for writing.\n", vcr_file_path);
      return {vcr_helpers, errno ? errno : -1};
    }
  } else if (command_line->CheckParm("-vcrplayback", &vcr_file_path)) {
    if (!VCRStart(vcr_file_path, false, &vcr_helpers)) {
      Warning("-vcrplayback: can't open '%s' for reading.\n", vcr_file_path);
      return {vcr_helpers, errno ? errno : -1};
    }
  }

  return {vcr_helpers, 0};
}

}  // namespace s1::launcher
