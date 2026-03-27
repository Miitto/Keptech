#pragma once

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace kt::core {
  inline std::shared_ptr<spdlog::logger>
  createLogger(const std::string& name,
               const spdlog::level::level_enum level) noexcept {
    auto logger = spdlog::stdout_color_mt(name);
    logger->set_level(level);
    logger->set_pattern("[%H:%M:%S.%e] [%n] [%^%L%$] [%s:%#] %v");
    return logger;
  }
} // namespace kt::core
