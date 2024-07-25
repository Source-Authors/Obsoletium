// Copyright Valve Corporation, All rights reserved.

#include "resource_listing.h"

#include "filesystem.h"
#include "tier1/utlrbtree.h"
#include "tier1/fmtstr.h"
#include "characterset.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"
#include "tier1/utlbuffer.h"
#include "tier0/icommandline.h"
#include "tier1/KeyValues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace {

struct WorkItem {
  CUtlString m_sSubDir;
  CUtlString m_sAddCommands;
};

bool ReslistLogLessFunc(CUtlString const &pLHS, CUtlString const &pRHS) {
  return CaselessStringLessThan(pLHS.Get(), pRHS.Get());
}

bool SaveResourceListing(IFileSystem *file_system,
                         const CUtlRBTree<CUtlString, int> &list,
                         const char *pchFileName, const char *pchSearchPath) {
  FileHandle_t fh = file_system->Open(pchFileName, "wt", pchSearchPath);
  if (fh != FILESYSTEM_INVALID_HANDLE) {
    for (int i = list.FirstInorder(); i != list.InvalidIndex();
         i = list.NextInorder(i)) {
      file_system->Write(list[i].String(), Q_strlen(list[i].String()), fh);
      file_system->Write("\n", 1, fh);
    }

    file_system->Close(fh);
    return true;
  }
  return false;
}

void LoadResourceListing(IFileSystem *file_system,
                         CUtlRBTree<CUtlString, int> &list,
                         const char *pchFileName, const char *pchSearchPath) {
  CUtlBuffer buffer((intp)0, 0, CUtlBuffer::TEXT_BUFFER);
  if (!file_system->ReadFile(pchFileName, pchSearchPath, buffer)) {
    // does not exist
    return;
  }

  characterset_t breakSet;
  CharacterSetBuild(&breakSet, "");

  // parse reslist
  char szToken[MAX_PATH];
  for (;;) {
    intp nTokenSize = buffer.ParseToken(&breakSet, szToken, sizeof(szToken));
    if (nTokenSize <= 0) {
      break;
    }

    Q_strlower(szToken);
    Q_FixSlashes(szToken);

    // Ensure filename has "quotes" around it
    CUtlString s;
    if (szToken[0] == '\"') {
      Assert(Q_strlen(szToken) > 2);
      Assert(szToken[Q_strlen(szToken) - 1] == '\"');
      s = szToken;
    } else {
      s = CFmtStr("\"%s\"", szToken);
    }

    int idx = list.Find(s);
    if (idx == list.InvalidIndex()) {
      list.Insert(s);
    }
  }
}

void MergeResLists(IFileSystem *file_system, CUtlVector<CUtlString> &fileNames,
                   const char *pchOutputFile, const char *pchSearchPath) {
  CUtlRBTree<CUtlString, int> sorted(0, 0, ReslistLogLessFunc);
  for (auto &fn : fileNames) {
    LoadResourceListing(file_system, sorted, fn.String(),
                        pchSearchPath);
  }

  // Now write it back out
  SaveResourceListing(file_system, sorted, pchOutputFile, pchSearchPath);
}

}  // namespace

namespace se::launcher {

enum class State { BuildingListing = 0, GeneratingCaches, Done };

class ResourceListing : public IResourceListing {
 public:
  ResourceListing(ICommandLine *command_line, IFileSystem *file_system);
  virtual ~ResourceListing() = default;

  void Init(const char *pchBaseDir, const char *pchGameDir) override;
  bool IsActive() override;
  void Collate();

  void SetupCommandLine() override;
  bool ShouldContinue() override;

 private:
  bool InitCommandFile(const char *pchGameDir, const char *pchCommandFile);
  void LoadMapList(const char *pchGameDir, CUtlVector<CUtlString> &vecMaps,
                   const char *pchMapFile);
  void CollateFiles(const char *pchResListFilename);

  ICommandLine *command_line_;
  IFileSystem *file_system_;

  CUtlString base_dir_;
  CUtlString game_dir_;
  CUtlString full_game_path_;

  CUtlString final_dir_;
  CUtlString working_dir_;
  CUtlString base_command_line_;
  CUtlString original_command_line_;
  CUtlString initial_start_map_;

  intp current_wi_;
  CUtlVector<WorkItem> work_items_;

  CUtlVector<CUtlString> map_list_;
  State current_state_;
  bool is_initialized_;
  bool is_active_;
};

std::unique_ptr<IResourceListing> CreateResourceListing(
    ICommandLine *command_line, IFileSystem *file_system) {
  return std::make_unique<ResourceListing>(command_line, file_system);
}

IResourceListing::~IResourceListing() = default;

ResourceListing::ResourceListing(ICommandLine *command_line,
                                 IFileSystem *file_system)
    : command_line_{command_line},
      file_system_{file_system},
      final_dir_{"reslists"},
      working_dir_{"reslists_work"},
      current_wi_(0),
      current_state_(State::BuildingListing),
      is_initialized_(false),
      is_active_(false) {
  MEM_ALLOC_CREDIT();
}

void ResourceListing::CollateFiles(const char *pchResListFilename) {
  CUtlVector<CUtlString> vecReslists{(intp)0, work_items_.Count()};
  char fn[MAX_PATH];

  for (auto &wi : work_items_) {
    Q_snprintf(fn, sizeof(fn), "%s\\%s\\%s\\%s", full_game_path_.String(),
               working_dir_.String(), wi.m_sSubDir.String(),
               pchResListFilename);

    vecReslists.AddToTail(fn);
  }

  MergeResLists(file_system_, vecReslists,
                CFmtStr("%s\\%s\\%s", full_game_path_.String(),
                        final_dir_.String(), pchResListFilename),
                "GAME");
}

void ResourceListing::Init(const char *pchBaseDir, const char *pchGameDir) {
  // Because we have to call this inside the first Apps "PreInit", we need only
  // Init on the very first call
  if (is_initialized_) {
    return;
  }

  is_initialized_ = true;

  base_dir_ = pchBaseDir;
  game_dir_ = pchGameDir;

  char path[MAX_PATH];
  Q_snprintf(path, sizeof(path), "%s/%s", base_dir_.String(),
             game_dir_.String());
  Q_FixSlashes(path);
  Q_strlower(path);
  full_game_path_ = path;

  const char *pchCommandFile = nullptr;
  if (command_line_->CheckParm("-makereslists", &pchCommandFile) &&
      pchCommandFile) {
    // base path setup, now can get and parse command file
    InitCommandFile(path, pchCommandFile);
  }
}

bool ResourceListing::IsActive() { return is_initialized_ && is_active_; }

void ResourceListing::Collate() {
  char szDir[MAX_PATH];
  V_snprintf(szDir, sizeof(szDir), "%s\\%s", full_game_path_.String(),
             final_dir_.String());
  file_system_->CreateDirHierarchy(szDir, "GAME");

  // Now create the collated/merged data
  CollateFiles("all.lst");
  CollateFiles("engine.lst");
  for (auto &m : map_list_) {
    CollateFiles(CFmtStr("%s.lst", m.String()));
  }
}

void ResourceListing::SetupCommandLine() {
  if (!is_active_) return;

  switch (current_state_) {
    case State::BuildingListing: {
      Assert(current_wi_ < work_items_.Count());

      const WorkItem &work = work_items_[current_wi_];

      // Clean the working dir
      char szWorkingDir[512];
      Q_snprintf(szWorkingDir, sizeof(szWorkingDir), "%s\\%s",
                 working_dir_.String(), work.m_sSubDir.String());

      char szFullWorkingDir[MAX_PATH];
      V_snprintf(szFullWorkingDir, sizeof(szFullWorkingDir), "%s\\%s",
                 full_game_path_.String(), szWorkingDir);
      file_system_->CreateDirHierarchy(szFullWorkingDir, "GAME");

      // Preserve startmap
      const char *pszStartMap = nullptr;
      command_line_->CheckParm("-startmap", &pszStartMap);
      char szMap[MAX_PATH] = {0};
      if (pszStartMap) {
        V_strcpy_safe(szMap, pszStartMap);
      }

      // Prepare stuff
      // Reset command line based on current state
      char szCmd[512];
      Q_snprintf(szCmd, sizeof(szCmd), "%s %s %s -reslistdir %s",
                 original_command_line_.String(), base_command_line_.String(),
                 work.m_sAddCommands.String(), szWorkingDir);

      Warning("Reslists:  Setting command line:\n'%s'\n", szCmd);

      command_line_->CreateCmdLine(szCmd);
      // Never rebuild caches by default, inly do it in GeneratingCaches
      command_line_->AppendParm("-norebuildaudio", nullptr);
      if (szMap[0]) {
        command_line_->AppendParm("-startmap", szMap);
      }
    } break;
    case State::GeneratingCaches: {
      Collate();

      // Prepare stuff
      // Reset command line based on current state
      char szCmd[512];
      Q_snprintf(szCmd, sizeof(szCmd), "%s -reslistdir %s -rebuildaudio",
                 original_command_line_.String(), final_dir_.String());

      Warning("Caches:  Setting command line:\n'%s'\n", szCmd);

      command_line_->CreateCmdLine(szCmd);
      command_line_->RemoveParm("-norebuildaudio");
      command_line_->RemoveParm("-makereslists");

      current_state_ = State::Done;
    } break;
    case State::Done:
      break;
  }
}

bool ResourceListing::ShouldContinue() {
  if (!is_active_) return false;

  bool bContinueAdvancing = false;
  do {
    switch (current_state_) {
      default:
        break;
      case State::BuildingListing: {
        command_line_->RemoveParm("-startmap");

        // Advance to next time
        ++current_wi_;

        if (current_wi_ >= work_items_.Count()) {
          // Will stay in the loop
          current_state_ = State::GeneratingCaches;
          bContinueAdvancing = true;
        } else {
          return true;
        }
      } break;
      case State::GeneratingCaches:
        return true;
    }
  } while (bContinueAdvancing);

  return false;
}

void ResourceListing::LoadMapList(const char *pchGameDir,
                                  CUtlVector<CUtlString> &vecMaps,
                                  const char *pchMapFile) {
  char fullpath[512];
  Q_snprintf(fullpath, sizeof(fullpath), "%s/%s", pchGameDir, pchMapFile);

  // Load them in
  CUtlBuffer buf((intp)0, 0, CUtlBuffer::TEXT_BUFFER);

  if (file_system_->ReadFile(fullpath, "GAME", buf)) {
    char szMap[MAX_PATH];
    while (true) {
      buf.GetLine(szMap, sizeof(szMap));
      if (!szMap[0]) break;

      // Strip trailing CR/LF chars
      intp len = Q_strlen(szMap);
      while (len >= 1 && (szMap[len - 1] == '\n' || szMap[len - 1] == '\r')) {
        szMap[len - 1] = 0;
        len = Q_strlen(szMap);
      }

      CUtlString newMap;
      newMap = szMap;
      vecMaps.AddToTail(newMap);
    }
  } else {
    Error("Unable to maplist file %s\n", fullpath);
  }
}

bool ResourceListing::InitCommandFile(const char *pchGameDir,
                                      const char *pchCommandFile) {
  if (*pchCommandFile == '+' || *pchCommandFile == '-') {
    Msg("falling back to legacy reslists system\n");
    return false;
  }

  char fullpath[512];
  Q_snprintf(fullpath, sizeof(fullpath), "%s/%s", pchGameDir, pchCommandFile);

  CUtlBuffer buf;
  if (!file_system_->ReadFile(fullpath, "GAME", buf)) {
    Error("Unable to load '%s'\n", fullpath);
    return false;
  }

  KeyValues::AutoDelete kv = KeyValues::AutoDelete("reslists");
  if (!kv->LoadFromBuffer("reslists", buf.Base<const char>())) {
    Error("Unable to parse keyvalues from '%s'\n", fullpath);
  }

  CUtlString sMapListFile = kv->GetString("maplist", "maplist.txt");
  LoadMapList(pchGameDir, map_list_, sMapListFile);
  if (map_list_.Count() <= 0) {
    Error("Maplist file '%s' empty or missing!!!\n", sMapListFile.String());
  }

  const char *pszSolo = nullptr;
  if (command_line_->CheckParm("+map", &pszSolo) && pszSolo) {
    map_list_.Purge();

    CUtlString newMap;
    newMap = pszSolo;
    map_list_.AddToTail(newMap);
  }

  current_wi_ = command_line_->ParmValue("-startstage", 0);

  const char *pszStartMap = nullptr;
  command_line_->CheckParm("-startmap", &pszStartMap);
  if (pszStartMap) {
    initial_start_map_ = pszStartMap;
  }

  command_line_->RemoveParm("-startstage");
  command_line_->RemoveParm("-makereslists");
  command_line_->RemoveParm("-reslistdir");
  command_line_->RemoveParm("-norebuildaudio");
  command_line_->RemoveParm("-startmap");

  original_command_line_ = command_line_->GetCmdLine();

  // Add it back in for first map
  if (pszStartMap) {
    command_line_->AppendParm("-startmap", initial_start_map_.String());
  }

  base_command_line_ = kv->GetString("basecommandline", "");
  final_dir_ = kv->GetString("finaldir", final_dir_.String());
  working_dir_ = kv->GetString("workdir", working_dir_.String());

  int i = 0;
  do {
    char sz[32];
    Q_snprintf(sz, sizeof(sz), "%i", i);
    KeyValues *subKey = kv->FindKey(sz, false);
    if (!subKey) break;

    WorkItem work;

    work.m_sSubDir = subKey->GetString("subdir", "");
    work.m_sAddCommands = subKey->GetString("addcommands", "");

    if (work.m_sSubDir.Length() > 0) {
      work_items_.AddToTail(work);
    } else {
      Error("%s: failed to specify 'subdir' for item %s\n", fullpath, sz);
    }

    ++i;
  } while (true);

  is_active_ = work_items_.Count() > 0;

  current_wi_ = Clamp(current_wi_, (intp)0, work_items_.Count() - 1);

  bool bCollate = command_line_->CheckParm("-collate") ? true : false;
  if (bCollate) {
    Collate();
    is_active_ = false;
    exit(-1);
  }

  return is_active_;
}

void SortResourceListing(IFileSystem *file_system, const char *file_name,
                         const char *search_path) {
  CUtlRBTree<CUtlString, int> sorted{0, 0, ReslistLogLessFunc};

  LoadResourceListing(file_system, sorted, file_name, search_path);

  // Now write it back out
  SaveResourceListing(file_system, sorted, file_name, search_path);
}

}  // namespace se::launcher
