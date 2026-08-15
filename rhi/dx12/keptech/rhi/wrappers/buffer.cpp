#include "buffer.hpp"

#include "D3D12MemAlloc.h"
#include "bufferRef.hpp"
#include "d3dx12.h"
#include "dx-logger.hpp"
#include "keptech/rhi/bufferCreateInfo.hpp"
#include "rhi.hpp"

namespace kt::rhi {
  const std::string& Buffer::getName() const { return name; }
  size_t Buffer::size() const { return _size; }
  bool Buffer::isMapped() const { return mapPtr != nullptr; }
  BufferType Buffer::getType() const { return type; }
  void* Buffer::mapping(size_t offset) const { return static_cast<uint8_t*>(mapPtr) + offset; }
  Bitflag<BufferUsage> Buffer::getUsage() const { return usage; }

  bool Buffer::isValid() const { return allocation != nullptr; }

  kt::Result<Buffer, HRESULT, 0> Buffer::create(const BufferCreateInfo& info) {
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(info.getSize());

    D3D12MA::ALLOCATION_DESC allocDesc{};
    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

    switch (info.getType()) {
    case BufferType::Default:
      allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
      break;
    case BufferType::GpuMapped:
      allocDesc.HeapType = D3D12_HEAP_TYPE_GPU_UPLOAD;
      initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
      break;
    case BufferType::Staging:
      allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
      initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
      break;
    case BufferType::Readback:
      allocDesc.HeapType = D3D12_HEAP_TYPE_READBACK;
      initialState = D3D12_RESOURCE_STATE_COPY_DEST;
      break;
    }

    D3D12MA::Allocation* allocation = nullptr;
    DX_REQUIRE(
        SUCCEEDED(RHI::get().dxGetAllocator()->CreateResource(&allocDesc, &desc, initialState, nullptr, &allocation, IID_NULL, NULL)),
        "Failed to create buffer resource");

    return Buffer(info.getName(), info.getSize(), info.getUsage(), info.getType(), allocation);
  }

  kt::Result<Buffer, HRESULT, S_OK> Buffer::reallocate(size_t newSize) { return Buffer::create({newSize, usage, type, name.c_str()}); }

  void Buffer::destroy() {
    if (allocation) {
      allocation->Release();
      allocation = nullptr;
    }
  }
  Buffer::operator ID3D12Resource*() const { return allocation ? allocation->GetResource() : nullptr; }

  Buffer::Buffer(Buffer&& other) noexcept
      : name(std::move(other.name)), _size(other._size), usage(other.usage), type(other.type), mapPtr(other.mapPtr),
        allocation(other.allocation) {
    other.allocation = nullptr;
    other.mapPtr = nullptr;
  }
  Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
      name = std::move(other.name);
      _size = other._size;
      type = other.type;
      usage = other.usage;
      mapPtr = other.mapPtr;
      allocation = other.allocation;

      other.allocation = nullptr;
      other.mapPtr = nullptr;
    }
    return *this;
  }

  Buffer::~Buffer() { destroy(); }
  D3D12MA::Allocation* Buffer::dxTakeAllocation() {
    auto* alloc = allocation;
    allocation = nullptr;
    return alloc;
  }

  Buffer::Buffer(std::string name, size_t size, Bitflag<BufferUsage> usage, BufferType type, D3D12MA::Allocation* allocation)
      : name(std::move(name)), _size(size), usage(usage), type(type), allocation(allocation) {
    if (type != BufferType::Default) {
      CD3DX12_RANGE readRange(0, 0);
      if (type == BufferType::Readback) {
        readRange.End = size;
      }

      if (FAILED(allocation->GetResource()->Map(0, &readRange, &mapPtr))) {
        DX_WARN("Failed to map buffer resource: {}", this->name);
      }
    }
  }

  ID3D12Resource* Buffer::operator->() const { return allocation->GetResource(); }

  Buffer::operator BufferRef() const { return BufferRef(name.c_str(), *this, _size, mapPtr); }
} // namespace kt::rhi