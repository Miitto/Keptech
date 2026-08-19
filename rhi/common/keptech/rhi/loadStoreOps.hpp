#pragma once

namespace kt::rhi {
  enum class LoadOp : uint8_t {
    DontCare,
    Clear,
    Load,
  };

  enum class StoreOp : uint8_t {
    Store,
    DontCare,
  };
} // namespace kt::rhi