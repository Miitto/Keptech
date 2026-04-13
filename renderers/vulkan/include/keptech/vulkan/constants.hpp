#pragma once

#include <vulkan/vulkan.h>

namespace kt::vkh {
  namespace constants {
    constexpr size_t BLOOM_MIP_LEVELS = 5;
    constexpr uint32_t SHADOW_MAP_SIZE = 4096;
    constexpr uint32_t BLOOM_SOURCE_INDEX = 7u;
    constexpr uint32_t BLOOM_FIRST_MIP_INDEX = 8u;
    constexpr uint32_t FIRST_USER_TEXTURE_INDEX = BLOOM_FIRST_MIP_INDEX + BLOOM_MIP_LEVELS;
  } // namespace constants

  namespace limits {
    extern VkDeviceSize minUniformBufferOffsetAlignment;
    extern VkDeviceSize minStorageBufferOffsetAlignment;
    extern VkDeviceSize maxPushConstantsSize;
    extern VkDeviceSize maxMemoryAllocationSize;
  } // namespace limits
} // namespace kt::vkh
