#pragma once

#include <Volk/volk.h>
#include <expected>
#include <span>
#include <string>

namespace kt::rdr {

  class Instance {
  public:
    Instance() = default;
    operator VkInstance() const { return handle; }

    void destroy() {
#ifndef NDEBUG
      if (debugMessenger) {
        vkDestroyDebugUtilsMessengerEXT(handle, debugMessenger, nullptr);
      }
#endif
      vkDestroyInstance(handle, nullptr);
    }

    static auto create(const char* appName, const bool enableValidationLayers, const std::span<const char* const> extraExtensions = {},
                       const std::span<const char* const> extraLayers = {}) -> std::expected<Instance, std::string>;

  private:
    Instance(VkInstance handle
#ifndef NDEBUG
             ,
             VkDebugUtilsMessengerEXT debugMessenger
#endif
             )
        : handle(handle)
#ifndef NDEBUG
          ,
          debugMessenger(debugMessenger)
#endif
    {
    }

    VkInstance handle = nullptr;
#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debugMessenger = nullptr;
#endif
  };
} // namespace kt::rdr
