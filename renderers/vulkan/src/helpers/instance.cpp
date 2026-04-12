#include "keptech/vulkan/helpers/instance.hpp"

#include "macros.hpp"

#include "vk-logger.hpp"

#include "keptech/vulkan/helpers/validators.hpp"
#include "vulkan/vulkan.h"
#include <SDL3/SDL_vulkan.h>
#include <keptech/core/window.hpp>

namespace kt::vkh {

  VKAPI_ATTR VkBool32 debug_utils_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                                     VkDebugUtilsMessageTypeFlagsEXT message_type,
                                                     const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
    if (message_severity & VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
      VK_WARN("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
    } else if (message_severity & VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
      VK_ERROR("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
    } else {
      VK_INFO("{} - {}: {}", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
    }
    return VK_FALSE;
  }

  auto createInstance(const char* appName, const bool enableValidationLayers, const std::span<const char* const> extraExtensions,
                      const std::span<const char* const> extraLayers) -> std::expected<Instance, std::string> {
    VK_TRACE("Creating Instance");
    auto appInfo = VkApplicationInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = appName,
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_MAKE_API_VERSION(0, 1, 4, 0),
    };

    Uint32 extCnt = 0;
    auto extensionsSDL = SDL_Vulkan_GetInstanceExtensions(&extCnt);

    std::vector<const char*> extensions{extensionsSDL, extensionsSDL + extCnt};
    extensions.insert(extensions.end(), extraExtensions.begin(), extraExtensions.end());

#ifndef NDEBUG
    extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif

    auto layerNames = std::vector<const char*>{};

    layerNames.insert(layerNames.end(), extraLayers.begin(), extraLayers.end());

    if (enableValidationLayers) {
      printExtensions(spdlog::level::trace);
      layerNames.push_back("VK_LAYER_KHRONOS_validation");
      extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    auto missingExtensions = vkh::checkExtensions(extensions);
    if (!missingExtensions.empty()) {
      VK_ERROR("Missing required extensions:");
      for (const auto& ext : missingExtensions) {
        VK_ERROR("  - {}", ext);
      }
      return std::unexpected("Missing required Vulkan extensions");
    }

    auto missingLayers = vkh::checkLayers(layerNames);
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

    auto iCreateInfo = VkInstanceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(layerNames.size()),
        .ppEnabledLayerNames = layerNames.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

#ifndef NDEBUG
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_utils_messenger_callback,
    };
    iCreateInfo.pNext = &debugCreateInfo;
#endif

    Instance instance;
    VK_MAKE(vkCreateInstance(&iCreateInfo, nullptr, &instance.handle), "Failed to create Vulkan Instance");

#ifndef NDEBUG
    auto createDebugUtilsMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (createDebugUtilsMessenger) {
      VkDebugUtilsMessengerEXT debugMessenger;
      VK_MAKE(createDebugUtilsMessenger(instance, &debugCreateInfo, nullptr, &debugMessenger), "Failed to create Debug Utils Messenger");
      instance.debugMessenger = debugMessenger;
    } else {
      VK_ERROR("Could not load vkCreateDebugUtilsMessengerEXT function.");
    }
#endif

    return instance;
  }
} // namespace kt::vkh
