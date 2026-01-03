// Copyright Valve Corporation, All rights reserved.
//
// D3DX command implementation.

#ifndef SE_UTILS_SHADERCOMPILE_D3DXFXC_H_
#define SE_UTILS_SHADERCOMPILE_D3DXFXC_H_

#include <memory>

#include "cmdsink.h"

#define kShaderCompilerOutputName "output.txt"

namespace se::shader_compile::fxc_intercept {

constexpr inline char kShaderArtefactOutputName[]{"shader.o"};

std::unique_ptr<se::shader_compile::command_sink::IResponse> TryExecuteCommand(
    const char *pCommand);

};  // namespace se::shader_compile::fxc_intercept

#endif  // !SE_UTILS_SHADERCOMPILE_D3DXFXC_H_
