#pragma once

#include "keptech/rendering/interface.hpp"
#include <glm/glm.hpp>

namespace kt::components {
  struct PointLight {
    glm::vec3 color{1.f, 1.f, 1.f};
    float intensity = 1.f;
    float radius = 1.f;
    rendering::Image shadowMap{};
  };
} // namespace kt::components
