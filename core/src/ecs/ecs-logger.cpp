#include "keptech/core/ecs/ecs-logger.hpp"

#include "keptech/core/createLogger.hpp"

namespace keptech::ecs {
  const std::shared_ptr<spdlog::logger> logger = keptech::core::createLogger(
      "ECS", static_cast<spdlog::level::level_enum>(ECS_LOG_LEVEL));
}
