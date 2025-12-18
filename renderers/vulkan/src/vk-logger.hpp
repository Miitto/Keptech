#pragma once

#include <spdlog/spdlog.h>

#ifndef VK_LOG_LEVEL
#define VK_LOG_LEVEL SPDLOG_ACTIVE_LEVEL
#endif

#if VK_LOG_LEVEL <= SPDLOG_LEVEL_TRACE
#define VK_TRACE(...)                                                          \
  keptech::vkh::logger->log(                                                   \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::trace, __VA_ARGS__)
#else
#define VK_TRACE(...) (void)0
#endif

#if VK_LOG_LEVEL <= SPDLOG_LEVEL_DEBUG
#define VK_DEBUG(...)                                                          \
  keptech::vkh::logger->log(                                                   \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::debug, __VA_ARGS__)
#else
#define VK_DEBUG(...) (void)0
#endif

#if VK_LOG_LEVEL <= SPDLOG_LEVEL_INFO
#define VK_INFO(...)                                                           \
  keptech::vkh::logger->log(                                                   \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::info, __VA_ARGS__)
#else
#define VK_INFO(...) (void)0
#endif

#if VK_LOG_LEVEL <= SPDLOG_LEVEL_WARN
#define VK_WARN(...)                                                           \
  keptech::vkh::logger->log(                                                   \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::warn, __VA_ARGS__)
#else
#define VK_WARN(...) (void)0
#endif

#if VK_LOG_LEVEL <= SPDLOG_LEVEL_ERROR
#define VK_ERROR(...)                                                          \
  keptech::vkh::logger->log(                                                   \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::err, __VA_ARGS__)
#else
#define VK_ERROR(...) (void)0
#endif

#if VK_LOG_LEVEL <= SPDLOG_LEVEL_CRITICAL
#define VK_CRITICAL(...)                                                       \
  keptech::vkh::logger->log(                                                   \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::critical, __VA_ARGS__)
#else
#define VK_CRITICAL(...) (void)0
#endif

namespace keptech::vkh {
  extern const std::shared_ptr<spdlog::logger> logger;
} // namespace keptech::vkh
