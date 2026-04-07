#pragma once

#include <vulkan/vulkan.h>

namespace kt::vkh::limits {
  extern VkDeviceSize minUniformBufferOffsetAlignment;
  extern VkDeviceSize minStorageBufferOffsetAlignment;
  extern VkDeviceSize maxPushConstantsSize;
} // namespace kt::vkh::limits
