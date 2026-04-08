#include "keptech/ecs/ecs-logger.hpp"

#include "keptech/logging/createLogger.hpp"

namespace kt::ecs {
  const std::shared_ptr<spdlog::logger> logger = kt::core::createLogger("ECS");
}
