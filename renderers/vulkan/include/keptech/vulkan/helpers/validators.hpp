#pragma once

#include <span>
#include <spdlog/fwd.h>
#include <vector>
#include <vulkan/vulkan.h>

namespace kt::vkh {

  void printExtensions(spdlog::level::level_enum logLevel);
  std::vector<const char*> checkExtensions(std::span<const char*> extensions);
  std::vector<const char*> checkLayers(std::span<const char*> layers);
} // namespace kt::vkh
