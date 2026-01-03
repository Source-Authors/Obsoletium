// Copyright Valve Corporation, All rights reserved.

#if defined(DX9_V00_PC)
#define SE_PRODUCT_FILE_DESCRIPTION_STRING   "DirectX 9.0 Shader Model 1 & 2 Compiler Proxy"
#elif defined(DX9_V30_PC)
#define SE_PRODUCT_FILE_DESCRIPTION_STRING	  "DirectX 9.0 Shader Model 3 Compiler Proxy"
#elif defined(DX11_V00_PC)
#define SE_PRODUCT_FILE_DESCRIPTION_STRING	  "DirectX 11.0 Shader Model 5 Compiler Proxy"
#else
#error Unknown DirectX version
#endif
