#pragma once

#include <span>
#include <spdlog/fwd.h>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {

  void printExtensions(vk::raii::Context& context,
                       spdlog::level::level_enum logLevel);
  std::vector<const char*> checkExtensions(vk::raii::Context& context,
                                           std::span<const char*> extensions);
  std::vector<const char*> checkLayers(vk::raii::Context& context,
                                       std::span<const char*> layers);
} // namespace keptech::vkh
