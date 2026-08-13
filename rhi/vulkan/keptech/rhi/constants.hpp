#pragma once

#include "keptech/rhi/constants.hpp"
#include <Volk/volk.h>

#define MAX_FRAMES_IN_FLIGHT 2

namespace kt::rhi {
  namespace constants {
    constexpr uint32_t SHADOW_MAP_SIZE = 4096;
    constexpr uint32_t SSAO_KERNEL_SIZE = 64;
    constexpr uint32_t SSAO_NOISE_SIZE = 4;

    constexpr uint32_t STATIC_TEXTURE_COUNT = 11;

#ifdef KT_USE_DESCRIPTOR_HEAP
    extern VkDeviceSize samplerDescriptorSize; // NOLINT
    extern VkDeviceSize bufferDescriptorSize;  // NOLINT
    extern VkDeviceSize imageDescriptorSize;   // NOLINT
#endif
  } // namespace constants

  namespace limits {
    extern VkDeviceSize minUniformBufferOffsetAlignment; // NOLINT
    extern VkDeviceSize minStorageBufferOffsetAlignment; // NOLINT
    extern VkDeviceSize maxPushConstantsSize;            // NOLINT
    extern VkDeviceSize maxMemoryAllocationSize;         // NOLINT
  } // namespace limits
} // namespace kt::rhi
