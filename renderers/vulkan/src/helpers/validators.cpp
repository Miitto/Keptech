#include "keptech/vulkan/helpers/validators.hpp"

#include "vk-logger.hpp"

namespace keptech::vkh {
  void printExtensions(vk::raii::Context& context,
                       spdlog::level::level_enum logLevel) {
    auto res = context.enumerateInstanceExtensionProperties();
    if (res.result != vk::Result::eSuccess) {
      VK_WARN("Failed to enumerate Vulkan extensions: {}",
              vk::to_string(res.result));
      return;
    }
    auto& extensions = res.value;

    logger->log(logLevel, "Available Vulkan Extensions:");
    for (const auto& ext : extensions) {
      logger->log(logLevel, "  - {}", ext.extensionName.data());
    }
  }

  std::vector<const char*> checkExtensions(vk::raii::Context& context,
                                           std::span<const char*> extensions) {
    auto res = context.enumerateInstanceExtensionProperties();
    if (res.result != vk::Result::eSuccess) {
      VK_WARN("Failed to enumerate Vulkan extensions: {}",
              vk::to_string(res.result));
      return {};
    }

    auto& availableExtensions = res.value;

    std::vector<const char*> missingExtensions{};

    for (const auto& ext : extensions) {
      if (std::ranges::find_if(
              availableExtensions,
              [&ext](const vk::ExtensionProperties& availableExt) {
                return strcmp(availableExt.extensionName, ext) == 0;
              }) == availableExtensions.end()) {
        missingExtensions.push_back(ext);
      }
    }
    return missingExtensions;
  }

  std::vector<const char*> checkLayers(vk::raii::Context& context,
                                       std::span<const char*> layers) {
    auto res = context.enumerateInstanceLayerProperties();
    if (res.result != vk::Result::eSuccess) {
      VK_WARN("Failed to enumerate Vulkan layers: {}",
              vk::to_string(res.result));
      return {};
    }
    auto& availableLayers = res.value;

    std::vector<const char*> missingLayers{};

    for (const auto& layer : layers) {
      if (std::ranges::find_if(
              availableLayers,
              [&layer](const vk::LayerProperties& availableLayer) {
                return strcmp(availableLayer.layerName, layer) == 0;
              }) == availableLayers.end()) {
        missingLayers.push_back(layer);
      }
    }
    return missingLayers;
  }

} // namespace keptech::vkh
