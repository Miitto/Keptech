#pragma once

#include "keptech/maths/sphere.hpp"
#include "renderer/types.hpp"
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

  struct GpuMesh {
    uint32_t indexOffset;
    uint32_t indexCount;
    int32_t vertexOffset;
    uint32_t vertexCount;
    uint32_t meshletOffset;
    uint32_t meshletCount;
    uint32_t meshletVertexOffset;
    uint32_t meshletVertexCount;
    uint32_t meshletTriangleOffset;
    uint32_t meshletTriangleCount;
    maths::Sphere boundingSphere;
  };

  struct GpuObject {
    glm::mat4 model;
    uint32_t meshIndex;
    uint32_t materialIndex;
    float pad1, pad2;
  };

  struct GpuPointLight {
    glm::vec3 position;
    float radius;
    glm::vec3 color;
    ImageHandle shadowMap;
  };
} // namespace kt::rdr