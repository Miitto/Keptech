#pragma once

#include <spdlog/spdlog.h>

#ifndef ECS_LOG_LEVEL
#define ECS_LOG_LEVEL SPDLOG_ACTIVE_LEVEL
#endif

#if ECS_LOG_LEVEL <= SPDLOG_LEVEL_TRACE
#define ECS_TRACE(...)                                                         \
  keptech::ecs::logger->log(                                                   \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::trace, __VA_ARGS__)
#else
#define ECS_TRACE(...) (void)0
#endif

#if ECS_LOG_LEVEL <= SPDLOG_LEVEL_DEBUG
#define ECS_DEBUG(...)                                                         \
  keptech::ecs::logger->log(                                                   \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::debug, __VA_ARGS__)
#else
#define ECS_DEBUG(...) (void)0
#endif

#if ECS_LOG_LEVEL <= SPDLOG_LEVEL_INFO
#define ECS_INFO(...)                                                          \
  keptech::ecs::logger->log(                                                   \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::info, __VA_ARGS__)
#else
#define ECS_INFO(...) (void)0
#endif

#if ECS_LOG_LEVEL <= SPDLOG_LEVEL_WARN
#define ECS_WARN(...)                                                          \
  keptech::ecs::logger->log(                                                   \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::warn, __VA_ARGS__)
#else
#define ECS_WARN(...) (void)0
#endif

#if ECS_LOG_LEVEL <= SPDLOG_LEVEL_ERROR
#define ECS_ERROR(...)                                                         \
  keptech::ecs::logger->log(                                                   \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::err, __VA_ARGS__)
#else
#define ECS_ERROR(...) (void)0
#endif

#if ECS_LOG_LEVEL <= SPDLOG_LEVEL_CRITICAL
#define ECS_CRITICAL(...)                                                      \
  keptech::ecs::logger->log(                                                   \
      spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION},                 \
      spdlog::level::critical, __VA_ARGS__)
#else
#define ECS_CRITICAL(...) (void)0
#endif

namespace keptech::ecs {
  extern const std::shared_ptr<spdlog::logger> logger;
} // namespace keptech::ecs
