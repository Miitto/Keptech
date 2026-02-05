#include <keptech/vulkan/structs.hpp>

#include "macros.hpp"

namespace keptech::vkh {
  std::expected<AllocatedImage, std::string>
  AllocatedImage::create(vma::Allocator& allocator,
                         const vk::raii::Device& device,
                         const vk::ImageCreateInfo& imgInfo,
                         const vma::AllocationCreateInfo& allocInfo,
                         vk::ImageViewCreateInfo viewInfo, bool useSameFormat,
                         std::optional<std::string> name) {
    vma::AllocationInfo aInfo = {};
    VMA_MAKE(image, allocator.createImage(imgInfo, allocInfo, aInfo),
             "Failed to create allocated image");

    if (name.has_value())
      allocator.setAllocationName(image.second, name->c_str());

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

  void AllocatedImage::destroy(vma::Allocator& allocator,
                               const vk::raii::Device& d) {
    if (image) {
      allocator.destroyImage(image, alloc);
      d.getDispatcher()->vkDestroyImageView(
          static_cast<VkDevice>(*d), static_cast<VkImageView>(view), nullptr);
      image = nullptr;
      view = nullptr;
      alloc = nullptr;
    }
  }

  std::expected<AllocatedBuffer, std::string>
  AllocatedBuffer::create(vma::Allocator& allocator,
                          const vk::BufferCreateInfo& bufInfo,
                          const vma::AllocationCreateInfo& allocInfo,
                          const std::optional<std::string>& name) {
    vma::AllocationInfo aInfo = {};
    VMA_MAKE(buffer, allocator.createBuffer(bufInfo, allocInfo, aInfo),
             "Failed to create allocated buffer");

    if (name.has_value())
      allocator.setAllocationName(buffer.second, name->c_str());

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
                                   const vma::AllocationCreateInfo& allocInfo,
                                   const std::optional<std::string>& name) {

    auto allocatedBufferRes =
        AllocatedBuffer::create(allocator, bufInfo, allocInfo);
    if (!allocatedBufferRes) {
      return std::unexpected(allocatedBufferRes.error());
    }

    if (name.has_value())
      allocator.setAllocationName(allocatedBufferRes->alloc, name->c_str());

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
