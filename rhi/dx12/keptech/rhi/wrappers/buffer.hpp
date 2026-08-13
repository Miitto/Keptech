#pragma once

#include "keptech/core/result.hpp"
#include "keptech/rhi/bufferTypes.hpp"
#include "keptech/rhi/bufferUsage.hpp"

namespace D3D12MA {
  struct Allocation;
}

namespace kt::rhi {
  class BufferCreateInfo;

  class Buffer {
  public:
    static kt::Result<Buffer, HRESULT, 0> create(const BufferCreateInfo& info);

    const std::string& getName() const;
    MappingMode getMappingMode() const;
    size_t size() const;
    bool isMapped() const;
    void* mapping() const;
    Bitflag<BufferUsage> getUsage() const;

    void destroy();

    operator ID3D12Resource*() const;

    Buffer() = default;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    ~Buffer();

    D3D12MA::Allocation* takeAllocation();

  private:
    Buffer(std::string name, size_t size, Bitflag<BufferUsage> usage, MappingMode mappingMode, D3D12MA::Allocation* allocation);

    std::string name;
    size_t _size = 0;
    Bitflag<BufferUsage> usage = BufferUsage::None;
    MappingMode mappingMode = MappingMode::None;
    void* mapPtr = nullptr;
    D3D12MA::Allocation* allocation = nullptr;
  };
} // namespace kt::rhi