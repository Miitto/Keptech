#pragma once

#include <Volk/volk.h>

namespace kt::rhi {
  using RawRhiResult = VkResult;
  constexpr RawRhiResult RawRhiResultOk = VK_SUCCESS;
} // namespace kt::rhi