#include "keptech/render/helpers/validators.hpp"

#include "vk-logger.hpp"

namespace kt::rdr {
  void printExtensions(spdlog::level::level_enum logLevel) {
    if (logLevel < RENDERER_LOG_LEVEL) {
      return;
    }
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    logger->log(logLevel, "Available Vulkan Extensions:");
    for (const auto& ext : extensions) {
      logger->log(logLevel, "  - {}", ext.extensionName);
    }
  }

  std::vector<const char*> checkExtensions(std::span<const char*> extensions) {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());
    std::vector<const char*> missingExtensions{};

    for (const auto& ext : extensions) {
      if (std::ranges::find_if(availableExtensions, [&ext](const VkExtensionProperties& availableExt) {
            return strcmp(availableExt.extensionName, ext) == 0;
          }) == availableExtensions.end()) {
        missingExtensions.push_back(ext);
      }
    }
    return missingExtensions;
  }

  std::vector<const char*> checkLayers(std::span<const char*> layers) {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    std::vector<const char*> missingLayers{};

    for (const auto& layer : layers) {
      if (std::ranges::find_if(availableLayers, [&layer](const VkLayerProperties& availableLayer) {
            return strcmp(availableLayer.layerName, layer) == 0;
          }) == availableLayers.end()) {
        missingLayers.push_back(layer);
      }
    }
    return missingLayers;
  }

} // namespace kt::rdr
