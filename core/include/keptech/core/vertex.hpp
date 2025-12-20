#pragma once

namespace keptech::core {
  struct Vertex {
    glm::vec3 position;
    float uvX;
    glm::vec3 normal;
    float uvY;
    glm::vec4 color;

    constexpr inline static Vertex create(glm::vec3 pos, glm::vec2 uv,
                                          glm::vec3 norm, glm::vec4 col) {
      return Vertex{
          .position = pos,
          .uvX = uv.x,
          .normal = norm,
          .uvY = uv.y,
          .color = col,
      };
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
} // namespace keptech::core
