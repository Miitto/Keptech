#include "buffer.hpp"

#include "D3D12MemAlloc.h"
#include "d3dx12.h"
#include "dx-logger.hpp"
#include "keptech/rhi/bufferCreateInfo.hpp"
#include "rhi.hpp"

namespace kt::rhi {
  const std::string& Buffer::getName() const { return name; }
  size_t Buffer::size() const { return _size; }
  bool Buffer::isMapped() const { return mapPtr != nullptr; }
  MappingMode Buffer::getMappingMode() const { return mappingMode; }
  void* Buffer::mapping() const { return mapPtr; }
  Bitflag<BufferUsage> Buffer::getUsage() const { return usage; }

  kt::Result<Buffer, HRESULT, 0> Buffer::create(const BufferCreateInfo& info) {
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(info.getSize());

    D3D12MA::ALLOCATION_DESC allocDesc{};
    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

    switch (info.getMappingMode()) {
    case MappingMode::None:
      allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
      break;
    case MappingMode::SeqWrite:
    case MappingMode::RandomWrite:
      allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
      initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
      break;
    case MappingMode::Read:
      allocDesc.HeapType = D3D12_HEAP_TYPE_READBACK;
      initialState = D3D12_RESOURCE_STATE_COPY_DEST;
      break;
    }

    D3D12MA::Allocation* allocation = nullptr;
    DX_REQUIRE(SUCCEEDED(RHI::get().getAllocator()->CreateResource(&allocDesc, &desc, initialState, nullptr, &allocation, IID_NULL, NULL)),
               "Failed to create buffer resource");

    return Buffer(info.getName(), info.getSize(), info.getUsage(), info.getMappingMode(), allocation);
  }

  void Buffer::destroy() {
    if (allocation) {
      allocation->Release();
      allocation = nullptr;
    }
  }
  Buffer::operator ID3D12Resource*() const { return allocation ? allocation->GetResource() : nullptr; }

  Buffer::Buffer(Buffer&& other) noexcept
      : name(std::move(other.name)), _size(other._size), mappingMode(other.mappingMode), mapPtr(other.mapPtr),
        allocation(other.allocation) {
    other.allocation = nullptr;
    other.mapPtr = nullptr;
  }
  Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
      name = std::move(other.name);
      _size = other._size;
      mappingMode = other.mappingMode;
      mapPtr = other.mapPtr;
      allocation = other.allocation;

      other.allocation = nullptr;
      other.mapPtr = nullptr;
    }
    return *this;
  }

  Buffer::~Buffer() { destroy(); }
  D3D12MA::Allocation* Buffer::takeAllocation() {
    auto* alloc = allocation;
    allocation = nullptr;
    return alloc;
  }

  Buffer::Buffer(std::string name, size_t size, Bitflag<BufferUsage> usage, MappingMode mappingMode, D3D12MA::Allocation* allocation)
      : name(std::move(name)), _size(size), usage(usage), mappingMode(mappingMode), allocation(allocation) {
    if (mappingMode != MappingMode::None) {
      CD3DX12_RANGE readRange(0, 0);
      if (mappingMode == MappingMode::Read) {
        readRange.End = size;
      }

      if (FAILED(allocation->GetResource()->Map(0, &readRange, &mapPtr))) {
        DX_WARN("Failed to map buffer resource: {}", this->name);
      }
    }
  }

} // namespace kt::rhi