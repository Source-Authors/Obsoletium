// Copyright Valve Corporation, All rights reserved.
//
// D3DX command implementation.

#ifndef SRC_UTILS_SHADERCOMPILE_D3DXFXC_H_
#define SRC_UTILS_SHADERCOMPILE_D3DXFXC_H_

#include "cmdsink.h"

namespace se::shader_compile::fxc_intercept {

bool TryExecuteCommand(
    const char *pCommand,
    se::shader_compile::command_sink::IResponse **ppResponse);

};  // namespace se::shader_compile::fxc_intercept

#endif  // !SRC_UTILS_SHADERCOMPILE_D3DXFXC_H_
