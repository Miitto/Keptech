#pragma once

#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "keptech/maths/sphere.hpp"

namespace kt {
  namespace constants {
    constexpr size_t VERTICES_PER_MESHLET = 64;
    constexpr size_t PRIMITIVES_PER_MESHLET = 64;
  } // namespace constants

  struct VertexAttribs {
    glm::vec3 normal;
    uint32_t packedUv;
    glm::vec4 tangent;

    glm::vec2 uv() const { return glm::unpackHalf2x16(packedUv); }
    VertexAttribs& setUv(glm::vec2 uv) {
      packedUv = glm::packHalf2x16(uv);
      return *this;
    }

    VertexAttribs() = default;
    VertexAttribs(glm::vec3 normal, glm::vec2 uv, glm::vec4 tangent) : normal(normal), packedUv(glm::packHalf2x16(uv)), tangent(tangent) {}
  };

  struct Meshlet {
    uint32_t vertexOffset;
    uint32_t vertexCount;
    uint32_t triangleOffset;
    uint32_t triangleCount;
    kt::maths::Sphere boundingSphere;
  };

  struct Submesh {
    /// Number of indices in the submesh.
    uint32_t indexCount;
    /// Index of the first index in the submesh in the global index buffer.
    uint32_t indexOffset;
    /// Index of the first vertex in the submesh in the global vertex buffer.
    int32_t vertexOffset;
    /// Index of the first meshlet in the submesh in the global meshlet buffer.
    uint32_t meshletOffset;
    /// Number of meshlets in the submesh
    uint32_t meshletCount;
    /// Index of the first meshlet vertex in the submesh in the global meshlet vertex buffer
    uint32_t meshletVertexOffset;
    /// Index of the first meshlet triangle in the submesh in the global meshlet triangle buffer
    uint32_t meshletTriangleOffset;

    uint32_t vertexCount;
    uint32_t meshletVertexCount;
    uint32_t meshletTriangleCount;

    /// Index of the material in the global material buffer
    uint32_t materialIndex = ~0u;
    kt::maths::Sphere boundingSphere;
    uint32_t id = ~0u;
  };

  class Mesh {
  public:
    [[nodiscard]] bool isValid() const { return !submeshes.empty(); }
    [[nodiscard]] const std::vector<Submesh>& getSubmeshes() const { return submeshes; }
    [[nodiscard]] const std::string& getName() const { return name; }

    Mesh() = default;
    Mesh(std::string name, std::vector<Submesh> submeshes) : name(std::move(name)), submeshes(std::move(submeshes)) {}

  private:
    std::string name;
    std::vector<Submesh> submeshes;
  };

  struct GpuSubmesh {
    uint32_t indexCount;
    uint32_t indexOffset;
    int32_t vertexOffset;
    uint32_t meshletOffset;
    uint32_t meshletCount;
    uint32_t meshletVertexOffset;
    uint32_t meshletTriangleOffset;

    uint32_t vertexCount;
    uint32_t meshletVertexCount;
    uint32_t meshletTriangleCount;

    uint32_t materialIndex = ~0u;
    kt::maths::Sphere boundingSphere;
  };
} // namespace kt