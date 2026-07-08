#include "image.hpp"
#include "keptech/vulkan/macros.hpp"
namespace kt::vkh {

  std::expected<Image, std::string> Image::create(const VmaAllocator& allocator, const VkDevice& device, const VkImageCreateInfo& imgInfo,
                                                  const VmaAllocationCreateInfo& allocInfo, VkImageViewCreateInfo viewInfo,
                                                  const std::string& name, bool useSameFormat) {
    VmaAllocation alloc{};
    VmaAllocationInfo aInfo = {};
    VkImage image = VK_NULL_HANDLE;
    VK_MAKE(vmaCreateImage(allocator, &imgInfo, &allocInfo, &image, &alloc, &aInfo), "Failed to create allocated image");

    viewInfo.image = image;
    if (useSameFormat) {
      viewInfo.format = imgInfo.format;
    }

    VkImageView view = VK_NULL_HANDLE;
    VK_MAKE(vkCreateImageView(device, &viewInfo, nullptr, &view), "Failed to create image view for allocated image");

    VK_TRACE("Created image [{}] with size {} bytes, format {}, extent: {}x{}x{}", name, aInfo.size, imgInfo.format, imgInfo.extent.width,
             imgInfo.extent.height, imgInfo.extent.depth);

#ifndef NDEBUG
    if (!name.empty()) {
      vmaSetAllocationName(allocator, alloc, name.c_str());
    }
#endif

    return Image(image, view, alloc, imgInfo.extent, imgInfo.format);
  }

  void Image::destroy(const VmaAllocator& allocator, const VkDevice& d) {
    if (image) {
      vmaDestroyImage(allocator, image, alloc);
      vkDestroyImageView(d, view, nullptr);
      image = nullptr;
      view = nullptr;
      alloc = nullptr;
    }
  }
} // namespace kt::vkh