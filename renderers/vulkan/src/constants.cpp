#include "keptech/vulkan/constants.hpp"
#include <vulkan/vulkan_raii.hpp>

namespace kt::vkh {
  namespace limits {
    VkDeviceSize minUniformBufferOffsetAlignment = 0;
    VkDeviceSize minStorageBufferOffsetAlignment = 0;
    VkDeviceSize maxPushConstantsSize = 0;
    VkDeviceSize maxMemoryAllocationSize = 0;
  } // namespace limits

  namespace ext {
    PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT vkGetPhysicalDeviceCalibrateableTimeDomainsEXT = nullptr;
    PFN_vkGetCalibratedTimestampsEXT vkGetCalibratedTimestampsEXT = nullptr;
  } // namespace ext
} // namespace kt::vkh
