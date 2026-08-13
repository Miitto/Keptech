#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace kt::rhi {
  class Image;
} // namespace kt::rhi

namespace kt {
  struct MaterialLayer {
    const rdr::Image* albedo;
    const rdr::Image* bump;
    const rdr::Image* emissive;
    const rdr::Image* metRough;
    const rdr::Image* ao;
    glm::vec4 albedoFactor;
    glm::vec3 emissiveFactor;
    float metFactor = 0.f;
    float roughFactor = 0.5f;
    float specFactor = 1.f;
    float alphaCutoff = 0.f;
  };

  using Material = uint32_t;

  namespace components {
    using Material = Material;
  }
} // namespace kt
