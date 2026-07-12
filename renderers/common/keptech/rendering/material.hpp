#pragma once

#include "keptech/rendering/interface.hpp"
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace kt {
  namespace rendering {
    struct MaterialLayer {
      Image albedo;
      Image bump;
      Image emissive;
      Image metRough;
      glm::vec4 albedoFactor;
      glm::vec3 emissiveFactor;
      Image ao;
      float metFactor;
      float roughFactor;
      float specFactor = 1.f;
      float alphaCutoff = 0.f;
    };

    using Material = uint32_t;
  } // namespace rendering

  namespace components {
    using Material = rendering::Material;
  }
} // namespace kt
