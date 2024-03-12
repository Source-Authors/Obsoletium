// Copyright Valve Corporation, All rights reserved.

#ifndef SRC_LAUNCHER_BOOT_APP_SYSTEM_GROUP_H
#define SRC_LAUNCHER_BOOT_APP_SYSTEM_GROUP_H

#include <memory>

#include "appframework/IAppSystemGroup.h"
#include "tier0/icommandline.h"
#include "tier1/strtools.h"
#include "tier2/tier2.h"
#include "engine_launcher_api.h"
#include "IHammer.h"

#include "file_logger.h"
#include "resource_listing.h"

namespace src::launcher {

// Inner loop: initialize, shutdown main systems, load steam to
class BootAppSystemGroup : public CSteamAppSystemGroup {
 public:
  BootAppSystemGroup(ICommandLine *command_line,
                     const char (&base_directory)[MAX_PATH], bool is_text_mode)
      : command_line_(command_line),
        file_system_(nullptr),
        engine_api_(nullptr),
        hammer_(nullptr),
        is_edit_mode_(command_line->CheckParm("-edit")),
        is_text_mode_(is_text_mode) {
    Q_strcpy(base_dir_, base_directory);
  }

  bool Create() override;
  bool PreInit() override;
  int Main() override;
  void PostShutdown() override;
  void Destroy() override;

  bool ShouldContinueGenerateReslist();

 private:
  // Callback function from filesystem
  static void LogAccessCallback(const char *file_path, const char *options);

  const char *DetermineDefaultMod();
  const char *DetermineDefaultGame();

  char base_dir_[_MAX_PATH];
  ICommandLine *command_line_;
  IFileSystem *file_system_;
  IEngineAPI *engine_api_;
  IHammer *hammer_;
  std::unique_ptr<IResourceListing> resource_listing_;
  const bool is_edit_mode_;
  const bool is_text_mode_;

  static std::unique_ptr<FileLogger> file_logger_;
};

}  // namespace src::launcher

#endif  // SRC_LAUNCHER_BOOT_APP_SYSTEM_GROUP_H