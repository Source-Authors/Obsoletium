// Copyright Valve Corporation, All rights reserved.

#ifndef SRC_LAUNCHER_FILE_LOGGER_H
#define SRC_LAUNCHER_FILE_LOGGER_H

#include "tier0/icommandline.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlstring.h"
#include "filesystem.h"

namespace se::launcher {

// create file to dump out to
class FileLogger {
 public:
  FileLogger(ICommandLine *command_line, IFileSystem *file_system,
             char (&base_directory)[MAX_PATH]);

  void Shutdown();
  void LogAccess(const char *file_path, const char *options);

 private:
  void Init();
  void LogAllResources(const char *line);

  char current_dir_[_MAX_PATH];
  char base_dir_[_MAX_PATH];
  ICommandLine *command_line_;
  IFileSystem *file_system_;

  FileHandle_t all_logs_file_;
  bool is_active_;

  // persistent across restarts
  CUtlRBTree<CUtlString, int> logged_tree_;
  CUtlString resource_listing_dir_;
  CUtlString full_game_path_;
};

}  // namespace se::launcher

#endif  // SRC_LAUNCHER_FILE_LOGGER_H
