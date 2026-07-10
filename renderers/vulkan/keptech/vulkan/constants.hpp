#pragma once

#include "keptech/rendering/constants.hpp"
#include <Volk/volk.h>

namespace kt::vkh {
  namespace constants {
    constexpr uint32_t SHADOW_MAP_SIZE = 4096;
    constexpr uint32_t SSAO_KERNEL_SIZE = 64;
    constexpr uint32_t SSAO_NOISE_SIZE = 4;

    constexpr uint32_t STATIC_TEXTURE_COUNT = 10; // SSAO Result is used in compute, so is a storage image.
  } // namespace constants

  namespace limits {
    extern VkDeviceSize minUniformBufferOffsetAlignment;
    extern VkDeviceSize minStorageBufferOffsetAlignment;
    extern VkDeviceSize maxPushConstantsSize;
    extern VkDeviceSize maxMemoryAllocationSize;
  } // namespace limits
} // namespace kt::vkh
