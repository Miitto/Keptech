#pragma once

#include "keptech/vulkan/structs.hpp"
#include <keptech/core/rendering/buffer.hpp>

namespace keptech::vkh {
  class Buffer final : public keptech::IBuffer {
  public:
    Buffer(vma::Allocator& allocator, AddressedAllocatedBuffer buffer)
        : IBuffer(buffer.allocInfo.size), allocator(&allocator),
          buffer(buffer) {}

    [[nodiscard]] void* getMapping() const final {
      return static_cast<uint8_t*>(buffer.allocInfo.pMappedData);
    }

    [[nodiscard]] uint64_t getDeviceAddress() const final {
      return buffer.address;
    }

    [[nodiscard]] const AddressedAllocatedBuffer& getBuffer() const {
      return buffer;
    }

    Buffer(const Buffer&) = delete;
    Buffer(Buffer&& o) noexcept
        :
#ifdef KT_ADD_RESOURCE_INFO
          IBuffer(o.size, std::move(o.debugName), o.usageFlags, o.memoryType),
#else
          IBuffer(o.size),
#endif
          allocator(o.allocator), buffer(o.buffer) {
      o.allocator = nullptr;
    }
    Buffer& operator=(const Buffer&) = delete;
    Buffer& operator=(Buffer&& o) noexcept {
      if (this != &o) {
        size = o.size;
        allocator = o.allocator;
        buffer = o.buffer;
        o.allocator = nullptr;

#ifdef KT_ADD_RESOURCE_INFO
        debugName = std::move(o.debugName);
        memoryType = o.memoryType;
        usageFlags = o.usageFlags;
#endif
      }
      return *this;
    }
    ~Buffer() final {
      if (allocator)
        buffer.destroy(*allocator);
    }

#ifdef KT_ADD_RESOURCE_INFO
    Buffer(std::string name, vma::Allocator& allocator,
           AddressedAllocatedBuffer buffer, Bitflag<BufferUsage> usage,
           BufferMemoryType memoryType)
        : IBuffer(buffer.allocInfo.size, std::move(name), usage, memoryType),
          allocator(&allocator), buffer(buffer) {}
#endif

  private:
    vma::Allocator* allocator;
    AddressedAllocatedBuffer buffer;
  };
} // namespace keptech::vkh
