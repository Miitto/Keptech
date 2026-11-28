#include "buffer.hpp"

#include "D3D12MemAlloc.h"
#include "bufferRef.hpp"
#include "d3dx12.h"
#include "dx/dx-logger.hpp"
#include "keptech/rhi/bufferCreateInfo.hpp"
#include "keptech/rhi/bufferTypes.hpp"
#include "rhi.hpp"

namespace kt::rhi {
  void BufferBase::setName(this Buffer& self, const std::string& newName) {
    self.name = newName;
#ifndef NDEBUG
    if (self.allocation) {
      std::wstring wName(self.name.begin(), self.name.end());
      self.allocation->SetName(wName.c_str());
      self.allocation->GetResource()->SetName(wName.c_str());
    }
#endif
  }
  bool BufferBase::isMapped(this const Buffer& self) { return self.mapPtr != nullptr; }
  void* BufferBase::mapping(this const Buffer& self, size_t offset) { return static_cast<uint8_t*>(self.mapPtr) + offset; }

  bool BufferBase::isValid(this const Buffer& self) { return self.allocation && self.allocation->GetResource() != nullptr; }

  kt::Result<Buffer, RawRhiResult, RawRhiResultOk> BufferBase::create(const BufferCreateInfo& info) {
    DX_ASSERT(info.getSize() > 0, "Buffer {}: size must be greater than 0", info.getName());
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(info.getSize());

    D3D12MA::ALLOCATION_DESC allocDesc{
        .HeapType = D3D12_HEAP_TYPE_DEFAULT,
    };

    switch (info.getType()) {
    case BufferType::Default:
      break;
    case BufferType::GpuMapped:
      allocDesc.HeapType = D3D12_HEAP_TYPE_GPU_UPLOAD;
      break;
    case BufferType::Staging:
      allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
      break;
    case BufferType::Readback:
      allocDesc.HeapType = D3D12_HEAP_TYPE_READBACK;
      break;
    }

    D3D12MA::Allocation* allocation = nullptr;
    DX_REQUIRE(SUCCEEDED(RHI::get().dxGetAllocator()->CreateResource(&allocDesc, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, &allocation,
                                                                     IID_NULL, NULL)),
               "Failed to create buffer resource");

    DX_REQUIRE(allocation != nullptr, "Failed to create buffer resource: allocation is null");

#ifndef NDEBUG
    if (info.getName() != nullptr) {
      auto usage = info.getUsage();
      auto name = fmt::format("Buffer {} [{}]", usage.clear(BufferUsage::TransferDst | BufferUsage::TransferSrc), info.getName());
      std::wstring wname(name.begin(), name.end());
      allocation->SetName(wname.c_str());
      allocation->GetResource()->SetName(wname.c_str());
    }
#endif

#ifndef KT_DISABLE_STATS
    auto& stats = RHI::get().getStats();
    if (info.getFlags().has(BufferCreateFlags::EngineBuffer)) {
      ++stats.engineBufferCount;
      stats.engineBufferMemUsage += allocation->GetSize();
    } else {
      ++stats.userBufferCount;
      stats.userBufferMemUsage += allocation->GetSize();
    }
#endif

    return Buffer(info.getName() == nullptr ? "<unnamed>" : info.getName(), info.getSize(), info.getUsage(), info.getType(),
                  Allocation::unique(allocation));
  }

  kt::Result<Buffer, RawRhiResult, RawRhiResultOk> BufferBase::reallocate(this const Buffer& self, size_t newSize) {
    return Buffer::create({newSize, self.getUsage(), self.getType(), self.getName().c_str()});
  }

  void BufferBase::destroy(this Buffer& self) {
    if (self.allocation) {
#ifndef KT_DISABLE_STATS
      auto& stats = RHI::get().getStats();
      if (self._flags.has(BufferCreateFlags::EngineBuffer)) {
        --stats.engineBufferCount;
        stats.engineBufferMemUsage -= self.allocation->GetSize();
      } else {
        --stats.userBufferCount;
        stats.userBufferMemUsage -= self.allocation->GetSize();
      }
#endif

      self.allocation.reset();
    }
  }
  Buffer::operator ID3D12Resource*() const { return allocation ? allocation->GetResource() : nullptr; }

  Buffer::Buffer(Buffer&& other) noexcept : BufferBase(std::move(other)), mapPtr(other.mapPtr), allocation(std::move(other.allocation)) {
    other.mapPtr = nullptr;
  }

  Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
      BufferBase::operator=(std::move(other));

      mapPtr = other.mapPtr;
      allocation = std::move(other.allocation);

      other.mapPtr = nullptr;
    }
    return *this;
  }

  Buffer::~Buffer() { destroy(); }
  Allocation&& Buffer::dxTakeAllocation() { return std::move(allocation); }

  Buffer::Buffer(std::string name, size_t size, Bitflag<BufferUsage> usage, BufferType type, Allocation allocation)
      : BufferBase(std::move(name), size, usage, type), allocation(allocation) {
    if (type != BufferType::Default) {
      CD3DX12_RANGE readRange(0, 0);
      if (type == BufferType::Readback) {
        readRange.End = size;
      }

      if (FAILED(allocation->GetResource()->Map(0, &readRange, &mapPtr))) {
        DX_WARN("Failed to map buffer resource: {}", getName());
      }
    }
  }

  ID3D12Resource* Buffer::operator->() const { return allocation->GetResource(); }

  BufferBase::operator BufferRef(this const Buffer& self) {
    return BufferRef(self.name.c_str(), self.allocation ? self.allocation->GetResource() : nullptr, self._size, self.mapPtr);
  }
} // namespace kt::rhi