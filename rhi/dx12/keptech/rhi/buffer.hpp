#pragma once

#include "keptech/core/result.hpp"
#include "keptech/rhi/bufferTypes.hpp"
#include "keptech/rhi/bufferUsage.hpp"
#include "keptech/rhi/result.hpp"

namespace D3D12MA {
  struct Allocation;
}

namespace kt::rhi {
  class BufferCreateInfo;
  class BufferRef;
  using RawBuffer = ID3D12Resource*;

  class Buffer {
#include "keptech/rhi/interface/buffer.inl"

  public:
    operator ID3D12Resource*() const;
    ID3D12Resource* operator->() const;
    D3D12MA::Allocation* dxTakeAllocation();

  private:
    Buffer(std::string name, size_t size, Bitflag<BufferUsage> usage, BufferType type, D3D12MA::Allocation* allocation);

    std::string name;
    size_t _size = 0;
    Bitflag<BufferUsage> usage = BufferUsage::None;
    BufferType type = BufferType::Default;
    void* mapPtr = nullptr;
    D3D12MA::Allocation* allocation = nullptr;
  };
} // namespace kt::rhi