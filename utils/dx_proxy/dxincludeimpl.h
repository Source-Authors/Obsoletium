// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SE_UTILS_DX_PROXY_DXINCLUDEIMPL_H_
#define SE_UTILS_DX_PROXY_DXINCLUDEIMPL_H_

#include <d3dcompiler.h>

#include "filememcache.h"

namespace se::dx_proxy {

class D3DIncludeImpl : public ID3DInclude {
 public:
  STDMETHOD(Open)
  (THIS_ D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData,
   LPCVOID *ppData, UINT *pBytes) override {
    if (!pFileName || !ppData || !pBytes) return E_POINTER;

    CachedFileData *data = file_cache_.Get(pFileName);
    if (data) {
      *ppData = data->GetDataPtr();
      *pBytes = data->GetDataLen();

      return S_OK;
    }

    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  STDMETHOD(Close)(THIS_ LPCVOID pData) override {
    if (!pData) return E_POINTER;

    // If we close files here, we lost cached file data.
    // I/O is slow, so better to consume more RAM and store all include files for next compilation.
    // Cache will be purged in destructor, so file data never leaks.
    // file_cache_.Close(pData);

    return S_OK;
  }

 private:
  FileCache file_cache_;
};

}  // namespace se::dx_proxy

#endif  // !SE_UTILS_DX_PROXY_DXINCLUDEIMPL_H_
