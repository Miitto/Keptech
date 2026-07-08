#pragma once

#include <Volk/volk.h>

namespace kt::vkh {
  struct Device {
    VkPhysicalDevice physical;
    VkDevice logical;

    operator VkPhysicalDevice() const noexcept { return physical; }
    operator VkDevice() const noexcept { return logical; }

    void destroy() {
      if (logical) {
        vkDestroyDevice(logical, nullptr);
        logical = VK_NULL_HANDLE;
      }
    }
  };
} // namespace kt::vkh
