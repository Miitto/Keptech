#pragma once

#include <cstdint>

namespace kt::rhi {
  enum class BufferType : uint8_t {
    /// Unmapped GPU resident buffer.
    Default,
    /// Mapped GPU resident buffer.
    GpuMapped,
    /// Mapped CPU resident buffer.
    Staging,
    /// Mapped CPU resident buffer that supports CPU readback.
    Readback
  };
};