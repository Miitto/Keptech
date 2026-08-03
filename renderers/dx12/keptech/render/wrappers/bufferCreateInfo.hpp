#pragma once

#include "keptech/render/types.hpp"

namespace kt::rdr {
  class Buffer;

  class BufferCreateInfo {
    friend class Buffer;

  public:
    constexpr BufferCreateInfo(size_t size, MappingMode allocFlags, MemoryUsage memUsage, const char* name = nullptr) noexcept
        : size(size), name(name), mappingMode(allocFlags), memoryUsage(memUsage) {}

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

  private:
    size_t size{};
    const char* name = nullptr;
    MappingMode mappingMode{};
    MemoryUsage memoryUsage{};
  };
} // namespace kt::rdr