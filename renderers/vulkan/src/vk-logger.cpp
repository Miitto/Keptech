#include "vk-logger.hpp"

#include <keptech/logging/createLogger.hpp>
#include <spdlog/common.h>

namespace kt::vkh {
  const std::shared_ptr<spdlog::logger> logger = kt::core::createLogger("Vulkan", static_cast<spdlog::level::level_enum>(VK_LOG_LEVEL));
}
