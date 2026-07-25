#include "image.hpp"
#include "helpers/transitions.hpp"
#include "keptech/core/result.hpp"
#include "keptech/vulkan/macros.hpp"
#include "wrappers/device.hpp"
#include <glm/vec3.hpp>

namespace kt::vkh {
  using namespace kt::rendering;

  kt::Result<Image, VkResult, VK_SUCCESS> Image::create(const Device& device, const VkImageCreateInfo& imgInfo,
                                                        const VmaAllocationCreateInfo& allocInfo, VkImageViewCreateInfo viewInfo,
                                                        const char* name, bool useSameFormat) {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation alloc{};
    VmaAllocationInfo aInfo = {};
    auto res = vmaCreateImage(device, &imgInfo, &allocInfo, &image, &alloc, &aInfo);
    if (res != VK_SUCCESS) {
      return {res};
    }

    viewInfo.image = image;
    if (useSameFormat) {
      viewInfo.format = imgInfo.format;
    }

    VkImageView view = VK_NULL_HANDLE;
    res = vkCreateImageView(device, &viewInfo, nullptr, &view);
    if (res != VK_SUCCESS) {
      return {res};
    }

    VK_TRACE("Created image [{}] with size {} bytes, format {}, extent: {}x{}x{}", name, aInfo.size, imgInfo.format, imgInfo.extent.width,
             imgInfo.extent.height, imgInfo.extent.depth);

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

    Image i(device, imgType, image, view, alloc, imgInfo.extent, imgInfo.format);
    if (name) {
      device.setAllocationName(i.alloc, name);
      device.setDebugName(i, name);
    }

    return std::move(i);
  }

  void Image::destroy() {
    if (image) {
      vmaDestroyImage(device, image, alloc);
      vkDestroyImageView(device, view, nullptr);
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

  VkImageSubresourceRange Image::getSubresourceRange() const {
    VkImageAspectFlags aspectMask = 0;
    switch (_format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
      aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      break;
    case VK_FORMAT_S8_UINT:
      aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
    default:
      aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      break;
    }
    return VkImageSubresourceRange{
        .aspectMask = aspectMask,
        .baseMipLevel = 0,
        .levelCount = _mips,
        .baseArrayLayer = 0,
        .layerCount = _layers,
    };
  }
} // namespace kt::vkh