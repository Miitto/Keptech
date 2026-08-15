#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace kt::rhi {
  class Image;
} // namespace kt::rhi

namespace kt {
  struct Material {
    const rhi::Image* albedo;
    const rhi::Image* bump;
    const rhi::Image* emissive;
    const rhi::Image* metRough;
    const rhi::Image* ao;
    glm::vec4 albedoFactor;
    glm::vec3 emissiveFactor;
    float metFactor = 0.f;
    float roughFactor = 0.5f;
    float specFactor = 1.f;
    float alphaCutoff = 0.f;
  };

  namespace components {
    using Material = Material;
  }

  struct GpuMaterial {
    uint64_t albedo = 0;
    uint64_t bump = 0;
    uint64_t emissive = 0;
    uint64_t metRough = 0;
    uint64_t ao = 0;
    glm::vec4 albedoFactor;
    glm::vec3 emissiveFactor;
    float metFactor = 0.f;
    float roughFactor = 0.5f;
    float specFactor = 1.f;
    float alphaCutoff = 0.f;
  };
} // namespace kt
