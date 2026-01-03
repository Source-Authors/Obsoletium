// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SE_UTILS_DX_PROXY_FILEMEMCACHE_H_
#define SE_UTILS_DX_PROXY_FILEMEMCACHE_H_

#include <atomic>

#include <unordered_map>
#include <string_view>

#include "tier0/threadtools.h"

namespace se::dx_proxy {

class CachedFileData final {
  friend class FileCache;

 public:
  [[nodiscard]] static CachedFileData *GetByDataPtr(const void *data_ptr);

  const char *GetFileName() const;
  const void *GetDataPtr() const;
  unsigned GetDataLen() const;

  unsigned AddRef() {
    return m_ref_count.fetch_add(1, std::memory_order::memory_order_relaxed) +
           1;
  }
  unsigned Release() {
    unsigned counter{
        m_ref_count.fetch_sub(1, std::memory_order::memory_order_relaxed) - 1};
    if (counter == 0) {
      this->~CachedFileData();
    }
    return counter;
  }

 protected:  // Constructed by FileCache
  ~CachedFileData();

  static CachedFileData *Create(const char *file_name);

 private:
  char m_file_name[256 - 8];
  std::atomic_uint m_ref_count;

  unsigned m_data_count;
  unsigned char m_data[1];  // file data spans further

  CachedFileData(const char *file_path, unsigned data_count)
      : m_data_count{data_count} {
    strcpy_s(m_file_name, file_path);

    AddRef();
  }
};

class FileCache final {
 public:
  FileCache() = default;
  ~FileCache() { Clear(); }

  CachedFileData *Get(const char *file_name);
  void Close(const void *data_ptr);
  void Clear();

 private:
  struct HasherAndComparer {
    const std::hash<std::string_view> hash = {};

    // Hasher.
    [[nodiscard]] inline size_t operator()(const char *s) const noexcept {
      return hash(std::string_view{s});
    }

    // Comparer.
    [[nodiscard]] inline bool operator()(const char *s1,
                                         const char *s2) const noexcept {
      return _stricmp(s1, s2) == 0;
    }
  };

  using FileName2DataMap =
      std::unordered_map<const char *, CachedFileData *, HasherAndComparer,
                         HasherAndComparer>;

  FileName2DataMap m_file_name_2_data_map;
  CThreadMutex m_mutex;
};

}  // namespace se::dx_proxy

#endif  // !SE_UTILS_DX_PROXY_FILEMEMCACHE_H_
