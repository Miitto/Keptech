#pragma once

#include <spdlog/spdlog.h>

#ifndef PHYS_LOG_LEVEL
#define PHYS_LOG_LEVEL SPDLOG_ACTIVE_LEVEL
#endif

#if PHYS_LOG_LEVEL <= SPDLOG_LEVEL_TRACE
#define PHYS_TRACE(...) kt::phys::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::trace, __VA_ARGS__)
#else
#define PHYS_TRACE(...) (void)0
#endif

#if PHYS_LOG_LEVEL <= SPDLOG_LEVEL_DEBUG
#define PHYS_DEBUG(...) kt::phys::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::debug, __VA_ARGS__)
#else
#define PHYS_DEBUG(...) (void)0
#endif

#if PHYS_LOG_LEVEL <= SPDLOG_LEVEL_INFO
#define PHYS_INFO(...) kt::phys::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info, __VA_ARGS__)
#else
#define PHYS_INFO(...) (void)0
#endif

#if PHYS_LOG_LEVEL <= SPDLOG_LEVEL_WARN
#define PHYS_WARN(...) kt::phys::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::warn, __VA_ARGS__)
#else
#define PHYS_WARN(...) (void)0
#endif

#if PHYS_LOG_LEVEL <= SPDLOG_LEVEL_ERROR
#define PHYS_ERROR(...) kt::phys::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::err, __VA_ARGS__)
#else
#define PHYS_ERROR(...) (void)0
#endif

#if PHYS_LOG_LEVEL <= SPDLOG_LEVEL_CRITICAL
#define PHYS_CRITICAL(...)                                                                                                                 \
  kt::phys::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical, __VA_ARGS__)
#else
#define PHYS_CRITICAL(...) (void)0
#endif

#ifndef NDEBUG
#define PHYS_ASSERT(cond, ...)                                                                                                             \
  if (!(cond)) {                                                                                                                           \
    kt::phys::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical,                                \
                          "Assertion failed: " __VA_ARGS__);                                                                               \
    spdlog::shutdown();                                                                                                                    \
    std::abort();                                                                                                                          \
  }
#else
#define PHYS_ASSERT(cond, ...) (void)0
#endif

#define PHYS_REQUIRE(expr, ...)                                                                                                            \
  if (!(expr)) {                                                                                                                           \
    kt::phys::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical,                                \
                          "Requirement failed: " __VA_ARGS__);                                                                             \
    spdlog::shutdown();                                                                                                                    \
    std::abort();                                                                                                                          \
  }

#define PHYS_ABORT(...)                                                                                                                    \
  kt::phys::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical, "Aborted: " __VA_ARGS__);        \
  spdlog::shutdown();                                                                                                                      \
  std::abort();

namespace kt::phys {
  extern const std::shared_ptr<spdlog::logger> logger;
} // namespace kt::phys
