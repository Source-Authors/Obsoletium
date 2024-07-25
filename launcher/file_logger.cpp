// Copyright Valve Corporation, All rights reserved.

#include "file_logger.h"

#include "filesystem.h"
#include "resource_listing.h"

#include "tier1/fmtstr.h"

#ifdef _WIN32
#include "winlite.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace {

constexpr inline char kAllResourceListingName[]{"all.lst"};
constexpr inline char kEngineResourceListingName[]{"engine.lst"};

bool FilePathLess(CUtlString const &l, CUtlString const &r) {
  return CaselessStringLessThan(l.Get(), r.Get());
}

}  // namespace

namespace se::launcher {

FileLogger::FileLogger(ICommandLine *command_line, IFileSystem *file_system,
                       char (&base_directory)[MAX_PATH])
    : command_line_{command_line},
      file_system_{file_system},
      all_logs_file_(FILESYSTEM_INVALID_HANDLE),
      is_active_(false),
      logged_tree_(0, 0, FilePathLess) {
  MEM_ALLOC_CREDIT();

  base_dir_[0] = '\0';
  V_strcpy_safe(base_dir_, base_directory);

  current_dir_[0] = '\0';
  resource_listing_dir_ = "reslists";

  Init();
}

void FileLogger::Init() {
  // Can't do this in edit mode
  if (command_line_->CheckParm("-edit") ||
      !command_line_->CheckParm("-makereslists")) {
    return;
  }

  is_active_ = true;

  const char *resource_listing_dir{nullptr};
  if (command_line_->CheckParm("-reslistdir", &resource_listing_dir) &&
      resource_listing_dir) {
    char override_dir[MAX_PATH];
    V_strcpy_safe(override_dir, resource_listing_dir);
    Q_StripTrailingSlash(override_dir);

#ifdef WIN32
    Q_strlower(override_dir);
#endif

    Q_FixSlashes(override_dir);

    if (!Q_isempty(override_dir)) {
      resource_listing_dir_ = override_dir;
    }
  }

  // game directory has not been established yet, must derive ourselves
  char path[MAX_PATH];
  Q_snprintf(path, sizeof(path), "%s/%s", base_dir_,
             command_line_->ParmValue("-game", "hl2"));
  Q_FixSlashes(path);

#ifdef WIN32
  Q_strlower(path);
#endif

  full_game_path_ = path;

  // create file to dump out to
  char directory[MAX_PATH];
  V_snprintf(directory, sizeof(directory), "%s\\%s", full_game_path_.String(),
             resource_listing_dir_.String());

  file_system_->CreateDirHierarchy(directory, "GAME");

  if (!command_line_->FindParm("-startmap") &&
      !command_line_->FindParm("-startstage")) {
    logged_tree_.RemoveAll();

    file_system_->RemoveFile(
        CFmtStr("%s\\%s\\%s", full_game_path_.String(),
                resource_listing_dir_.String(), kAllResourceListingName),
        "GAME");
  }

#ifdef WIN32
  ::GetCurrentDirectory(sizeof(current_dir_), current_dir_);
  V_strcat_safe(current_dir_, "\\");

  _strlwr(current_dir_);
#else
  getcwd(current_dir_, sizeof(current_dir_));
  V_strcat_safe(current_dir_, "/");
#endif

  // Open for append
  all_logs_file_ = file_system_->Open(
      CFmtStr("%s\\%s\\%s", full_game_path_.String(),
              resource_listing_dir_.String(), kAllResourceListingName),
      "at", "GAME");
}

void FileLogger::Shutdown() {
  if (!is_active_) return;

  is_active_ = false;

  // Now load and sort all.lst
  SortResourceListing(
      file_system_,
      CFmtStr("%s\\%s\\%s", full_game_path_.String(),
              resource_listing_dir_.String(), kAllResourceListingName),
      "GAME");

  // Now load and sort engine.lst
  SortResourceListing(
      file_system_,
      CFmtStr("%s\\%s\\%s", full_game_path_.String(),
              resource_listing_dir_.String(), kEngineResourceListingName),
      "GAME");

  logged_tree_.Purge();

  if (all_logs_file_ != FILESYSTEM_INVALID_HANDLE) {
    file_system_->Close(all_logs_file_);
    all_logs_file_ = FILESYSTEM_INVALID_HANDLE;
  }
}

void FileLogger::LogAllResources(const char *line) {
  if (all_logs_file_ != FILESYSTEM_INVALID_HANDLE) {
    file_system_->Write("\"", 1, all_logs_file_);
    file_system_->Write(line, Q_strlen(line), all_logs_file_);
    file_system_->Write("\"\n", 2, all_logs_file_);
  }
}

void FileLogger::LogAccess(const char *file_path, const char *options) {
  if (!is_active_) return;

  // write out to log file
  Assert(file_path[1] == ':');

  const auto idx = logged_tree_.Find(file_path);
  if (idx != logged_tree_.InvalidIndex()) return;

  logged_tree_.Insert(file_path);

  // make it relative to our root directory
  const char *relative{Q_stristr(file_path, base_dir_)};
  if (relative) {
    relative += Q_strlen(base_dir_) + 1;

    char rel[MAX_PATH];
    V_strcpy_safe(rel, relative);

#ifdef WIN32
    Q_strlower(rel);
#endif

    Q_FixSlashes(rel);

    LogAllResources(rel);
  }
}

}  // namespace se::launcher
