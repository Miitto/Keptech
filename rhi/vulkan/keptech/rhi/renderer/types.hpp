#pragma once

#include <cstdint>

namespace kt::rhi {
  constexpr uint64_t INVALID_HANDLE = 0;

  using ImageHandle = uint64_t;
  using SamplerHandle = uint64_t;
  using BufferHandle = uint64_t;
} // namespace kt::rhi