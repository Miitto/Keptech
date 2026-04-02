#include <keptech/vulkan/structs.hpp>

#include <set>
#include <spdlog/fmt/bundled/ranges.h>

#include "macros.hpp"

namespace kt::vkh {
  void Pools::resetAll(VkDevice device) {
    std::set<VkCommandPool> unique{
        &graphics.pool,
        &compute.pool,
    };
    for (auto& pool : unique) {
      vkResetCommandPool(device, pool, 0);
    }
  }
  std::expected<AllocatedImage, std::string> AllocatedImage::create(const VmaAllocator& allocator, const VkDevice& device,
                                                                    const VkImageCreateInfo& imgInfo,
                                                                    const VmaAllocationCreateInfo& allocInfo,
                                                                    VkImageViewCreateInfo viewInfo, bool useSameFormat,
                                                                    std::optional<std::string> name) {
    VmaAllocation alloc;
    VmaAllocationInfo aInfo = {};
    VkImage image;
    VK_MAKE(vmaCreateImage(allocator, &imgInfo, &allocInfo, &image, &alloc, &aInfo), "Failed to create allocated image");

    viewInfo.image = image;
    if (useSameFormat) {
      viewInfo.format = imgInfo.format;
    }

    VkImageView view;
    VK_MAKE(vkCreateImageView(device, &viewInfo, nullptr, &view), "Failed to create image view for allocated image");

    if (name.has_value()) {
      vmaSetAllocationName(allocator, alloc, name->c_str());
    }

    return AllocatedImage{
        .image = image,
        .view = view,
        .alloc = alloc,
        .extent = imgInfo.extent,
        .format = imgInfo.format,
    };
  }

  void AllocatedImage::destroy(const VmaAllocator& allocator, const VkDevice& d) {
    if (image) {
      vmaDestroyImage(allocator, image, alloc);
      vkDestroyImageView(d, view, nullptr);
      image = nullptr;
      view = nullptr;
      alloc = nullptr;
    }
  }

  std::expected<AllocatedBuffer, std::string> AllocatedBuffer::create(const VmaAllocator& allocator, VkDevice device,
                                                                      const VkBufferCreateInfo& bufInfo,
                                                                      const VmaAllocationCreateInfo& allocInfo,
                                                                      const std::optional<std::string>& name) {
    VkBuffer buffer;
    VmaAllocation alloc;
    VmaAllocationInfo aInfo = {};
    VK_MAKE(vmaCreateBuffer(allocator, &bufInfo, &allocInfo, &buffer, &alloc, &aInfo), "Failed to create allocated buffer");

#ifndef NDEBUG
    VkMemoryPropertyFlags props;
    vmaGetAllocationMemoryProperties(allocator, alloc, &props);
    bool isHostVisible = (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
    bool deviceLocal = (props & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
    bool mapped = aInfo.pMappedData != nullptr;

    std::vector<std::string> flags;
    if (isHostVisible)
      flags.emplace_back("Host Visible");
    if (deviceLocal)
      flags.emplace_back("Device Local");
    if (mapped)
      flags.emplace_back("Mapped");

    VK_DEBUG("Created buffer [{}] with size {} bytes. {}", name.value_or(""), aInfo.size, fmt::join(flags, ", "));
#endif

    if (name.has_value()) {
      vmaSetAllocationName(allocator, alloc, name->c_str());
    }

    return AllocatedBuffer{
        .buffer = buffer,
        .alloc = alloc,
        .allocInfo = aInfo,
    };
  }

  std::expected<AddressedAllocatedBuffer, std::string>
  AddressedAllocatedBuffer::create(const VkDevice& device, const VmaAllocator& allocator, const VkBufferCreateInfo& bufInfo,
                                   const VmaAllocationCreateInfo& allocInfo, const std::optional<std::string>& name) {

    auto allocatedBufferRes = AllocatedBuffer::create(allocator, device, bufInfo, allocInfo, name);
    if (!allocatedBufferRes) {
      return std::unexpected(allocatedBufferRes.error());
    }

    return fromAllocatedBuffer(device, *allocatedBufferRes);
  }

  std::expected<AddressedAllocatedBuffer, std::string>
  AddressedAllocatedBuffer::fromAllocatedBuffer(const VkDevice& device, const AllocatedBuffer& allocatedBuffer) {
    VkBufferDeviceAddressInfo addrVknfo{.buffer = allocatedBuffer.buffer};
    VkDeviceAddress address = vkGetBufferDeviceAddress(device, &addrVknfo);

    AddressedAllocatedBuffer buf{
        .address = address,
    };

    buf.buffer = allocatedBuffer.buffer;
    buf.alloc = allocatedBuffer.alloc;
    buf.allocInfo = allocatedBuffer.allocInfo;

    return buf;
  }
} // namespace kt::vkh
