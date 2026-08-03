#pragma once

#include "keptech/core/result.hpp"
#include "keptech/render/types.hpp"

namespace D3D12MA {
  struct Allocation;
}

namespace kt::rdr {
  class BufferCreateInfo;

  class Buffer {
  public:
    static kt::Result<Buffer, HRESULT, 0> create(const BufferCreateInfo& info);

    const std::string& getName() const;
    MappingMode getMappingMode() const;
    size_t size() const;
    bool isMapped() const;
    void* mapping() const;

    void destroy();

    Buffer() = default;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    ~Buffer();

  private:
    Buffer(std::string name, size_t size, MappingMode mappingMode, D3D12MA::Allocation* allocation);

    std::string name;
    size_t _size;
    MappingMode mappingMode;
    void* mapPtr = nullptr;
    D3D12MA::Allocation* allocation = nullptr;
  };
} // namespace kt::rdr