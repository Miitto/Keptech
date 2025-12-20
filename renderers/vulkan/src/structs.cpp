#include <keptech/vulkan/structs.hpp>

#include "macros.hpp"

namespace keptech::vkh {
  std::expected<AllocatedBuffer, std::string>
  AllocatedBuffer::create(vma::Allocator& allocator,
                          const vk::BufferCreateInfo& bufInfo,
                          const vma::AllocationCreateInfo& allocInfo) {
    vma::AllocationInfo aInfo = {};
    VMA_MAKE(buffer, allocator.createBuffer(bufInfo, allocInfo, aInfo),
             "Failed to create allocated buffer");

    return AllocatedBuffer{
        .buffer = buffer.first,
        .alloc = buffer.second,
        .allocInfo = aInfo,
    };
  }

  std::expected<AddressedAllocatedBuffer, std::string>
  AddressedAllocatedBuffer::create(const vk::raii::Device& device,
                                   vma::Allocator& allocator,
                                   const vk::BufferCreateInfo& bufInfo,
                                   const vma::AllocationCreateInfo& allocInfo) {
    auto allocatedBufferRes =
        AllocatedBuffer::create(allocator, bufInfo, allocInfo);
    if (!allocatedBufferRes) {
      return std::unexpected(allocatedBufferRes.error());
    }

    return fromAllocatedBuffer(device, *allocatedBufferRes);
  }

  std::expected<AddressedAllocatedBuffer, std::string>
  AddressedAllocatedBuffer::fromAllocatedBuffer(
      const vk::raii::Device& device, const AllocatedBuffer& allocatedBuffer) {
    vk::BufferDeviceAddressInfo addressInfo{.buffer = allocatedBuffer.buffer};
    vk::DeviceAddress address = device.getBufferAddress(addressInfo);

    AddressedAllocatedBuffer buf{
        .address = address,
    };

    buf.buffer = allocatedBuffer.buffer;
    buf.alloc = allocatedBuffer.alloc;
    buf.allocInfo = allocatedBuffer.allocInfo;

    return buf;
  }
} // namespace keptech::vkh
