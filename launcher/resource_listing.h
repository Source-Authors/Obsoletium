// Copyright Valve Corporation, All rights reserved.

#ifndef SRC_LAUNCHER_RESOURCE_LISTING_H
#define SRC_LAUNCHER_RESOURCE_LISTING_H

#include <memory>
#include "tier0/icommandline.h"
#include "tier2/tier2.h"

namespace se::launcher {

struct IResourceListing {
  virtual ~IResourceListing() = 0;

  virtual void Init(const char *base_directory, const char *game_directory) = 0;
  virtual bool IsActive() = 0;

  virtual void SetupCommandLine() = 0;

  virtual bool ShouldContinue() = 0;
};

std::unique_ptr<IResourceListing> CreateResourceListing(
    ICommandLine *command_line, IFileSystem *file_system);

void SortResourceListing(IFileSystem *file_system, const char *file_name,
                         const char *search_path);

}  // namespace se::launcher

#endif  // SRC_LAUNCHER_RESOURCE_LISTING_H
