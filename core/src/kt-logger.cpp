#include "keptech/core/kt-logger.hpp"

#include <keptech/logging/createLogger.hpp>
#include <spdlog/common.h>

namespace kt::core {
  const std::shared_ptr<spdlog::logger> logger =
      kt::core::createLogger("Keptech", spdlog::level::trace);
}
