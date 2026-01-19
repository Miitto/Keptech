#include <keptech/vulkan/structs.hpp>

#include "macros.hpp"

namespace keptech::vkh {
  std::expected<AllocatedImage, std::string>
  AllocatedImage::create(vma::Allocator& allocator,
                         const vk::raii::Device& device,
                         const vk::ImageCreateInfo& imgInfo,
                         const vma::AllocationCreateInfo& allocInfo,
                         vk::ImageViewCreateInfo viewInfo, bool useSameFormat) {
    vma::AllocationInfo aInfo = {};
    VMA_MAKE(image, allocator.createImage(imgInfo, allocInfo, aInfo),
             "Failed to create allocated image");

    viewInfo.image = image.first;
    if (useSameFormat) {
      viewInfo.format = imgInfo.format;
    }

    VK_MAKE(viewRaii, device.createImageView(viewInfo),
            "Failed to create image view for allocated image");

    vk::ImageView view = viewRaii.release();

    return AllocatedImage{
        .image = image.first,
        .view = view,
        .alloc = image.second,
        .extent = imgInfo.extent,
        .format = imgInfo.format,
    };
  }

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
