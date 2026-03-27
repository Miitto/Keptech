#pragma once

#include <vulkan/vulkan.h>

namespace kt::vkh {
  struct Device {
    VkPhysicalDevice physical;
    VkDevice logical;

    operator VkPhysicalDevice&() { return physical; }
    operator VkDevice&() { return logical; }

    void destroy() {
      if (logical) {
        vkDestroyDevice(logical, nullptr);
        logical = VK_NULL_HANDLE;
      }
    }
  };
} // namespace kt::vkh
