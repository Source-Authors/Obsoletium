// Copyright Valve Corporation, All rights reserved.

#include "input_layout_dx11.h"

#include "shader_device_dx11.h"

#include "winlite.h"
#include <d3d11.h>
#include <d3d11shader.h>
#include <dxgiformat.h>

#include "materialsystem/imesh.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

namespace {

// Standard input layouts.
constexpr inline DXGI_FORMAT kSize2DxgiFormatLookup[] = {
    DXGI_FORMAT_UNKNOWN,             // Should be unused...
    DXGI_FORMAT_R32_FLOAT,           // D3DDECLTYPE_FLOAT1
    DXGI_FORMAT_R32G32_FLOAT,        // D3DDECLTYPE_FLOAT2,
    DXGI_FORMAT_R32G32B32_FLOAT,     // D3DDECLTYPE_FLOAT3,
    DXGI_FORMAT_R32G32B32A32_FLOAT,  // D3DDECLTYPE_FLOAT4
};

struct FieldInfo_t {
  const char *semantic_string;
  uint32_t semantic_index;
  uint64_t format_mask;
  uint32_t field_size;
};

FieldInfo_t g_field_infos[] = {
    {"POSITION", 0, VERTEX_POSITION, sizeof(float) * 3},
    {"BLENDWEIGHT", 0, VERTEX_BONE_WEIGHT_MASK, 0},
    {"BLENDINDICES", 0, VERTEX_BONE_INDEX, 4},
    {"NORMAL", 0, VERTEX_NORMAL, sizeof(float) * 3},
    {"COLOR", 0, VERTEX_COLOR, 4},
    {"SPECULAR", 0, VERTEX_SPECULAR, 4},
    {"TEXCOORD", 0, VERTEX_TEXCOORD_MASK(0), 0},
    {"TEXCOORD", 1, VERTEX_TEXCOORD_MASK(1), 0},
    {"TEXCOORD", 2, VERTEX_TEXCOORD_MASK(2), 0},
    {"TEXCOORD", 3, VERTEX_TEXCOORD_MASK(3), 0},
    {"TEXCOORD", 4, VERTEX_TEXCOORD_MASK(4), 0},
    {"TEXCOORD", 5, VERTEX_TEXCOORD_MASK(5), 0},
    {"TEXCOORD", 6, VERTEX_TEXCOORD_MASK(6), 0},
    {"TEXCOORD", 7, VERTEX_TEXCOORD_MASK(7), 0},
    {"TANGENT", 0, VERTEX_TANGENT_S, sizeof(float) * 3},
    {"BINORMAL", 0, VERTEX_TANGENT_T, sizeof(float) * 3},
    {"USERDATA", 0, USER_DATA_SIZE_MASK, 0}};

D3D11_INPUT_ELEMENT_DESC kVertexDescs[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
     D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"BLENDWEIGHT", 0, DXGI_FORMAT_UNKNOWN, 0, 0, D3D11_INPUT_PER_VERTEX_DATA,
     0},
    {"BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0,
     D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
     D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0, D3D11_INPUT_PER_VERTEX_DATA,
     0},
    {"SPECULAR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0,
     D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_UNKNOWN, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 1, DXGI_FORMAT_UNKNOWN, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 2, DXGI_FORMAT_UNKNOWN, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 3, DXGI_FORMAT_UNKNOWN, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 4, DXGI_FORMAT_UNKNOWN, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 5, DXGI_FORMAT_UNKNOWN, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 6, DXGI_FORMAT_UNKNOWN, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 7, DXGI_FORMAT_UNKNOWN, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
     D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
     D3D11_INPUT_PER_VERTEX_DATA, 0},
    {"USERDATA", 0, DXGI_FORMAT_UNKNOWN, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
};

constexpr D3D11_INPUT_ELEMENT_DESC kFallbackVertexDescs[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 15, 0,
     D3D11_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32_FLOAT, 15, 12,
     D3D11_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 15, 20,
     D3D11_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 15, 24,
     D3D11_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 15, 36,
     D3D11_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"SPECULAR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 15, 40,
     D3D11_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 15, 44,
     D3D11_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 15, 52,
     D3D11_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 15, 60,
     D3D11_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TEXCOORD", 3, DXGI_FORMAT_R32G32_FLOAT, 15, 68,
     D3D11_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TEXCOORD", 4, DXGI_FORMAT_R32G32_FLOAT, 15, 76,
     D3D11_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TEXCOORD", 5, DXGI_FORMAT_R32G32_FLOAT, 15, 84,
     D3D11_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TEXCOORD", 6, DXGI_FORMAT_R32G32_FLOAT, 15, 92,
     D3D11_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TEXCOORD", 7, DXGI_FORMAT_R32G32_FLOAT, 15, 100,
     D3D11_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 15, 108,
     D3D11_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 15, 120,
     D3D11_INPUT_PER_INSTANCE_DATA, UINT_MAX},
    {"USERDATA", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 15, 132,
     D3D11_INPUT_PER_INSTANCE_DATA, UINT_MAX},
};

// Checks to see if a shader requires a particular field.
bool CheckShaderSignatureExpectations(ID3D11ShaderReflection *reflection,
                                      const char *semantic_string,
                                      unsigned int semantic_index) {
  Assert(reflection);
  Assert(semantic_string);

  D3D11_SHADER_DESC shader_desc;
  HRESULT hr{reflection->GetDesc(&shader_desc)};
  if (FAILED(hr)) {
    Assert(SUCCEEDED(hr));
    return false;
  }

  D3D11_SIGNATURE_PARAMETER_DESC param_desc;
  for (decltype(shader_desc.InputParameters) k{0};
       k < shader_desc.InputParameters; k++) {
    hr = reflection->GetInputParameterDesc(k, &param_desc);
    if (FAILED(hr)) {
      Assert(SUCCEEDED(hr));
      return false;
    }

    if (semantic_index == param_desc.SemanticIndex &&
        !V_stricmp(semantic_string, param_desc.SemanticName)) {
      return true;
    }
  }

  return false;
}

// Computes the required input desc based on the vertex format.
template <uint32_t size>
uint32_t ComputeInputDesc(VertexFormat_t fmt,
                          D3D11_INPUT_ELEMENT_DESC (&input_desc)[size],
                          ID3D11ShaderReflection *reflection) {
  uint32_t count{0}, offset{0};

  // Fix up the global table so we don't need special-case code
  const int bones_count{NumBoneWeights(fmt)};
  g_field_infos[1].field_size = sizeof(float) * bones_count;
  kVertexDescs[1].Format = kSize2DxgiFormatLookup[bones_count];

  const int user_data_size{UserDataSize(fmt)};
  g_field_infos[16].field_size = sizeof(float) * user_data_size;
  kVertexDescs[16].Format = kSize2DxgiFormatLookup[user_data_size];

  // NOTE: Fix g_field_infos, kVertexDescs, kFallbackVertexDescs if you add
  // more fields As well as the fallback stream (stream #15)
  static_assert(VERTEX_MAX_TEXTURE_COORDINATES == 8);
  static_assert(6 + VERTEX_MAX_TEXTURE_COORDINATES < std::size(g_field_infos));
  static_assert(6 + VERTEX_MAX_TEXTURE_COORDINATES < std::size(kVertexDescs));
  for (int i{0}; i < VERTEX_MAX_TEXTURE_COORDINATES; ++i) {
    const int tex_coord_count{TexCoordSize(i, fmt)};

    g_field_infos[6 + i].field_size = sizeof(float) * tex_coord_count;
    kVertexDescs[6 + i].Format = kSize2DxgiFormatLookup[tex_coord_count];
  }

  static_assert(std::size(g_field_infos) <= size);

  size_t i{0};
  // FIXME: Change this loop so CheckShaderSignatureExpectations is called once!
  for (const auto &field_info : g_field_infos) {
    if (fmt & field_info.format_mask) {
      memcpy(&input_desc[count], &kVertexDescs[i],
             sizeof(D3D11_INPUT_ELEMENT_DESC));

      input_desc[count].AlignedByteOffset = offset;
      offset += field_info.field_size;

      ++count;
    } else if (CheckShaderSignatureExpectations(reflection,
                                                field_info.semantic_string,
                                                field_info.semantic_index)) {
      memcpy(&input_desc[count], &kFallbackVertexDescs[i],
             sizeof(D3D11_INPUT_ELEMENT_DESC));

      ++count;
    }

    ++i;
  }

  return count;
}

}  // namespace

namespace se::shaderapidx11 {

// Gets the input layout associated with a vertex format.
win::com::com_ptr<ID3D11InputLayout> CreateInputLayout(
    VertexFormat_t format, ID3D11Device5 *device,
    ID3D11ShaderReflection *reflection, const void *byte_code,
    size_t byte_code_size) {
  D3D11_INPUT_ELEMENT_DESC descs[32];
  const uint32_t decs_count{ComputeInputDesc(format, descs, reflection)};

  Assert(device);
  Assert(byte_code);

  se::win::com::com_ptr<ID3D11InputLayout> input_layout;
  HRESULT hr = device->CreateInputLayout(descs, decs_count, byte_code,
                                         byte_code_size, &input_layout);
  if (FAILED(hr)) {
    Warning(
        "CreateInputLayout: Unable to create input layout for format %llX (hr "
        "0x%x).\n",
        format, hr);
    return {};
  }

  SetDebugName(input_layout, "Input Layout");
  return input_layout;
}

}  // namespace se::shaderapidx11
