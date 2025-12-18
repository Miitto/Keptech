#include "keptech/vulkan/helpers/physicalDeviceSelector.hpp"

#include "vk-logger.hpp"
#include <macros.hpp>

namespace keptech::vkh {
  auto PhysicalDeviceSelector::create(const vk::raii::Instance& instance)
      -> std::expected<PhysicalDeviceSelector, std::string> {

    VK_MAKE(devices, instance.enumeratePhysicalDevices(),
            "Failed to enumerate physical devices");

    if (devices.empty()) {
      return std::unexpected("No physical devices found");
    }

    auto specs = std::vector<DeviceSpecs>();
    specs.reserve(devices.size());

    for (const auto& device : devices) {
      VK_MAKE(properties, device.enumerateDeviceExtensionProperties(),
              "Failed to get physical device properties");
      DeviceSpecs spec{.device = device,
                       .properties = device.getProperties(),
                       .features = device.getFeatures(),
                       .memoryProperties = device.getMemoryProperties(),
                       .queueFamilyProperties =
                           device.getQueueFamilyProperties(),
                       .availableExtensions = properties};

      specs.push_back(std::move(spec));
    }

    PhysicalDeviceSelector selector(specs);

    return selector;
  }

  void PhysicalDeviceSelector::requireExtensions(
      const std::span<const char* const> extensions) noexcept {
    VK_DEBUG("Requiring extensions:");
    for (const auto& ext : extensions) {
      VK_DEBUG("  - {}", ext);
    }

    VK_TRACE("Filtering {} devices", physicalDevices.size());
    for (size_t i = physicalDevices.size() - 1; i != (~(size_t)0); --i) {
      VK_TRACE("Checking device {} for required extensions", i);
      const auto& device = physicalDevices[i];

      for (const auto& ext : extensions) {
        if (std::ranges::find_if(
                device.availableExtensions,
                [&ext](const vk::ExtensionProperties& availableExt) {
                  return strcmp(availableExt.extensionName, ext) == 0;
                }) == device.availableExtensions.end()) {
          physicalDevices.erase(physicalDevices.begin() +
                                static_cast<std::ptrdiff_t>(i));
          break;
        }
      }
    }
  }

  void PhysicalDeviceSelector::requireFeatures(
      const std::function<bool(const vk::PhysicalDeviceFeatures&)>&
          featureCheck) noexcept {
    VK_DEBUG("Requiring features");
    for (size_t i = physicalDevices.size() - 1; i != (~(size_t)0); --i) {
      const auto& device = physicalDevices[i];

      if (!(featureCheck(device.features))) {
        physicalDevices.erase(physicalDevices.begin() +
                              static_cast<std::ptrdiff_t>(i));
      }
    }
  }

  void PhysicalDeviceSelector::requireQueueFamily(
      vk::Flags<vk::QueueFlagBits> queueFlags) noexcept {
    VK_DEBUG("Requiring queue family with flags: {}",
             vk::to_string(queueFlags));

    VK_TRACE("Filtering {} devices", physicalDevices.size());
    for (size_t i = physicalDevices.size() - 1; i != (~(size_t)0); --i) {
      VK_TRACE("Checking device {} for required queue family", i);
      const auto& device = physicalDevices[i];

      bool hasRequiredQueue = false;
      for (const auto& queueFamily : device.queueFamilyProperties) {
        if (queueFamily.queueFlags & queueFlags) {
          hasRequiredQueue = true;
          break;
        }
      }

      if (!hasRequiredQueue) {
        physicalDevices.erase(physicalDevices.begin() +
                              static_cast<std::ptrdiff_t>(i));
      }
    }
  }

  void PhysicalDeviceSelector::requireMemoryType(
      uint32_t typeBits, vk::MemoryPropertyFlags properties) noexcept {
    VK_TRACE("Requiring memory type with type bits: {}, properties: {}",
             typeBits, vk::to_string(properties));
    for (size_t i = physicalDevices.size() - 1; i != (~(size_t)0); --i) {
      const auto& device = physicalDevices[i];

      bool hasRequiredMemoryType = false;
      for (uint32_t j = 0; j < device.memoryProperties.memoryTypeCount; ++j) {
        if ((typeBits & (1 << j)) &&
            (device.memoryProperties.memoryTypes[j].propertyFlags &
             properties) == properties) {
          hasRequiredMemoryType = true;
          break;
        }
      }

      if (!hasRequiredMemoryType) {
        physicalDevices.erase(physicalDevices.begin() +
                              static_cast<std::ptrdiff_t>(i));
      }
    }
  }

  void PhysicalDeviceSelector::requireVersion(uint32_t major, uint32_t minor,
                                              uint32_t patch) noexcept {
    VK_DEBUG("Requiring API version: {}.{}.{}", major, minor, patch);
    auto version = VK_MAKE_VERSION(major, minor, patch);
    for (size_t i = physicalDevices.size() - 1; i > 0; --i) {
      const auto& device = physicalDevices[i];

      if (device.properties.apiVersion < version) {
        physicalDevices.erase(physicalDevices.begin() +
                              static_cast<std::ptrdiff_t>(i));
      }
    }
  }

  void PhysicalDeviceSelector::scoreDevices(
      const std::function<uint32_t(
          const vkh::PhysicalDeviceSelector::DeviceSpecs&)>& scoreFn) noexcept {
    VK_DEBUG("Scoring devices");
    for (auto& device : physicalDevices) {
      device.score = scoreFn(device);
    }
  }

  void PhysicalDeviceSelector::sortDevices() noexcept {
    VK_DEBUG("Sorting devices by score");
    std::ranges::sort(physicalDevices, [](DeviceSpecs& a, DeviceSpecs& b) {
      return a.score > b.score;
    });
  }

  auto PhysicalDeviceSelector::select()
      -> std::vector<vk::raii::PhysicalDevice> {
    VK_DEBUG("Selecting devices");
    std::vector<vk::raii::PhysicalDevice> devices;
    devices.reserve(physicalDevices.size());
    for (const auto& spec : physicalDevices) {
      devices.push_back(spec.device);
    }
    return devices;
  }

} // namespace keptech::vkh
