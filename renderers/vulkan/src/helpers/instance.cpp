#include "keptech/vulkan/helpers/instance.hpp"

#include "macros.hpp"

#include "vk-logger.hpp"

#include "keptech/vulkan/helpers/validators.hpp"
#include <SDL3/SDL_vulkan.h>
#include <keptech/core/window.hpp>

namespace keptech::vkh {

  auto createInstance(vk::raii::Context& context, const char* appName,
                      const bool enableValidationLayers,
                      const std::span<const char* const> extraExtensions,
                      const std::span<const char* const> extraLayers)
      -> std::expected<vk::raii::Instance, std::string> {
    VK_TRACE("Creating Instance");
    auto appInfo = vk::ApplicationInfo{
        .pApplicationName = appName,
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = vk::ApiVersion14,
    };

    Uint32 extCnt = 0;
    auto extensionsSDL = SDL_Vulkan_GetInstanceExtensions(&extCnt);

    std::vector<const char*> extensions{extensionsSDL, extensionsSDL + extCnt};
    extensions.insert(extensions.end(), extraExtensions.begin(),
                      extraExtensions.end());

    auto layerNames = std::vector<const char*>{};

    layerNames.insert(layerNames.end(), extraLayers.begin(), extraLayers.end());

    if (enableValidationLayers) {
      printExtensions(context, spdlog::level::trace);
      layerNames.push_back("VK_LAYER_KHRONOS_validation");
      extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    auto missingExtensions = vkh::checkExtensions(context, extensions);
    if (!missingExtensions.empty()) {
      VK_ERROR("Missing required extensions:");
      for (const auto& ext : missingExtensions) {
        VK_ERROR("  - {}", ext);
      }
      return std::unexpected("Missing required Vulkan extensions");
    }

    auto missingLayers = vkh::checkLayers(context, layerNames);
    if (!missingLayers.empty()) {
      VK_ERROR("Missing required layers:");
      for (const auto& layer : missingLayers) {
        VK_ERROR("  - {}", layer);
      }
      return std::unexpected("Missing required Vulkan layers");
    }

    VK_DEBUG("Enabled Instance Extensions:");
    for (const auto& ext : extensions) {
      VK_DEBUG("  - {}", ext);
    }

    VK_DEBUG("Enabled Instance Layers:");
    for (const auto& layer : layerNames) {
      VK_DEBUG("  - {}", layer);
    }

    auto iCreateInfo = vk::InstanceCreateInfo{
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(layerNames.size()),
        .ppEnabledLayerNames = layerNames.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()};
    VK_MAKE(instance, context.createInstance(iCreateInfo),
            "Failed to create Vulkan Instance");

    return std::move(instance);
  }
} // namespace keptech::vkh
