#pragma once

#include "keptech/core/slotmap.hpp"
#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace keptech::core::rendering {
  class Mesh {
  public:
    struct Handle {
      SlotMapSmartHandle handle;

      bool operator==(const Handle& other) const {
        return handle == other.handle;
      }

      bool operator==(const SlotMapHandle other) const {
        return handle == other;
      }
    };
    struct WeakHandle {
      SlotMapWeakHandle handle;
    };

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

    struct UnpackedVertex {
      glm::vec3 position;
      glm::vec2 uv;
      glm::vec3 normal;
      glm::vec4 color;

      operator Vertex() const {
        return Vertex::create(position, uv, normal, color);
      }
    };

    struct Submesh {
      uint32_t indexCount;
      uint32_t indexOffset;
    };

    std::string& getName() { return name; }
    [[nodiscard]] const std::vector<Submesh>& getSubmeshes() const {
      return submeshes;
    }
    [[nodiscard]] size_t getVertexCount() const { return vertexCount; }
    [[nodiscard]] size_t getIndexCount() const { return indexCount; }

    Mesh(std::string name, std::vector<Submesh>&& submeshes,
         size_t vertexCount, // NOLINT
         size_t indexCount)
        : name(std::move(name)), submeshes(std::move(submeshes)),
          vertexCount(vertexCount), indexCount(indexCount) {}

  protected:
    std::string name;
    std::vector<Submesh> submeshes;
    size_t vertexCount;
    size_t indexCount;
  };

  struct MeshData {
    std::string name;
    std::vector<rendering::Mesh::Vertex> vertices;
    std::vector<uint32_t> indices = {};
    std::vector<rendering::Mesh::Submesh> submeshes = {};
  };
} // namespace keptech::core::rendering
