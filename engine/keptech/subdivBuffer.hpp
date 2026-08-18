#pragma once

#include "keptech/rhi/buffer.hpp"
#include "keptech/rhi/bufferRef.hpp"

namespace kt {
  template <typename T> class SubdivBuffer {
  public:
    consteval static size_t sElementSize() { return sizeof(T); }
    consteval size_t elementSize() const { return sizeof(T); }

    size_t count() const { return _count; }
    size_t occupied() const { return _count * sizeof(T); }
    size_t capacity() const { return buffer.size() / sizeof(T); }

    T* endPtr() const { return buffer.mapping<T>(occupied()); }
    T* beginPtr() const { return buffer.mapping<T>(); }

    void setCount(size_t count) { _count = count; }
    void registerWrites(size_t count) { _count += count; }

    bool hasSpaceFor(size_t count) const { return (_count + count) * sizeof(T) <= buffer.size(); }

    operator const rhi::Buffer&() const { return buffer; }
    operator rhi::RawBuffer() const { return buffer; }
    operator rhi::BufferRef() const { return buffer; }

    rhi::Buffer* operator->() { return &buffer; }

    rhi::Buffer& getBuffer() { return buffer; }

  private:
    rhi::Buffer buffer;
    size_t _count = 0;
  };
} // namespace kt