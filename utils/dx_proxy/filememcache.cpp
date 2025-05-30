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

  // Placement new.
  auto *data = new (buffer) CachedFileData();
  strcpy_s(data->m_chFilename, file_path);

  std::tie(std::ignore, errc) =
      f.read(data->m_data, file_size_safe, 1, file_size_safe);
  if (errc) {
    free(buffer);

    char error[MAX_PATH * 2 + 32];
    sprintf_s(error, "Unable to read %u bytes from '%s' for caching: %s.\n",
              file_size_safe, file_path, errc.message().c_str());

    OutputDebugString(error);
    return nullptr;
  }

  data->m_numRefs = 0;
  data->m_numDataBytes = file_size_safe;

  return data;
}

CachedFileData::~CachedFileData() { free(this); }

CachedFileData *CachedFileData::GetByDataPtr(const void *data_ptr) {
  // Expected to point to m_data.
  auto *buffer = static_cast<const unsigned char *>(data_ptr);

  constexpr size_t kDataOffset{offsetof(CachedFileData, m_data)};
  auto *data = reinterpret_cast<const CachedFileData *>(buffer - kDataOffset);

  return const_cast<CachedFileData *>(data);
}

char const *CachedFileData::GetFileName() const { return m_chFilename; }

void const *CachedFileData::GetDataPtr() const { return m_data; }

unsigned CachedFileData::GetDataLen() const {
  return m_numDataBytes != UINT_MAX ? m_numDataBytes : 0U;
}

bool CachedFileData::IsValid() const { return m_numDataBytes != UINT_MAX; }

CachedFileData *FileCache::Get(char const *file_name) {
  // Search the cache first
  auto it = m_map.find(file_name);
  if (it != m_map.end()) return it->second;

  // Create the cached file data
  CachedFileData *data = CachedFileData::Create(file_name);
  if (data) m_map.emplace(data->GetFileName(), data);

  return data;
}

void FileCache::Clear() {
  for (auto &[_, data] : m_map) {
    data->~CachedFileData();
  }

  m_map.clear();
}

}  // namespace se::dx_proxy
