#pragma once

#include "keptech/rhi/constants.hpp"
#include <Volk/volk.h>

namespace kt::rhi::constants {
#ifdef KT_USE_DESCRIPTOR_HEAP
  extern VkDeviceSize samplerDescriptorSize; // NOLINT
  extern VkDeviceSize bufferDescriptorSize;  // NOLINT
  extern VkDeviceSize imageDescriptorSize;   // NOLINT
#endif

  extern VkDeviceSize minUniformBufferOffsetAlignment; // NOLINT
  extern VkDeviceSize minStorageBufferOffsetAlignment; // NOLINT
  extern VkDeviceSize maxPushConstantsSize;            // NOLINT
  extern VkDeviceSize maxMemoryAllocationSize;         // NOLINT
} // namespace kt::rhi::constants
