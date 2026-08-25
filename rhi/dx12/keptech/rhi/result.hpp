#pragma once

#include <d3d12.h>

namespace kt::rhi {
  using RawRhiResult = HRESULT;
  constexpr RawRhiResult RawRhiResultOk = S_OK;
} // namespace kt::rhi