#pragma once

#include "keptech/rendering/interface.hpp"
#include "keptech/rendering/material.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <keptech/maths/sphere.hpp>
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

  struct Submesh {
    int32_t vertexOffset;
    uint32_t start;
    uint32_t count;
    std::optional<rendering::Material> material;
    kt::maths::Sphere boundingSphere;
    uint32_t id = ~0u;
  };

  class Mesh {
  public:
    [[nodiscard]] bool isValid() const { return vertexCount > 0 && indexCount > 0; }
    [[nodiscard]] uint32_t getVertexCount() const { return vertexCount; }
    [[nodiscard]] uint32_t getIndexCount() const { return indexCount; }
    [[nodiscard]] const std::vector<Submesh>& getSubmeshes() const { return submeshes; }
    [[nodiscard]] const rendering::RendererMesh& getRMesh() const { return rMesh; }
    rendering::RendererMesh& getRMesh() { return rMesh; }

#ifdef KT_ADD_RESOURCE_INFO
    std::string& getDebugName() { return name; }
#endif

    Mesh() = default;
    Mesh(uint32_t vertexCount, uint32_t indexCount, rendering::RendererMesh rData, std::vector<Submesh> submeshes, std::string name = {})
        : vertexCount(vertexCount), indexCount(indexCount), rMesh(std::move(rData)), submeshes(std::move(submeshes))
#ifdef KT_ADD_RESOURCE_INFO
          ,
          name(std::move(name))
#endif
    {
    }

  protected:
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    rendering::RendererMesh rMesh;
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
    std::vector<uint32_t> indices = {};
  };
} // namespace kt
