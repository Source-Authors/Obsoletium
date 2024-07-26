// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_UTILS_DX_PROXY_DXINCLUDEIMPL_H_
#define SRC_UTILS_DX_PROXY_DXINCLUDEIMPL_H_

#include <d3dcompiler.h>

#include "filememcache.h"

namespace se::dx_proxy {

class D3DIncludeImpl : public ID3DInclude {
 public:
  STDMETHOD(Open)
  (THIS_ D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData,
   LPCVOID *ppData, UINT *pBytes) override {
    CachedFileData *data = file_cache_.Get(pFileName);
    if (data && data->IsValid()) {
      *ppData = data->GetDataPtr();
      *pBytes = data->GetDataLen();

      data->AddRef();

      return S_OK;
    }

    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  STDMETHOD(Close)(THIS_ LPCVOID pData) override {
    if (CachedFileData *data = CachedFileData::GetByDataPtr(pData)) {
      data->Release();
    }

    return S_OK;
  }

 private:
  FileCache file_cache_;
};

}  // namespace se::dx_proxy

#endif  // !SRC_UTILS_DX_PROXY_DXINCLUDEIMPL_H_
