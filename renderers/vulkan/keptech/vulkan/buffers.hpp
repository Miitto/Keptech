#pragma once

#include "keptech/vulkan/constants.hpp"
#include "keptech/vulkan/helpers/owned.hpp"
#include "keptech/vulkan/wrappers/buffer.hpp"
#include <Volk/volk.h>
#include <array>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace kt {
  struct VertexAttribs;
  struct Meshlet;

  namespace vkh {
    struct GpuMaterial;
    struct GpuObject;
    struct GpuPointLight;

    struct PerFrameBuffers {
      template <typename T> using SB = Owned<SubdivBuffer<T>>;
      SB<GpuObject> objects;
      SB<GpuPointLight> pointLights;
      SB<glm::mat4> shadowMatrices;
    };

    struct Buffers {
      using B = Owned<Buffer>;
      template <typename T> using SB = Owned<SubdivBuffer<T>>;
      // All buffers need to be initialized otherwise the device and allocator in `Owned` will be null even after being allocated by other
      // functions. Ensure the owned's are inited with the device and allocator of the vkcore.
      B camera;
      B addresses;
      B ssaoKernel;
      SB<uint32_t> indices;
      SB<glm::vec3> vertexPositions;
      SB<VertexAttribs> vertexAttribs;
      SB<Meshlet> meshlets;
      SB<uint32_t> meshletVertices;
      SB<uint32_t> meshletTriangles;
      SB<GpuMaterial> materials;
      std::array<PerFrameBuffers, MAX_FRAMES_IN_FLIGHT> perFrame;
    };

    struct BufferPointers {
      VkDeviceAddress vertexPositions;
      VkDeviceAddress vertexAttribs;
      VkDeviceAddress indices;
      VkDeviceAddress meshlets;
      VkDeviceAddress meshletVertices;
      VkDeviceAddress meshletTriangles;
      VkDeviceAddress materials;
    };
  } // namespace vkh
} // namespace kt