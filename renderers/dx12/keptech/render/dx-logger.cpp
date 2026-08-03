#include "dx-logger.hpp"

#include <keptech/logging/createLogger.hpp>
#include <spdlog/common.h>

namespace kt::rdr {
  const std::shared_ptr<spdlog::logger> logger = kt::core::createLogger("DirectX 12");
}
