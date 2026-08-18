#pragma once

#include <spdlog/spdlog.h>

#ifndef RHI_LOG_LEVEL
#define RHI_LOG_LEVEL SPDLOG_ACTIVE_LEVEL
#endif

#if RHI_LOG_LEVEL <= SPDLOG_LEVEL_TRACE
#define DX_TRACE(...) kt::rhi::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::trace, __VA_ARGS__)
#else
#define DX_TRACE(...) (void)0
#endif

#if RHI_LOG_LEVEL <= SPDLOG_LEVEL_DEBUG
#define DX_DEBUG(...) kt::rhi::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::debug, __VA_ARGS__)
#else
#define DX_DEBUG(...) (void)0
#endif

#if RHI_LOG_LEVEL <= SPDLOG_LEVEL_INFO
#define DX_INFO(...) kt::rhi::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info, __VA_ARGS__)
#else
#define DX_INFO(...) (void)0
#endif

#if RHI_LOG_LEVEL <= SPDLOG_LEVEL_WARN
#define DX_WARN(...) kt::rhi::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::warn, __VA_ARGS__)
#else
#define DX_WARN(...) (void)0
#endif

#if RHI_LOG_LEVEL <= SPDLOG_LEVEL_ERROR
#define DX_ERROR(...) kt::rhi::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::err, __VA_ARGS__)
#else
#define DX_ERROR(...) (void)0
#endif

#if RHI_LOG_LEVEL <= SPDLOG_LEVEL_CRITICAL
#define DX_CRITICAL(...) kt::rhi::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical, __VA_ARGS__)
#else
#define DX_CRITICAL(...) (void)0
#endif

#ifndef NDEBUG
/// Assert macro that logs a critical message and aborts if the expression is false. Only active in debug builds.
#define DX_ASSERT(expr, ...)                                                                                                               \
  if (!(expr)) {                                                                                                                           \
    kt::rhi::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical,                                 \
                         "Assertion failed: " __VA_ARGS__);                                                                                \
    spdlog::shutdown();                                                                                                                    \
    std::abort();                                                                                                                          \
  }
#else
/// Assert macro that logs a critical message and aborts if the expression is false. Only active in debug builds.
#define DX_ASSERT(expr, ...) (void)0;
#endif

/// Same as #DX_ASSERT, but always active regardless of build type.
#define DX_REQUIRE(expr, ...)                                                                                                              \
  if (!(expr)) {                                                                                                                           \
    kt::rhi::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical,                                 \
                         "Requirement failed: " __VA_ARGS__);                                                                              \
    spdlog::shutdown();                                                                                                                    \
    std::abort();                                                                                                                          \
  }

#define DX_ABORT(...)                                                                                                                      \
  kt::rhi::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical, "Aborting: " __VA_ARGS__);        \
  spdlog::shutdown();                                                                                                                      \
  std::abort();

namespace kt::rhi {
  extern const std::shared_ptr<spdlog::logger> logger;
} // namespace kt::rhi
