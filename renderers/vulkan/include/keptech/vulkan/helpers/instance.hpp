#pragma once

#include <expected>
#include <span>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {

  auto createInstance(vk::raii::Context& context, const char* appName,
                      const bool enableValidationLayers,
                      const std::span<const char* const> extraExtensions = {},
                      const std::span<const char* const> extraLayers = {})
      -> std::expected<vk::raii::Instance, std::string>;
} // namespace keptech::vkh
