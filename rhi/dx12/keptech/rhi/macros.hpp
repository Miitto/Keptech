#pragma once

#include "dx-logger.hpp"

#define DX_MAKE(expr, err)                                                                                                                 \
  {                                                                                                                                        \
    HRESULT hr__ = (expr);                                                                                                                 \
    if (FAILED(hr__)) {                                                                                                                    \
      return std::unexpected("DirectX call failed: " err);                                                                                 \
    }                                                                                                                                      \
  }

#define DX_CREATE(var, expr, err)                                                                                                          \
  auto var##res = expr;                                                                                                                    \
  if (!var##res.has_value()) {                                                                                                             \
    return std::unexpected(fmt::format(err ": {}", var##res.error()));                                                                     \
  }                                                                                                                                        \
  auto& var = var##res.value();