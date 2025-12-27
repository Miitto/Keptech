#pragma once

#include "keptech/core/slotmap.hpp"

namespace keptech::core::rendering {
  struct Mesh {
    using Handle = SlotMapSmartHandle;

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

    std::string name;
    std::vector<Submesh> submeshes;
  };

  struct MeshData {
    std::string name;
    std::vector<rendering::Mesh::Vertex> vertices;
    std::vector<uint32_t> indices = {};
    std::vector<rendering::Mesh::Submesh> submeshes = {};
  };
} // namespace keptech::core::rendering
