#pragma once

#include "keptech/material.hpp"
#include "keptech/mesh.hpp"
#include "keptech/subdivBuffer.hpp"

namespace kt {
  class Buffers {
    template <typename T> using SB = kt::SubdivBuffer<T>;

  public:
    static Buffers& get() { return instance; }

    SB<glm::vec3> positions;
    SB<kt::VertexAttribs> vertexAttribs;
    SB<uint32_t> indices;
    SB<kt::Meshlet> meshlets;
    SB<uint32_t> meshletVertices;
    SB<uint32_t> meshletTriangles;
    SB<kt::GpuSubmesh> submeshes;
    SB<kt::GpuMaterial> materials;

  private:
    static Buffers instance;
  };
} // namespace kt