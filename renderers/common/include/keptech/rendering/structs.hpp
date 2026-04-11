#pragma once

#include <glm/vec3.hpp>

namespace kt::rendering {
  struct PointLight {
    glm::vec3 position;
    float radius;
    glm::vec3 color;
    float intensity;
  };
} // namespace kt::rendering
