// Copyright Valve Corporation, All rights reserved.
//
// D3DX command implementation.

#ifndef SE_UTILS_SHADERCOMPILE_D3DXFXC_H_
#define SE_UTILS_SHADERCOMPILE_D3DXFXC_H_

#include <memory>

#include "cmdsink.h"

#define kShaderCompilerOutputName "output.txt"

namespace s1::shader_compile::fxc_intercept {

constexpr inline char kShaderArtefactOutputName[]{"shader.o"};

std::unique_ptr<s1::shader_compile::command_sink::IResponse> TryExecuteCommand(
    const char *pCommand);

};  // namespace s1::shader_compile::fxc_intercept

#endif  // !SE_UTILS_SHADERCOMPILE_D3DXFXC_H_
