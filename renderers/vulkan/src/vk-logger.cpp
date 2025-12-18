#include "vk-logger.hpp"

#include <keptech/core/createLogger.hpp>
#include <spdlog/common.h>

namespace keptech::vkh {
  const std::shared_ptr<spdlog::logger> logger = keptech::core::createLogger(
      "Vulkan", static_cast<spdlog::level::level_enum>(VK_LOG_LEVEL));
}
