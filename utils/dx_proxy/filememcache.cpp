// Copyright Valve Corporation, All rights reserved.
//
//

#include "filememcache.h"

namespace se::dx_proxy {

CachedFileData *CachedFileData::Create(char const *file_path) {
  FILE *f{fopen(file_path, "rb")};

  long file_size{-1};
  if (f) {
    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
  }

  void *buffer = malloc(static_cast<size_t>(eHeaderSize) + max(file_size, 0L));
  if (!buffer) return nullptr;

  // Placement new.
  auto *data = new (buffer) CachedFileData();

  strcpy_s(data->m_chFilename, file_path);

  data->m_numRefs = 0;
  data->m_numDataBytes = static_cast<unsigned>(file_size);

  if (f) {
    fread(data->m_data, 1, file_size, f);
    fclose(f);
  }

  return data;
}

CachedFileData::~CachedFileData() { free(this); }

CachedFileData *CachedFileData::GetByDataPtr(void const *ptr) {
  auto *buffer = static_cast<unsigned char const *>(ptr);
  auto *data = reinterpret_cast<CachedFileData const *>(buffer - eHeaderSize);

  return const_cast<CachedFileData *>(data);
}

char const *CachedFileData::GetFileName() const { return m_chFilename; }

void const *CachedFileData::GetDataPtr() const { return m_data; }

unsigned CachedFileData::GetDataLen() const {
  return m_numDataBytes != UINT_MAX ? m_numDataBytes : 0U;
}

bool CachedFileData::IsValid() const { return m_numDataBytes != UINT_MAX; }

CachedFileData *FileCache::Get(char const *szFilename) {
  // Search the cache first
  auto it = m_map.find(szFilename);
  if (it != m_map.end()) return it->second;

  // Create the cached file data
  CachedFileData *data = CachedFileData::Create(szFilename);
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
