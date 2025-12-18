#pragma once

#include <expected>
#include <functional>
#include <span>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {

  class PhysicalDeviceSelector {
  public:
    struct DeviceSpecs {
      vk::raii::PhysicalDevice device;
      vk::PhysicalDeviceProperties properties;
      vk::PhysicalDeviceFeatures features;
      vk::PhysicalDeviceMemoryProperties memoryProperties;
      std::vector<vk::QueueFamilyProperties> queueFamilyProperties;
      std::vector<vk::ExtensionProperties> availableExtensions;

      uint32_t score = 0;
    };

    static auto create(const vk::raii::Instance& instance)
        -> std::expected<PhysicalDeviceSelector, std::string>;
    void
    requireExtensions(const std::span<const char* const> extensions) noexcept;
    void requireFeatures(
        const std::function<bool(const vk::PhysicalDeviceFeatures&)>&
            featureCheck) noexcept;
    void requireQueueFamily(vk::Flags<vk::QueueFlagBits> queueFlags) noexcept;
    void requireMemoryType(uint32_t typeBits,
                           vk::MemoryPropertyFlags properties) noexcept;
    void requireVersion(uint32_t major, uint32_t minor,
                        uint32_t patch) noexcept;
    void scoreDevices(const std::function<uint32_t(
                          const vkh::PhysicalDeviceSelector::DeviceSpecs&)>&
                          scoreFn) noexcept;
    void sortDevices() noexcept;
    auto select() -> std::vector<vk::raii::PhysicalDevice>;

  private:
    std::vector<DeviceSpecs> physicalDevices;

    PhysicalDeviceSelector(std::vector<DeviceSpecs>& specs) noexcept
        : physicalDevices(std::move(specs)) {}
  };
} // namespace keptech::vkh
