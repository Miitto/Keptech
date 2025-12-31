#include "keptech/core/kt-logger.hpp"

#include <keptech/logging/createLogger.hpp>
#include <spdlog/common.h>

namespace keptech::core {
  const std::shared_ptr<spdlog::logger> logger = keptech::core::createLogger(
      "Keptech", static_cast<spdlog::level::level_enum>(KT_LOG_LEVEL));
}
