#pragma once

#include <Volk/volk.h>
#include <expected>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace kt::vkh {

  class PhysicalDeviceSelector {
  public:
    struct DeviceSpecs {
      VkPhysicalDevice device;
      VkPhysicalDeviceProperties properties;
      VkPhysicalDeviceFeatures features;
      VkPhysicalDeviceMemoryProperties memoryProperties;
      std::vector<VkQueueFamilyProperties> queueFamilyProperties;
      std::vector<VkExtensionProperties> availableExtensions;

      uint32_t score = 0;
    };

    static auto create(const VkInstance& instance) -> std::expected<PhysicalDeviceSelector, std::string>;
    void requireExtensions(const std::span<const char* const> extensions) noexcept;
    void requireFeatures(const std::function<bool(const VkPhysicalDeviceFeatures&)>& featureCheck) noexcept;
    void requireQueueFamily(VkQueueFlags queueFlags) noexcept;
    void requireMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) noexcept;
    void requireVersion(uint32_t major, uint32_t minor, uint32_t patch) noexcept;
    void scoreDevices(const std::function<uint32_t(const vkh::PhysicalDeviceSelector::DeviceSpecs&)>& scoreFn) noexcept;
    auto select() -> VkPhysicalDevice;

  private:
    std::vector<DeviceSpecs> physicalDevices;

    PhysicalDeviceSelector(std::vector<DeviceSpecs>& specs) noexcept : physicalDevices(std::move(specs)) {}
  };
} // namespace kt::vkh
