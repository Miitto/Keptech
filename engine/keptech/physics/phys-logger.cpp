#include "phys-logger.hpp"
#include <keptech/logging/createLogger.hpp>
#include <spdlog/common.h>

namespace kt::phys {
  const std::shared_ptr<spdlog::logger> logger = kt::core::createLogger("Physics"); // NOLINT
}
