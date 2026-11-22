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

template <> struct fmt::formatter<kt::rhi::BufferType> : fmt::formatter<std::string_view> {
  template <typename FormatContext> auto format(const kt::rhi::BufferType& type, FormatContext& ctx) {
    std::string_view name;
    switch (type) {
    case kt::rhi::BufferType::Default:
      name = "Default";
      break;
    case kt::rhi::BufferType::GpuMapped:
      name = "GpuMapped";
      break;
    case kt::rhi::BufferType::Staging:
      name = "Staging";
      break;
    case kt::rhi::BufferType::Readback:
      name = "Readback";
      break;
    default:
      name = "Unknown";
      break;
    }
    return fmt::formatter<std::string_view>::format(name, ctx);
  }
};