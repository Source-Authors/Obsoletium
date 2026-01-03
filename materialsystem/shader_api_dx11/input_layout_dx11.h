// Copyright Valve Corporation, All rights reserved.

#ifndef SE_MATERIALSYSTEM_SHADERAPIDX11_INPUT_LAYOUT_DX11_H_
#define SE_MATERIALSYSTEM_SHADERAPIDX11_INPUT_LAYOUT_DX11_H_

#include <cstddef>  // std::size_t
#include <cstdint>

#define NOMINMAX
#include "com_ptr.h"

#include <d3d11_4.h>
#include <d3d11shader.h>

using VertexFormat_t = uint64_t;

namespace se::shaderapidx11 {

// Gets the input layout associated with a vertex format
// FIXME: Note that we'll need to change this from a VertexFormat_t.
win::com::com_ptr<ID3D11InputLayout> CreateInputLayout(
    VertexFormat_t format, ID3D11Device5 *device,
    ID3D11ShaderReflection *shader_reflection, const void *byte_code,
    std::size_t byte_code_size);

}  // namespace se::shaderapidx11

#endif  // !SE_MATERIALSYSTEM_SHADERAPIDX11_INPUT_LAYOUT_DX11_H_
