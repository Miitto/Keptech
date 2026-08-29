#pragma once

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace kt::core {
  inline std::shared_ptr<spdlog::logger> createLogger(const std::string& name) noexcept {
    auto logger = spdlog::stdout_color_mt(name);
    logger->set_level(spdlog::level::trace);
    logger->set_pattern("[%H:%M:%S.%e] [%n] [%^%L%$]"
#ifndef NDEBUG
                        " [%s:%#]"
#endif
                        " %v");
    return logger;
  }
} // namespace kt::core
