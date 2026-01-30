#pragma once

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
    glm::vec4 color{1.f};
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
    [[nodiscard]] uint32_t getVertexOffset() const { return vertexOffset; }
    [[nodiscard]] size_t getIndexCount() const { return indexCount; }
    [[nodiscard]] uint32_t getIndexOffset() const { return indexOffset; }

    Mesh& setVertexOffset(uint32_t offset) {
      vertexOffset = offset;
      return *this;
    }
    Mesh& setIndexOffset(uint32_t offset) {
      indexOffset = offset;
      return *this;
    }

#ifdef KT_ADD_RESOURCE_INFO
    std::string& getDebugName() { return name; }
    [[nodiscard]] size_t getVertexCount() const { return vertexCount; }
#endif

    Mesh(uint32_t vertexOffset, uint32_t indexCount, uint32_t indexOffset
#ifdef KT_ADD_RESOURCE_INFO
         ,
         std::string name, size_t vertexCount
#endif
         )
        : vertexOffset(vertexOffset), indexCount(indexCount),
          indexOffset(indexOffset)
#ifdef KT_ADD_RESOURCE_INFO
          ,
          name(std::move(name)), vertexCount(vertexCount)
#endif
    {
    }

  protected:
    uint32_t vertexOffset;
    uint32_t indexCount;
    uint32_t indexOffset;

#ifdef KT_ADD_RESOURCE_INFO
    std::string name;
    size_t vertexCount;
#endif
  };

  using MeshPtr = std::shared_ptr<Mesh>;

  struct MeshData {
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices = {};
  };
} // namespace keptech
