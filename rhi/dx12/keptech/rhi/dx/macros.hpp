#pragma once

#include "dx/dx-logger.hpp"
#include <comdef.h>

#define DX_MAKE(expr, err)                                                                                                                 \
  {                                                                                                                                        \
    HRESULT hr__ = (expr);                                                                                                                 \
    if (FAILED(hr__)) {                                                                                                                    \
      _com_error e(hr__);                                                                                                                  \
      DX_ERROR("DirectX call failed: {}: {}", err, e.ErrorMessage());                                                                      \
      return std::unexpected("DirectX call failed: " err);                                                                                 \
    }                                                                                                                                      \
  }

#define DX_CREATE(var, expr, err)                                                                                                          \
  auto var##res = expr;                                                                                                                    \
  if (!var##res.has_value()) {                                                                                                             \
    return std::unexpected(fmt::format(err ": {}", var##res.error()));                                                                     \
  }                                                                                                                                        \
  auto& var = var##res.value();