#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace kt::vkh {
  struct GpuMaterial {
    uint32_t albedo;
    uint32_t bump;
    uint32_t emissive;
    uint32_t metRough;
    glm::vec4 albedoFactor;
    glm::vec3 emissiveFactor;
    uint32_t ao;
    float metFactor;
    float roughFactor;
    float specFactor = 1.f;
    float alphaCutoff = 0.f;
  };

  struct GpuObject {
    glm::mat4 model;
    uint32_t materialIndex;
    float pad1, pad2, pad3;
  };

  struct GpuPointLight {
    glm::vec3 position;
    float radius;
    glm::vec3 color;
    uint32_t shadowMapIndex;
  };
} // namespace kt::vkh