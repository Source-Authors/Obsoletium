// Copyright Valve Corporation, All rights reserved.

#if defined(DX9_V00_PC)
#define SRC_PRODUCT_FILE_DESCRIPTION_STRING   "DirectX 9.0 Shader Model 1 & 2 Compiler Proxy"
#elif defined(DX9_V30_PC)
#define SRC_PRODUCT_FILE_DESCRIPTION_STRING	  "DirectX 9.0 Shader Model 3 Compiler Proxy"
#elif defined(DX10_V00_PC)
#define SRC_PRODUCT_FILE_DESCRIPTION_STRING	  "DirectX 10.0 Shader Model 4 Compiler Proxy"
#else
#error Unknown DirectX version
#endif
