#pragma once

#include <spdlog/spdlog.h>

#ifndef SHADER_LOG_LEVEL
#define SHADER_LOG_LEVEL SPDLOG_ACTIVE_LEVEL
#endif

#if SHADER_LOG_LEVEL <= SPDLOG_LEVEL_TRACE
#define SHDR_TRACE(...)                                                                                                                    \
  kt::shader_processor::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::trace, __VA_ARGS__)
#else
#define SHDR_TRACE(...) (void)0
#endif

#if SHADER_LOG_LEVEL <= SPDLOG_LEVEL_DEBUG
#define SHDR_DEBUG(...)                                                                                                                    \
  kt::shader_processor::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::debug, __VA_ARGS__)
#else
#define SHDR_DEBUG(...) (void)0
#endif

#if SHADER_LOG_LEVEL <= SPDLOG_LEVEL_INFO
#define SHDR_INFO(...)                                                                                                                     \
  kt::shader_processor::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info, __VA_ARGS__)
#else
#define SHDR_INFO(...) (void)0
#endif

#if SHADER_LOG_LEVEL <= SPDLOG_LEVEL_WARN
#define SHDR_WARN(...)                                                                                                                     \
  kt::shader_processor::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::warn, __VA_ARGS__)
#else
#define SHDR_WARN(...) (void)0
#endif

#if SHADER_LOG_LEVEL <= SPDLOG_LEVEL_ERROR
#define SHDR_ERROR(...)                                                                                                                    \
  kt::shader_processor::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::err, __VA_ARGS__)
#else
#define SHDR_ERROR(...) (void)0
#endif

#if SHADER_LOG_LEVEL <= SPDLOG_LEVEL_CRITICAL
#define SHDR_CRITICAL(...)                                                                                                                 \
  kt::shader_processor::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical, __VA_ARGS__)
#else
#define SHDR_CRITICAL(...) (void)0
#endif

#ifndef NDEBUG
/// Assert macro that logs a critical message and aborts if the expression is false. Only active in debug builds.
#define SHDR_ASSERT(expr, ...)                                                                                                             \
  if (!(expr)) {                                                                                                                           \
    kt::shader_processor::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical,                    \
                                      "Assertion failed: " __VA_ARGS__);                                                                   \
    spdlog::shutdown();                                                                                                                    \
    std::abort();                                                                                                                          \
  }
#else
/// Assert macro that logs a critical message and aborts if the expression is false. Only active in debug builds.
#define SHDR_ASSERT(expr, ...) (void)0;
#endif

/// Same as #SHDR_ASSERT, but always active regardless of build type.
#define SHDR_REQUIRE(expr, ...)                                                                                                            \
  if (!(expr)) {                                                                                                                           \
    kt::shader_processor::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical,                    \
                                      "Requirement failed: " __VA_ARGS__);                                                                 \
    spdlog::shutdown();                                                                                                                    \
    std::abort();                                                                                                                          \
  }

#define SHDR_ABORT(...)                                                                                                                    \
  kt::shader_processor::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical,                      \
                                    "Aborting: " __VA_ARGS__);                                                                             \
  spdlog::shutdown();                                                                                                                      \
  std::abort();

namespace kt::shader_processor {
  extern const std::shared_ptr<spdlog::logger> logger;
} // namespace kt::shader_processor
