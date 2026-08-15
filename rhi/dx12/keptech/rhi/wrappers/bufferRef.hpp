#pragma once

#include <d3d12.h>

namespace kt::rhi {
  class Buffer;

  using RawBuffer = ID3D12Resource*;

  class BufferRef {
  public:
    friend class Buffer;

    const char* getName() const { return name; }
    operator RawBuffer() const { return resource; }
    size_t size() const { return _size; }
    void* mapping(size_t offset = 0) const { return static_cast<char*>(mappedPtr) + offset; }
    template <typename T> T* mapping(size_t offset = 0) const { return static_cast<T*>(mapping(offset)); }

    BufferRef() = default;
    BufferRef(const char* name, RawBuffer resource, size_t size, void* mappedPtr)
        : name(name), resource(resource), _size(size), mappedPtr(mappedPtr) {}

    RawBuffer dxGetResource() const { return resource; }

  private:
    const char* name = nullptr;
    RawBuffer resource = nullptr;
    size_t _size = 0;
    void* mappedPtr = nullptr;
  };
} // namespace kt::rhi