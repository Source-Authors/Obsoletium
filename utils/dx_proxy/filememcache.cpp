// Copyright Valve Corporation, All rights reserved.
//
//

#include "filememcache.h"

#include <cinttypes>

#include "posix_file_stream.h"
#include "winlite.h"

namespace se::dx_proxy {

CachedFileData *CachedFileData::Create(char const *file_path) {
  auto [f, errc] = se::posix::posix_file_stream_factory::open(file_path, "rb");
  if (errc) {
    char error[MAX_PATH * 2 + 32];
    sprintf_s(error, "Unable to read '%s' for caching: %s.\n", file_path,
              errc.message().c_str());

    OutputDebugString(error);
    return nullptr;
  }

  int64_t file_size{-1};
  std::tie(file_size, errc) = f.size();
  if (errc) {
    char error[MAX_PATH * 2 + 32];
    sprintf_s(error, "Unable to get size of '%s' for caching: %s.\n", file_path,
              errc.message().c_str());

    OutputDebugString(error);
    return nullptr;
  }

  constexpr size_t kDataOffset{offsetof(CachedFileData, m_data)};
  // Limit to unsigned as assign to.
  using MaxType = std::common_type_t<decltype(file_size), unsigned>;

  if (static_cast<MaxType>(file_size + static_cast<int64_t>(kDataOffset)) >
      static_cast<MaxType>(std::numeric_limits<unsigned>::max())) {
    char error[MAX_PATH * 2 + 32];
    sprintf_s(error,
              "Unable to cache '%s', size %" PRId64 " is too large: %s.\n",
              file_path, file_size,
              std::generic_category().message(EOVERFLOW).c_str());

    OutputDebugString(error);
    return nullptr;
  }

  const unsigned file_size_safe{static_cast<unsigned>(file_size)};
  void *buffer{malloc(kDataOffset + file_size_safe)};
  if (!buffer) return nullptr;

  std::tie(std::ignore, errc) =
      f.read(static_cast<byte *>(buffer) + kDataOffset, file_size_safe, 1,
             file_size_safe);
  if (errc) {
    free(buffer);

    char error[MAX_PATH * 2 + 32];
    sprintf_s(error, "Unable to read %u bytes from '%s' for caching: %s.\n",
              file_size_safe, file_path, errc.message().c_str());

    OutputDebugString(error);
    return nullptr;
  }

  // Placement new.
  return new (buffer) CachedFileData{file_path, file_size_safe};
}

CachedFileData::~CachedFileData() { free(this); }

CachedFileData *CachedFileData::GetByDataPtr(const void *data_ptr) {
  // Expected to point to m_data.
  auto *buffer = static_cast<const unsigned char *>(data_ptr);

  constexpr size_t kDataOffset{offsetof(CachedFileData, m_data)};
  auto *data = reinterpret_cast<const CachedFileData *>(buffer - kDataOffset);

  return const_cast<CachedFileData *>(data);
}

char const *CachedFileData::GetFileName() const { return m_file_name; }

void const *CachedFileData::GetDataPtr() const { return m_data; }

unsigned CachedFileData::GetDataLen() const { return m_data_count; }

CachedFileData *FileCache::Get(char const *file_name) {
  {
    AUTO_LOCK(m_mutex);

    // Search the cache first.
    auto it = m_file_name_2_data_map.find(file_name);
    if (it != m_file_name_2_data_map.end()) {
      CachedFileData *data{it->second};
      data->AddRef();
      return data;
    }
  }

  // Create the cached file data.
  CachedFileData *expected_data = CachedFileData::Create(file_name);
  if (expected_data) {
    CachedFileData *actual_data;

    {
      AUTO_LOCK(m_mutex);

      auto pair = m_file_name_2_data_map.emplace(expected_data->GetFileName(),
                                                 expected_data);

      actual_data = pair.first->second;
    }

    // May already be inserted by other thread, so need to cleanup.
    if (expected_data != actual_data) expected_data->Release();

    return actual_data;
  }

  return nullptr;
}

void FileCache::Close(const void *data_ptr) {
  if (CachedFileData *data = CachedFileData::GetByDataPtr(data_ptr)) {
    char file_name[256];
    strcpy_s(file_name, data->GetFileName());

    if (data->Release() == 0) {
      AUTO_LOCK(m_mutex);

      m_file_name_2_data_map.erase(file_name);
    }
  }
}

void FileCache::Clear() {
  AUTO_LOCK(m_mutex);

  for (auto &[_, data] : m_file_name_2_data_map) {
    data->~CachedFileData();
  }

  m_file_name_2_data_map.clear();
}

}  // namespace se::dx_proxy
