#pragma once

#include "vk-logger.hpp"

#define VKH_MAKE(_NAME, _EXPR, _ERROR)                                         \
  auto _NAME##_res = _EXPR;                                                    \
  if (!_NAME##_res.has_value()) {                                              \
    VK_ERROR(_ERROR ": {}", _NAME##_res.error());                              \
    return std::unexpected(_ERROR);                                            \
  }                                                                            \
  auto&(_NAME) = _NAME##_res.value();

#define VK_MAKE(_NAME, _EXPR, _ERROR)                                          \
  auto _NAME##_res = _EXPR;                                                    \
  if (_NAME##_res.result != vk::Result::eSuccess) {                            \
    VK_ERROR(_ERROR ": {}", vk::to_string(_NAME##_res.result));                \
    return std::unexpected(_ERROR);                                            \
  }                                                                            \
  auto&(_NAME) = _NAME##_res.value;

#define VMA_MAKE(_NAME, _EXPR, _ERROR)                                         \
  auto _NAME##_res = _EXPR;                                                    \
  if (_NAME##_res.result != vk::Result::eSuccess) {                            \
    VK_ERROR(_ERROR ": {}", vk::to_string(_NAME##_res.result));                \
    return std::unexpected(_ERROR);                                            \
  }                                                                            \
  auto&(_NAME) = _NAME##_res.value;
