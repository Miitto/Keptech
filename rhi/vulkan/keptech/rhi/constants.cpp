#include "keptech/rhi/constants.hpp"
#include <Volk/volk.h>

namespace kt::rhi {
  namespace limits {
    VkDeviceSize minUniformBufferOffsetAlignment = 0;
    VkDeviceSize minStorageBufferOffsetAlignment = 0;
    VkDeviceSize maxPushConstantsSize = 0;
    VkDeviceSize maxMemoryAllocationSize = 0;
  } // namespace limits

  namespace constants {
#ifdef KT_USE_DESCRIPTOR_HEAP
    VkDeviceSize samplerDescriptorSize = 0;
    VkDeviceSize bufferDescriptorSize = 0;
    VkDeviceSize imageDescriptorSize = 0;
#endif
  } // namespace constants
} // namespace kt::rhi
