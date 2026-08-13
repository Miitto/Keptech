#pragma once

#include "keptech/rhi/helpers/formatting.hpp"
#include "vk-logger.hpp"

#define VKH_MAKE(_NAME, _EXPR, _ERROR)                                                                                                     \
  auto _NAME##_res = _EXPR;                                                                                                                \
  if (!_NAME##_res.has_value()) {                                                                                                          \
    VK_ERROR(_ERROR ": {}", _NAME##_res.error());                                                                                          \
    return std::unexpected(_ERROR);                                                                                                        \
  }                                                                                                                                        \
  auto&(_NAME) = _NAME##_res.value();

#define VKH_CHECK(_EXPR, _ERROR)                                                                                                           \
  {                                                                                                                                        \
    auto res = _EXPR;                                                                                                                      \
    if (!res.has_value()) {                                                                                                                \
      VK_CRITICAL(_ERROR ": {}", res.error());                                                                                             \
      std::abort();                                                                                                                        \
    }                                                                                                                                      \
  }

#define VK_MAKE(_EXPR, _ERROR)                                                                                                             \
  {                                                                                                                                        \
    auto res = _EXPR;                                                                                                                      \
    if (res != VK_SUCCESS) {                                                                                                               \
      VK_ERROR(_ERROR ": {}", res);                                                                                                        \
      return std::unexpected(_ERROR);                                                                                                      \
    }                                                                                                                                      \
  }

#define VK_CHECK(_EXPR, _ERROR)                                                                                                            \
  {                                                                                                                                        \
    auto res = _EXPR;                                                                                                                      \
    if (res != VK_SUCCESS) {                                                                                                               \
      VK_CRITICAL(_ERROR);                                                                                                                 \
      std::abort();                                                                                                                        \
    }                                                                                                                                      \
  }

#define VMA_MAKE(_NAME, _EXPR, _ERROR)                                                                                                     \
  auto _NAME##_res = _EXPR;                                                                                                                \
  if (_NAME##_res.result != vk::Result::eSuccess) {                                                                                        \
    VK_ERROR(_ERROR ": {}", vk::to_string(_NAME##_res.result));                                                                            \
    return std::unexpected(_ERROR);                                                                                                        \
  }                                                                                                                                        \
  auto&(_NAME) = _NAME##_res.value;

/// Macro for declaring a class as move-only, deleting copy constructor and copy assignment operator, and defaulting move constructor and
/// move assignment operator.
#define MOVE_ONLY_DEFAULT(_TYPE)                                                                                                           \
  _TYPE(const _TYPE&) = delete;                                                                                                            \
  _TYPE& operator=(const _TYPE&) = delete;                                                                                                 \
  _TYPE(_TYPE&&) noexcept = default;                                                                                                       \
  _TYPE& operator=(_TYPE&&) noexcept = default;

/// Macro for declaring a class as move-only, deleting copy constructor and copy assignment operator, and declaring move constructor and
/// move assignment operator.
#define MOVE_ONLY(_TYPE)                                                                                                                   \
  _TYPE(const _TYPE&) = delete;                                                                                                            \
  _TYPE& operator=(const _TYPE&) = delete;                                                                                                 \
  _TYPE(_TYPE&&) noexcept;                                                                                                                 \
  _TYPE& operator=(_TYPE&&) noexcept;