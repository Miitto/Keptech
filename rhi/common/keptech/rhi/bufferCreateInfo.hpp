#pragma once

#include "keptech/rhi/bufferTypes.hpp"
#include "keptech/rhi/bufferUsage.hpp"

namespace kt::rhi {
  class Buffer;

  class BufferCreateInfo {
    friend class Buffer;

  public:
    constexpr BufferCreateInfo(size_t size, Bitflag<BufferUsage> usage, BufferType type = BufferType::Default,
                               const char* name = nullptr) noexcept
        : size(size), usage(usage), type(type), name(name) {}

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

  private:
    size_t size{};
    Bitflag<BufferUsage> usage = BufferUsage::None;
    BufferType type = BufferType::Default;
    const char* name = nullptr;
  };
} // namespace kt::rhi