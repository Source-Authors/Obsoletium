// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_UTILS_DX_PROXY_FILEMEMCACHE_H_
#define SRC_UTILS_DX_PROXY_FILEMEMCACHE_H_

#include <unordered_map>

#include "tier0/platform.h"
#include "tier1/generichash.h"

namespace se::dx_proxy {

class CachedFileData final {
  friend class FileCache;

 public:
  static CachedFileData *GetByDataPtr(void const *pvDataPtr);

  char const *GetFileName() const;
  void const *GetDataPtr() const;
  unsigned GetDataLen() const;

  unsigned AddRef() { return ++m_numRefs; }
  unsigned Release() { return --m_numRefs; }

  bool IsValid() const;

 protected:  // Constructed by FileCache
  ~CachedFileData();

  static CachedFileData *Create(char const *szFilename);

 private:
  enum { eHeaderSize = 256 };

  char m_chFilename[256 - 8];
  unsigned m_numRefs;
  unsigned m_numDataBytes;

  unsigned char m_data[0];  // file data spans further

  CachedFileData() = default;
};

class FileCache final {
 public:
  FileCache() = default;
  ~FileCache() { Clear(); }

  CachedFileData *Get(char const *szFilename);
  void Clear();

 private:
  struct FileNameEq {
    inline size_t operator()(const char *s) const noexcept {
      if (!s) return 0;

      return HashString(s);
    }

    inline bool operator()(const char *s1, const char *s2) const noexcept {
      return _stricmp(s1, s2) < 0;
    }
  };

  using Mapping = std::unordered_map<const char *, CachedFileData *, FileNameEq,
                                     FileNameEq>;

  Mapping m_map;
};

}  // namespace se::dx_proxy

#endif  // !SRC_UTILS_DX_PROXY_FILEMEMCACHE_H_
