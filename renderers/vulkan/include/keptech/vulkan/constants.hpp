#pragma once

#include <vulkan/vulkan.h>

namespace kt::vkh::limits {
  extern VkDeviceSize minUniformBufferOffsetAlignment;
  extern VkDeviceSize minStorageBufferOffsetAlignment;
  extern VkDeviceSize maxPushConstantsSize;
  extern VkDeviceSize maxMemoryAllocationSize;
} // namespace kt::vkh::limits
