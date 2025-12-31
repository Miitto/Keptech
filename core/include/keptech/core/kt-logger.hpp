#pragma once

#include <spdlog/spdlog.h>

#ifndef KT_LOG_LEVEL
#define KT_LOG_LEVEL SPDLOG_ACTIVE_LEVEL
#endif

#if KT_LOG_LEVEL <= SPDLOG_LEVEL_TRACE
#define KT_TRACE(...)                                                          \
  keptech::core::logger->log(                                                  \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::trace, __VA_ARGS__)
#else
#define KT_TRACE(...) (void)0
#endif

#if KT_LOG_LEVEL <= SPDLOG_LEVEL_DEBUG
#define KT_DEBUG(...)                                                          \
  keptech::core::logger->log(                                                  \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::debug, __VA_ARGS__)
#else
#define KT_DEBUG(...) (void)0
#endif

#if KT_LOG_LEVEL <= SPDLOG_LEVEL_INFO
#define KT_INFO(...)                                                           \
  keptech::core::logger->log(                                                  \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::info, __VA_ARGS__)
#else
#define KT_INFO(...) (void)0
#endif

#if KT_LOG_LEVEL <= SPDLOG_LEVEL_WARN
#define KT_WARN(...)                                                           \
  keptech::core::logger->log(                                                  \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::warn, __VA_ARGS__)
#else
#define KT_WARN(...) (void)0
#endif

#if KT_LOG_LEVEL <= SPDLOG_LEVEL_ERROR
#define KT_ERROR(...)                                                          \
  keptech::core::logger->log(                                                  \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::err, __VA_ARGS__)
#else
#define KT_ERROR(...) (void)0
#endif

#if KT_LOG_LEVEL <= SPDLOG_LEVEL_CRITICAL
#define KT_CRITICAL(...)                                                       \
  keptech::core::logger->log(                                                  \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::critical, __VA_ARGS__)
#else
#define KT_CRITICAL(...) (void)0
#endif

namespace keptech::core {
  extern const std::shared_ptr<spdlog::logger> logger;
} // namespace keptech::core
