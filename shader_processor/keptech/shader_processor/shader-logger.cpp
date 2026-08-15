#include "shader-logger.hpp"

#include <keptech/logging/createLogger.hpp>
#include <spdlog/common.h>

namespace kt::shader_processor {
  const std::shared_ptr<spdlog::logger> logger = kt::core::createLogger("Shader Processor");
}
