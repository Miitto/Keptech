#pragma once

#include "keptech/core/slotmap.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace keptech {

  struct Vertex {
    glm::vec3 position;
    float uvX;
    glm::vec3 normal;
    float uvY;
    glm::vec4 color;
    glm::vec4 tangent;

    constexpr inline static Vertex create(glm::vec3 pos, glm::vec2 uv,
                                          glm::vec3 norm, glm::vec4 col,
                                          glm::vec4 tang = glm::vec4(0.0f)) {
      return Vertex{.position = pos,
                    .uvX = uv.x,
                    .normal = norm,
                    .uvY = uv.y,
                    .color = col,
                    .tangent = tang};
    }
  };

  class Mesh {
  public:
    struct Submesh {
      uint32_t indexCount;
      uint32_t indexOffset;
    };

    [[nodiscard]] const std::vector<Submesh>& getSubmeshes() const {
      return submeshes;
    }

    [[nodiscard]] uint32_t getIndexOffset() const { return indexOffset; }

#ifdef KT_ADD_RESOURCE_INFO
    std::string& getDebugName() { return name; }
    [[nodiscard]] size_t getVertexCount() const { return vertexCount; }
    [[nodiscard]] size_t getIndexCount() const { return indexCount; }
#endif

    Mesh(uint32_t indexOffset, std::vector<Submesh> submeshes
#ifdef KT_ADD_RESOURCE_INFO
         ,
         std::string name,
         size_t vertexCount, // NOLINT
         size_t indexCount
#endif
         )
        : indexOffset(indexOffset), submeshes(std::move(submeshes))
#ifdef KT_ADD_RESOURCE_INFO
          ,
          name(std::move(name)), vertexCount(vertexCount),
          indexCount(indexCount)
#endif
    {
    }

  protected:
    uint32_t indexOffset;
    std::vector<Submesh> submeshes;

#ifdef KT_ADD_RESOURCE_INFO
    std::string name;
    size_t vertexCount;
    size_t indexCount;
#endif
  };

  using MeshPtr = std::shared_ptr<Mesh>;

  struct MeshData {
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices = {};
    std::vector<Mesh::Submesh> submeshes = {};
  };
} // namespace keptech
