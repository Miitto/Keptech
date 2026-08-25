#pragma once

namespace kt::components {
  struct PointLight {
    glm::vec3 color;
    float intensity;
    float radius;
  };
} // namespace kt::components