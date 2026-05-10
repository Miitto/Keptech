#pragma once

#include <Volk/volk.h>
#include <expected>
#include <span>
#include <string>

namespace kt::vkh {

  struct Instance {
    VkInstance handle;
#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debugMessenger;
#endif

    operator VkInstance() const { return handle; }

    void destroy() {
#ifndef NDEBUG
      auto destroyDebugUtilsMessenger =
          (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(handle, "vkDestroyDebugUtilsMessengerEXT");
      if (destroyDebugUtilsMessenger && debugMessenger) {
        destroyDebugUtilsMessenger(handle, debugMessenger, nullptr);
      }
#endif
      vkDestroyInstance(handle, nullptr);
    }
  };

  auto createInstance(const char* appName, const bool enableValidationLayers, const std::span<const char* const> extraExtensions = {},
                      const std::span<const char* const> extraLayers = {}) -> std::expected<Instance, std::string>;
} // namespace kt::vkh
