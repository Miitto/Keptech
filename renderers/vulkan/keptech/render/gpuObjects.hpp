#pragma once

#include "types.hpp"
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace kt::rdr {
  struct GpuMaterial {
    ImageHandle albedo = ~0u;
    ImageHandle bump = ~0u;
    ImageHandle emissive = ~0u;
    ImageHandle metRough = ~0u;
    ImageHandle ao = ~0u;
    glm::vec4 albedoFactor = glm::vec4(1.0f);
    glm::vec3 emissiveFactor = glm::vec3(0.0f);
    float metFactor = 1.0f;
    float roughFactor = 1.0f;
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
    ImageHandle shadowMap;
  };
} // namespace kt::rdr