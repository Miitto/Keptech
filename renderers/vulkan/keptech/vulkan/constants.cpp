#include "keptech/vulkan/constants.hpp"
#include <Volk/volk.h>

namespace kt::vkh {
  namespace limits {
    VkDeviceSize minUniformBufferOffsetAlignment = 0;
    VkDeviceSize minStorageBufferOffsetAlignment = 0;
    VkDeviceSize maxPushConstantsSize = 0;
    VkDeviceSize maxMemoryAllocationSize = 0;
  } // namespace limits
} // namespace kt::vkh
