#include "vk-logger.hpp"

#include <keptech/logging/createLogger.hpp>
#include <spdlog/common.h>

namespace kt::rhi {
  const std::shared_ptr<spdlog::logger> logger = kt::core::createLogger("Vulkan");
}
