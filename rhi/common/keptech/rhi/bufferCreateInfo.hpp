#pragma once

#include "keptech/rhi/bufferTypes.hpp"
#include "keptech/rhi/bufferUsage.hpp"

namespace kt::rhi {
  class Buffer;

  class BufferCreateInfo {
    friend class Buffer;

  public:
    constexpr BufferCreateInfo(size_t size, Bitflag<BufferUsage> usage, MappingMode allocFlags, MemoryUsage memUsage,
                               const char* name = nullptr) noexcept
        : size(size), usage(usage), mappingMode(allocFlags), memoryUsage(memUsage), name(name) {}

    [[nodiscard]]
    const char* getName() const noexcept {
      return name;
    }

    [[nodiscard]]
    MappingMode getMappingMode() const noexcept {
      return mappingMode;
    }

    [[nodiscard]]
    MemoryUsage getMemoryUsage() const noexcept {
      return memoryUsage;
    }

    [[nodiscard]]
    size_t getSize() const noexcept {
      return size;
    }

    Bitflag<BufferUsage> getUsage() const noexcept { return usage; }

  private:
    size_t size{};
    Bitflag<BufferUsage> usage = BufferUsage::None;
    MappingMode mappingMode{};
    MemoryUsage memoryUsage{};
    const char* name = nullptr;
  };
} // namespace kt::rhi