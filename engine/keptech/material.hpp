#pragma once

#include "keptech/rhi/wrappers/imageRef.hpp"
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace kt::rhi {
  class Image;
  class ImageRef;
} // namespace kt::rhi

namespace kt {
  enum class AlphaMode : uint8_t { Opaque, Mask, Blend };

  struct Material {
    rhi::ImageRef albedo;
    rhi::ImageRef bump;
    rhi::ImageRef emissive;
    rhi::ImageRef metRough;
    rhi::ImageRef ao;
    glm::vec4 albedoFactor;
    glm::vec3 emissiveFactor;
    float metFactor = 0.f;
    float roughFactor = 0.5f;
    float specFactor = 1.f;
    float alphaCutoff = 0.f;
    AlphaMode alphaMode = AlphaMode::Opaque;
    bool doubleSided = false;
    uint32_t id = ~0u;
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
    uint32_t alphaMode = static_cast<uint32_t>(AlphaMode::Opaque);
  };
} // namespace kt
