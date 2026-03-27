#pragma once

#include <expected>
#include <span>
#include <string>
#include <vulkan/vulkan.h>

namespace kt::vkh {

  auto createInstance(const char* appName, const bool enableValidationLayers,
                      const std::span<const char* const> extraExtensions = {},
                      const std::span<const char* const> extraLayers = {})
      -> std::expected<VkInstance, std::string>;
} // namespace kt::vkh
