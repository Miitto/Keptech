#pragma once

#include <glm/glm.hpp>

namespace keptech::components {
  struct PointLight {
    glm::vec3 color;
    float intensity;
    float radius;
  };
} // namespace keptech::components
