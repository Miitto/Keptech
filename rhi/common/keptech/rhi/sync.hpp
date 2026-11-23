#pragma once

#include "keptech/std/mixins/postfixIncrement.hpp"
#include <spdlog/fmt/bundled/format.h>

namespace kt::rhi {
  class IWaitValue : PostfixIncrement {
  public:
    IWaitValue(uint64_t value) : _value(value) {}

    uint64_t value() const { return _value; }

    template <typename Self> Self operator++(this Self& self) {
      ++self._value;
      return self;
    }

  private:
    uint64_t _value = 0;
  };

  class GraphicsWaitValue : public IWaitValue {
  public:
    GraphicsWaitValue(uint64_t value = 0) : IWaitValue(value) {}
  };

  class ComputeWaitValue : public IWaitValue {
  public:
    ComputeWaitValue(uint64_t value = 0) : IWaitValue(value) {}
  };

  class CopyWaitValue : public IWaitValue {
  public:
    CopyWaitValue(uint64_t value = 0) : IWaitValue(value) {}
  };
} // namespace kt::rhi

template <> struct fmt::formatter<kt::rhi::GraphicsWaitValue> : fmt::formatter<uint64_t> {
  template <typename FormatContext> auto format(const kt::rhi::GraphicsWaitValue& value, FormatContext& ctx) const {
    return fmt::formatter<uint64_t>::format(value.value(), ctx);
  }
};

template <> struct fmt::formatter<kt::rhi::ComputeWaitValue> : fmt::formatter<uint64_t> {
  template <typename FormatContext> auto format(const kt::rhi::ComputeWaitValue& value, FormatContext& ctx) const {
    return fmt::formatter<uint64_t>::format(value.value(), ctx);
  }
};

template <> struct fmt::formatter<kt::rhi::CopyWaitValue> : fmt::formatter<uint64_t> {
  template <typename FormatContext> auto format(const kt::rhi::CopyWaitValue& value, FormatContext& ctx) const {
    return fmt::formatter<uint64_t>::format(value.value(), ctx);
  }
};