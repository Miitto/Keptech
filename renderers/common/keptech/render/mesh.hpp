#pragma once

#include "keptech/render/material.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <keptech/maths/sphere.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace kt {

  struct VertexAttribs {
    glm::vec4 tangent{1.f, 0.f, 0.f, 1.f};
    glm::vec3 normal;
    uint32_t encodedUv;

    constexpr static uint32_t encodeUv(glm::vec2 uv) { return glm::packHalf2x16(uv); }

    void setUv(glm::vec2 uv) { encodedUv = encodeUv(uv); }
  };

  struct Meshlet {
    /// Offset into the meshlet vertex buffer relative to the submesh start
    uint32_t vertexOffset;
    /// Number of vertices in the meshlet
    uint32_t vertexCount;
    /// Offset into the meshlet triangle buffer relative to the submesh start
    uint32_t triangleOffset;
    /// Number of triangles in the meshlet
    uint32_t triangleCount;
    /// Meshlet bounding sphere in object space
    kt::maths::Sphere boundingSphere;
  };

  struct Submesh {
    uint32_t indexOffset;
    /// Offset of the first vertex in the submesh in the global vertex buffer
    int32_t vertexOffset;
    /// Offset of the first meshlet in the submesh in the global meshlet buffer
    uint32_t meshletOffset;
    /// Number of meshlets in the submesh
    uint32_t meshletCount;
    /// Offset of the first meshlet vertex in the submesh in the global meshlet vertex buffer
    uint32_t meshletVertexOffset;
    /// Offset of the first meshlet triangle in the submesh in the global meshlet triangle buffer
    uint32_t meshletTriangleOffset;

    uint32_t vertexCount;
    uint32_t meshletVertexCount;
    uint32_t meshletTriangleCount;

    std::optional<Material> material;
    kt::maths::Sphere boundingSphere;
    uint32_t id = ~0u;
  };

  class Mesh {
  public:
    [[nodiscard]] bool isValid() const { return vertexCount > 0; }
    [[nodiscard]] uint32_t getVertexCount() const { return vertexCount; }
    [[nodiscard]] const std::vector<Submesh>& getSubmeshes() const { return submeshes; }

#ifdef KT_ADD_RESOURCE_INFO
    std::string& getDebugName() { return name; }
#endif

    Mesh() = default;
    Mesh(uint32_t vertexCount, std::vector<Submesh> submeshes, std::string name = {})
        : vertexCount(vertexCount), submeshes(std::move(submeshes))
#ifdef KT_ADD_RESOURCE_INFO
          ,
          name(std::move(name))
#endif
    {
    }

  protected:
    uint32_t vertexCount = 0;
    std::vector<Submesh> submeshes;

#ifdef KT_ADD_RESOURCE_INFO
    std::string name;
#endif
  };

  namespace components {
    using Mesh = Mesh;
  }

  struct MeshData {
    std::string name;
    std::vector<glm::vec3> positions;
    std::vector<VertexAttribs> vertexAttribs;
  };
} // namespace kt
