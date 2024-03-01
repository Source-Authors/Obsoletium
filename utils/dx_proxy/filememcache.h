// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_UTILS_DX_PROXY_FILEMEMCACHE_H_
#define SRC_UTILS_DX_PROXY_FILEMEMCACHE_H_

#include <atomic>

#include <unordered_map>
#include <string_view>

namespace se::dx_proxy {

class CachedFileData final {
  friend class FileCache;

 public:
  [[nodiscard]] static CachedFileData *GetByDataPtr(const void *data_ptr);

  const char *GetFileName() const;
  const void *GetDataPtr() const;
  unsigned GetDataLen() const;

  unsigned AddRef() {
    return m_numRefs.fetch_add(1, std::memory_order::memory_order_relaxed);
  }
  unsigned Release() {
    return m_numRefs.fetch_sub(1, std::memory_order::memory_order_relaxed);
  }

  bool IsValid() const;

 protected:  // Constructed by FileCache
  ~CachedFileData();

  static CachedFileData *Create(const char *file_name);

 private:
  char m_chFilename[256 - 8];
  std::atomic_uint m_numRefs;
  unsigned m_numDataBytes;

  unsigned char m_data[1];  // file data spans further

  CachedFileData() = default;
};

class FileCache final {
 public:
  FileCache() = default;
  ~FileCache() { Clear(); }

  CachedFileData *Get(const char *file_name);
  void Clear();

 private:
  struct HasherAndComparer {
    const std::hash<std::string_view> hash = {};

    // Hasher.
    [[nodiscard]] inline size_t operator()(const char *s) const noexcept {
      if (!s) return 0;

      return hash(std::string_view{s, strlen(s)});
    }

    // Comparer.
    [[nodiscard]] inline bool operator()(const char *s1,
                                         const char *s2) const noexcept {
      return _stricmp(s1, s2) < 0;
    }
  };

  using Mapping = std::unordered_map<const char *, CachedFileData *,
                                     HasherAndComparer, HasherAndComparer>;

  Mapping m_map;
};

}  // namespace se::dx_proxy

#endif  // !SRC_UTILS_DX_PROXY_FILEMEMCACHE_H_
