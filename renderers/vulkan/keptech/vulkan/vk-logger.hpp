#pragma once

#include <spdlog/spdlog.h>

#ifndef RENDERER_LOG_LEVEL
#define RENDERER_LOG_LEVEL SPDLOG_ACTIVE_LEVEL
#endif

#if RENDERER_LOG_LEVEL <= SPDLOG_LEVEL_TRACE
#define VK_TRACE(...) kt::vkh::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::trace, __VA_ARGS__)
#else
#define VK_TRACE(...) (void)0
#endif

#if RENDERER_LOG_LEVEL <= SPDLOG_LEVEL_DEBUG
#define VK_DEBUG(...) kt::vkh::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::debug, __VA_ARGS__)
#else
#define VK_DEBUG(...) (void)0
#endif

#if RENDERER_LOG_LEVEL <= SPDLOG_LEVEL_INFO
#define VK_INFO(...) kt::vkh::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::info, __VA_ARGS__)
#else
#define VK_INFO(...) (void)0
#endif

#if RENDERER_LOG_LEVEL <= SPDLOG_LEVEL_WARN
#define VK_WARN(...) kt::vkh::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::warn, __VA_ARGS__)
#else
#define VK_WARN(...) (void)0
#endif

#if RENDERER_LOG_LEVEL <= SPDLOG_LEVEL_ERROR
#define VK_ERROR(...) kt::vkh::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::err, __VA_ARGS__)
#else
#define VK_ERROR(...) (void)0
#endif

#if RENDERER_LOG_LEVEL <= SPDLOG_LEVEL_CRITICAL
#define VK_CRITICAL(...) kt::vkh::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical, __VA_ARGS__)
#else
#define VK_CRITICAL(...) (void)0
#endif

#ifndef NDEBUG
/// Assert macro that logs a critical message and aborts if the expression is false. Only active in debug builds.
#define VK_ASSERT(expr, ...)                                                                                                               \
  if (!(expr)) {                                                                                                                           \
    kt::vkh::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical,                                 \
                         "Assertion failed: " __VA_ARGS__);                                                                                \
    std::abort();                                                                                                                          \
  }
#else
/// Assert macro that logs a critical message and aborts if the expression is false. Only active in debug builds.
#define VK_ASSERT(expr, ...) (void)0;
#endif

/// Same as #VK_ASSERT, but always active regardless of build type.
#define VK_REQUIRE(expr, ...)                                                                                                              \
  if (!(expr)) {                                                                                                                           \
    kt::vkh::logger->log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, spdlog::level::critical,                                 \
                         "Requirement failed: " __VA_ARGS__);                                                                              \
    std::abort();                                                                                                                          \
  }

namespace kt::vkh {
  extern const std::shared_ptr<spdlog::logger> logger;
} // namespace kt::vkh
