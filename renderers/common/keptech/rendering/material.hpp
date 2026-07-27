#pragma once

#include "keptech/rendering/interface.hpp"
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace kt {
  struct MaterialLayer {
    const Image* albedo;
    const Image* bump;
    const Image* emissive;
    const Image* metRough;
    glm::vec4 albedoFactor;
    glm::vec3 emissiveFactor;
    const Image* ao;
    float metFactor;
    float roughFactor;
    float specFactor = 1.f;
    float alphaCutoff = 0.f;
  };

  using Material = uint32_t;

  namespace components {
    using Material = Material;
  }
} // namespace kt
