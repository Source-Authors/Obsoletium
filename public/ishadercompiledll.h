// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef SRC_PUBLIC_ISHADERCOMPILEDLL_H_
#define SRC_PUBLIC_ISHADERCOMPILEDLL_H_

#include "tier1/interface.h"

constexpr inline char SHADER_COMPILE_INTERFACE_VERSION[]{"shadercompiledll_0"};

// This is the DLL interface to ShaderCompile
abstract_class IShaderCompileDLL {
 public:
  // All vrad.exe does is load the VRAD DLL and run this.
  virtual int main(int argc, char **argv) = 0;
};

#endif  // !SRC_PUBLIC_ISHADERCOMPILEDLL_H_
