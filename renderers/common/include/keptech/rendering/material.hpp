#pragma once

#include "texture.hpp"

namespace kt {
  namespace rendering {
    struct MaterialLayer {
      Texture albedo;
      Texture bump;
      Texture emissive;
      Texture metRough;
      glm::vec4 albedoFactor;
      glm::vec3 emissiveFactor;
      Texture ao;
      float metFactor;
      float roughFactor;
      float specFactor = 1.f;
      float alphaCutoff = 0.f;
    };

    using Material = vkh::AddressedAllocatedBuffer;
  } // namespace rendering

  namespace components {
    using Material = rendering::Material;
  }
} // namespace kt
