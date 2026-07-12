#include "image.hpp"
#include "helpers/transitions.hpp"
#include "keptech/vulkan/macros.hpp"
#include <glm/vec3.hpp>
namespace kt::vkh {
  using namespace kt::rendering;

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

    ImageType imgType = ImageType::Color;

    switch (imgInfo.format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
      imgType = ImageType::Depth;
      break;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
      imgType = ImageType::DepthStencil;
      break;
    case VK_FORMAT_S8_UINT:
      imgType = ImageType::Stencil;
      break;
    default:
      break;
    }

    return Image(imgType, image, view, alloc, imgInfo.extent, imgInfo.format);
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

  [[nodiscard]] const glm::uvec3 Image::extent() const { return glm::uvec3{_extent.width, _extent.height, _extent.depth}; }

  [[nodiscard]] VkImageMemoryBarrier2 Image::transition(const TransitionInfo& transition, uint32_t srcIndex, uint32_t dstIndex) const {
    auto b = transition.image(image);
    b.srcQueueFamilyIndex = srcIndex;
    b.dstQueueFamilyIndex = dstIndex;
    return b;
  }
} // namespace kt::vkh