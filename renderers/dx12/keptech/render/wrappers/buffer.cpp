#include "buffer.hpp"

#include "D3D12MemAlloc.h"
#include "bufferCreateInfo.hpp"
#include "d3dx12.h"
#include "dx-logger.hpp"
#include "keptech/render/renderer.hpp"

namespace kt::rdr {
  const std::string& Buffer::getName() const { return name; }
  size_t Buffer::size() const { return _size; }
  bool Buffer::isMapped() const { return mapPtr != nullptr; }
  MappingMode Buffer::getMappingMode() const { return mappingMode; }
  void* Buffer::mapping() const { return mapPtr; }

  kt::Result<Buffer, HRESULT, 0> Buffer::create(const BufferCreateInfo& info) {
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(info.getSize());

    D3D12MA::ALLOCATION_DESC allocDesc{};

    switch (info.getMappingMode()) {
    case MappingMode::None:
      allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
      break;
    case MappingMode::SeqWrite:
    case MappingMode::RandomWrite:
      allocDesc.HeapType = D3D12_HEAP_TYPE_GPU_UPLOAD; // TODO: Should probably check that ReBAR is enabled, and otherwise use normal UPLOAD
      break;
    case MappingMode::Read:
      allocDesc.HeapType = D3D12_HEAP_TYPE_READBACK;
      break;
    }

    D3D12MA::Allocation* allocation = nullptr;
    DX_REQUIRE(SUCCEEDED(Renderer::get().getMembers().allocator->CreateResource(&allocDesc, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                                                &allocation, IID_NULL, NULL)),
               "Failed to create buffer resource");

    return Buffer(info.getName(), info.getSize(), info.getMappingMode(), allocation);
  }

  void Buffer::destroy() {
    if (allocation) {
      allocation->Release();
      allocation = nullptr;
    }
  }

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

  Buffer::Buffer(std::string name, size_t size, MappingMode mappingMode, D3D12MA::Allocation* allocation)
      : name(std::move(name)), _size(size), mappingMode(mappingMode), allocation(allocation) {}

} // namespace kt::rdr