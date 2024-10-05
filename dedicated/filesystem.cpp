// Copyright Valve Corporation, All rights reserved.

#include "filesystem.h"

#include "tier1/interface.h"
#include "tier1/strtools.h"

extern IFileSystem *g_pFileSystem;
extern IBaseFileSystem *g_pBaseFileSystem;

namespace se::dedicated {

// Implement our own special factory that we don't export outside of the DLL, to
// stop people being able to get a pointer to a FILESYSTEM_INTERFACE_VERSION
// stdio interface.
void *FileSystemFactory(const char *name, int *rc) {
  if (!Q_stricmp(name, FILESYSTEM_INTERFACE_VERSION)) {
    if (rc) *rc = IFACE_OK;

    return g_pFileSystem;
  }

  if (!Q_stricmp(name, BASEFILESYSTEM_INTERFACE_VERSION)) {
    if (rc) *rc = IFACE_OK;

    return g_pBaseFileSystem;
  }

  if (rc) *rc = IFACE_FAILED;

  return nullptr;
}

}  // namespace se::dedicated
