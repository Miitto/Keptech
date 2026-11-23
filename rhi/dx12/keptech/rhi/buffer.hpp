#pragma once

#include "keptech/rhi/bufferTypes.hpp"
#include "keptech/rhi/bufferUsage.hpp"
#include "keptech/rhi/interface/buffer_base.hpp"

namespace D3D12MA {
  struct Allocation; // NOLINT
}

namespace kt::rhi {
  class BufferCreateInfo;
  class BufferRef;
  using RawBuffer = ID3D12Resource*;

  class Buffer : public BufferBase {
  public:
    friend class BufferBase;

    operator ID3D12Resource*() const;
    ID3D12Resource* operator->() const;
    D3D12MA::Allocation* dxTakeAllocation();

    Buffer() = default;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&&) noexcept;
    Buffer& operator=(Buffer&&) noexcept;
    ~Buffer();

  private:
    Buffer(std::string name, size_t size, Bitflag<BufferUsage> usage, BufferType type, D3D12MA::Allocation* allocation);

    void* mapPtr = nullptr;
    D3D12MA::Allocation* allocation = nullptr;
  };
} // namespace kt::rhi