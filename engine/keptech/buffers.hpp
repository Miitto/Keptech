#pragma once

#include "keptech/material.hpp"
#include "keptech/mesh.hpp"
#include "keptech/rhi/descriptorLayout.hpp"
#include "keptech/rhi/descriptorSet.hpp"
#include "keptech/subdivBuffer.hpp"

namespace kt {
  class Buffers {
    template <typename T> using SB = kt::SubdivBuffer<T>;

  public:
    static Buffers& get() { return instance; }
    static void ensureDescriptorLayout();
    static rhi::DescriptorLayout& getLayout() { return layout; }

    SB<glm::vec3> positions;
    SB<kt::VertexAttribs> vertexAttribs;
    SB<uint32_t> indices;
    SB<kt::Meshlet> meshlets;
    SB<uint32_t> meshletVertices;
    SB<uint32_t> meshletTriangles;
    SB<kt::GpuSubmesh> submeshes;
    SB<kt::GpuMaterial> materials;

    rhi::DescriptorSet descriptor;

    Result<SB<glm::vec3>, rhi::RawRhiResult, rhi::RawRhiResultOk> reallocatePositions(size_t newSize);
    Result<SB<kt::VertexAttribs>, rhi::RawRhiResult, rhi::RawRhiResultOk> reallocateVertexAttribs(size_t newSize);
    Result<SB<uint32_t>, rhi::RawRhiResult, rhi::RawRhiResultOk> reallocateIndices(size_t newSize);
    Result<SB<kt::Meshlet>, rhi::RawRhiResult, rhi::RawRhiResultOk> reallocateMeshlets(size_t newSize);
    Result<SB<uint32_t>, rhi::RawRhiResult, rhi::RawRhiResultOk> reallocateMeshletVertices(size_t newSize);
    Result<SB<uint32_t>, rhi::RawRhiResult, rhi::RawRhiResultOk> reallocateMeshletTriangles(size_t newSize);
    Result<SB<kt::GpuSubmesh>, rhi::RawRhiResult, rhi::RawRhiResultOk> reallocateSubmeshes(size_t newSize);
    Result<SB<kt::GpuMaterial>, rhi::RawRhiResult, rhi::RawRhiResultOk> reallocateMaterials(size_t newSize);

  private:
    static Buffers instance;
    static rhi::DescriptorLayout layout;
  };
} // namespace kt