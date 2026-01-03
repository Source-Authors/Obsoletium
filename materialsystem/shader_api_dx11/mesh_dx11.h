// Copyright Valve Corporation, All rights reserved.

#ifndef SE_MATERIALSYSTEM_SHADERAPIDX11_MESH_DX11_H_
#define SE_MATERIALSYSTEM_SHADERAPIDX11_MESH_DX11_H_

#include "meshbase.h"
#include "shaderapi/IShaderDevice.h"

#define NOMINMAX
#include <d3d11_4.h>
#include "com_ptr.h"

namespace se::shaderapidx11 {

// Vertex buffer on DirectX 11 hardware.
class CVertexBufferDx11 final : public CVertexBufferBase {
  using BaseClass = CVertexBufferBase;

 public:
  int VertexCount() const override;
  VertexFormat_t GetVertexFormat() const override;
  bool Lock(int nMaxVertexCount, bool bAppend, VertexDesc_t& desc) override;
  void Unlock(int nWrittenVertexCount, VertexDesc_t& desc) override;
  bool IsDynamic() const override;
  void BeginCastBuffer(VertexFormat_t format) override;
  void EndCastBuffer() override;
  int GetRoomRemaining() const override;

 public:
  CVertexBufferDx11(win::com::com_ptr<ID3D11Device5> device,
                    win::com::com_ptr<ID3D11DeviceContext4> deviceContext,
                    ShaderBufferType_t type, VertexFormat_t format,
                    int nVertexCount, const char* pBudgetGroupName);
  ~CVertexBufferDx11();

  win::com::com_ptr<ID3D11Buffer> GetDx11Buffer() const {
    return m_pVertexBuffer;
  }
  int VertexSize() const;

  // Only used by dynamic buffers, indicates the next lock should perform a
  // discard.
  void Flush();

 protected:
  // Creates, destroys the index buffer
  bool Allocate();
  void Free();

  win::com::com_ptr<ID3D11Device5> m_Device;
  win::com::com_ptr<ID3D11DeviceContext4> m_DeviceContext;
  win::com::com_ptr<ID3D11Buffer> m_pVertexBuffer;
  VertexFormat_t m_VertexFormat;
  int m_nVertexCount;
  int m_nBufferSize;
  int m_nFirstUnwrittenOffset;
  bool m_bIsLocked : 1;
  bool m_bIsDynamic : 1;
  // Used only for dynamic buffers, indicates to discard the next time.
  bool m_bFlush : 1;

#ifdef _DEBUG
  static uint32_t s_nBufferCount;
#endif
};

inline int CVertexBufferDx11::VertexSize() const {
  return VertexFormatSize(m_VertexFormat);
}

// Index buffer on DirectX 11 hardware.
class CIndexBufferDx11 final : public CIndexBufferBase {
  using BaseClass = CIndexBufferBase;

 public:
  int IndexCount() const override;
  MaterialIndexFormat_t IndexFormat() const override;
  int GetRoomRemaining() const override;
  bool Lock(int nMaxIndexCount, bool bAppend, IndexDesc_t& desc) override;
  void Unlock(int nWrittenIndexCount, IndexDesc_t& desc) override;
  void ModifyBegin(bool bReadOnly, int nFirstIndex, int nIndexCount,
                   IndexDesc_t& desc) override;
  void ModifyEnd(IndexDesc_t& desc) override;
  bool IsDynamic() const override;
  void BeginCastBuffer(MaterialIndexFormat_t format) override;
  void EndCastBuffer() override;

 public:
  CIndexBufferDx11(win::com::com_ptr<ID3D11Device5> device,
                   win::com::com_ptr<ID3D11DeviceContext4> deviceContext,
                   ShaderBufferType_t type, MaterialIndexFormat_t fmt,
                   int nIndexCount, const char* pBudgetGroupName);
  ~CIndexBufferDx11();

  win::com::com_ptr<ID3D11Buffer> GetDx11Buffer() const {
    return m_pIndexBuffer;
  }
  MaterialIndexFormat_t GetIndexFormat() const;

  // Only used by dynamic buffers, indicates the next lock should perform a
  // discard.
  void Flush();

 protected:
  // Creates, destroys the index buffer.
  bool Allocate();
  void Free();

  // Returns the size of the index in bytes
  int IndexSize() const;

  win::com::com_ptr<ID3D11Device5> m_Device;
  win::com::com_ptr<ID3D11DeviceContext4> m_DeviceContext;
  win::com::com_ptr<ID3D11Buffer> m_pIndexBuffer;
  MaterialIndexFormat_t m_IndexFormat;
  int m_nIndexCount;
  int m_nBufferSize;
  // Used only for dynamic buffers, indicates where it's safe to write
  // (nooverwrite).
  int m_nFirstUnwrittenOffset;
  bool m_bIsLocked : 1;
  bool m_bIsDynamic : 1;
  // Used only for dynamic buffers, indicates to discard the next time.
  bool m_bFlush : 1;

#ifdef _DEBUG
  static uint32_t s_nBufferCount;
#endif
};

// Returns the size of the index in bytes.
inline int CIndexBufferDx11::IndexSize() const {
  switch (m_IndexFormat) {
    default:
    case MATERIAL_INDEX_FORMAT_UNKNOWN:
      return 0;
    case MATERIAL_INDEX_FORMAT_16BIT:
      return 2;
    case MATERIAL_INDEX_FORMAT_32BIT:
      return 4;
  }
}

inline MaterialIndexFormat_t CIndexBufferDx11::GetIndexFormat() const {
  return m_IndexFormat;
}

// Mesh on DirectX 11 hardware.
class CMeshDx11 final : public CMeshBase {
 public:
  CMeshDx11();
  ~CMeshDx11();

  // FIXME: Make this work! Unsupported methods of IIndexBuffer
  bool Lock(int nMaxIndexCount, bool bAppend, IndexDesc_t& desc) override {
    Assert(0);
    return false;
  }
  void Unlock(int nWrittenIndexCount, IndexDesc_t& desc) override { Assert(0); }

  int GetRoomRemaining() const override {
    Assert(0);
    return 0;
  }

  void ModifyBegin(bool bReadOnly, int nFirstIndex, int nIndexCount,
                   IndexDesc_t& desc) override {
    Assert(0);
  }
  void ModifyEnd(IndexDesc_t& desc) override { Assert(0); }

  void Spew(int nIndexCount, const IndexDesc_t& desc) override { Assert(0); }

  void ValidateData(int nIndexCount, const IndexDesc_t& desc) override {
    Assert(0);
  }

  bool Lock(int nVertexCount, bool bAppend, VertexDesc_t& desc) override {
    Assert(0);
    return false;
  }
  void Unlock(int nVertexCount, VertexDesc_t& desc) override { Assert(0); }

  void Spew(int nVertexCount, const VertexDesc_t& desc) override { Assert(0); }

  void ValidateData(int nVertexCount, const VertexDesc_t& desc) override {
    Assert(0);
  }

  bool IsDynamic() const override {
    Assert(0);
    return false;
  }

  void BeginCastBuffer(MaterialIndexFormat_t format) override { Assert(0); }
  void BeginCastBuffer(VertexFormat_t format) override { Assert(0); }
  void EndCastBuffer() override { Assert(0); }

  void LockMesh(int numVerts, int numIndices, MeshDesc_t& desc);
  void UnlockMesh(int numVerts, int numIndices, MeshDesc_t& desc);

  void ModifyBeginEx(bool bReadOnly, int firstVertex, int numVerts,
                     int firstIndex, int numIndices, MeshDesc_t& desc);
  void ModifyBegin(int firstVertex, int numVerts, int firstIndex,
                   int numIndices, MeshDesc_t& desc);
  void ModifyEnd(MeshDesc_t& desc);

  // returns the # of vertices (static meshes only)
  int VertexCount() const;

  void BeginPass() override {}
  void RenderPass() override {}
  bool HasColorMesh() const override { return false; }
  bool IsUsingMorphData() const override { return false; }
  bool HasFlexMesh() const override { return false; }

  // Sets the primitive type
  void SetPrimitiveType(MaterialPrimitiveType_t type);

  // Draws the entire mesh
  void Draw(int firstIndex, int numIndices);

  void Draw(CPrimList* pPrims, int nPrims);

  // Copy verts and/or indices to a mesh builder. This only works for temp
  // meshes!
  virtual void CopyToMeshBuilder(
      int iStartVert,  // Which vertices to copy.
      int nVerts,
      int iStartIndex,  // Which indices to copy.
      int nIndices,
      int indexOffset,  // This is added to each index.
      CMeshBuilder& builder);

  // Spews the mesh data
  void Spew(int numVerts, int numIndices, const MeshDesc_t& desc);

  void ValidateData(int numVerts, int numIndices, const MeshDesc_t& desc);

  // gets the associated material
  IMaterial* GetMaterial();

  void SetColorMesh(IMesh* pColorMesh, int nVertexOffset) {}

  int IndexCount() const override { return 0; }

  MaterialIndexFormat_t IndexFormat() const override {
    Assert(0);
    return MATERIAL_INDEX_FORMAT_UNKNOWN;
  }

  void SetFlexMesh(IMesh*, int) override {}

  void DisableFlexMesh() override {}

  void MarkAsDrawn() override {}

  unsigned ComputeMemoryUsed() override { return 0; }

  VertexFormat_t GetVertexFormat() const override { return VERTEX_POSITION; }

  IMesh* GetMesh() override { return this; }

 private:
  enum { VERTEX_BUFFER_SIZE = 1024 * 1024 };

  unsigned char* m_pVertexMemory;
};

}  // namespace se::shaderapidx11

#endif  // !SE_MATERIALSYSTEM_SHADERAPIDX11_MESH_DX11_H_
