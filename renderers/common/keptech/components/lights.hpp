#pragma once

#include "keptech/rhi/image.hpp"
#include "keptech/rhi/interface.hpp"
#include <glm/glm.hpp>

namespace kt::components {
  struct PointLight {
    glm::vec3 color{1.f, 1.f, 1.f};
    float intensity = 1.f;
    float radius = 1.f;
    rdr::Image shadowMap{};
  };
} // namespace kt::components
