#pragma once

#include "keptech/rhi/bufferTypes.hpp"
#include "keptech/rhi/bufferUsage.hpp"

namespace kt::rhi {
  class Buffer;

  enum class BufferCreateFlags : uint8_t {
    /// Internal use only. Indicates that the buffer is managed internally by the engine.
    EngineBuffer = 1 << 7,
  };

  class BufferCreateInfo {
    friend class Buffer;

  public:
    constexpr BufferCreateInfo(size_t size, Bitflag<BufferUsage> usage, BufferType type = BufferType::Default, const char* name = nullptr,
                               Bitflag<BufferCreateFlags> flags = {}) noexcept
        : size(size), usage(usage), type(type), name(name), flags(flags) {}

    [[nodiscard]]
    const char* getName() const noexcept {
      return name;
    }

    [[nodiscard]]
    BufferType getType() const noexcept {
      return type;
    }

    [[nodiscard]]
    size_t getSize() const noexcept {
      return size;
    }

    Bitflag<BufferUsage> getUsage() const noexcept { return usage; }

    Bitflag<BufferCreateFlags> getFlags() const noexcept { return flags; }

  private:
    size_t size{};
    Bitflag<BufferUsage> usage = BufferUsage::None;
    BufferType type = BufferType::Default;
    const char* name = nullptr;
    Bitflag<BufferCreateFlags> flags{};
  };
} // namespace kt::rhi

DEFINE_BITFLAG_ENUM_OPERATORS(kt::rhi::BufferCreateFlags)