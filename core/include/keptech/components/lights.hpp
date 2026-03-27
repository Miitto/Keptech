#pragma once

#include <glm/glm.hpp>

namespace kt::components {
  struct PointLight {
    glm::vec3 color{1.f, 1.f, 1.f};
    float intensity = 1.f;
    float radius = 1.f;
  };
} // namespace kt::components
