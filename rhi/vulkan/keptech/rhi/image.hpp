#pragma once

#include "keptech/core/result.hpp"
#include "keptech/rhi/imageCreateInfo.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include "keptech/rhi/result.hpp"
#include <Volk/volk.h>
#include <string>
#include <vma/vk_mem_alloc.h>

namespace kt::rhi {

  class ImageCreateInfo;
  class ImageRef;

  class Image {
#include "keptech/rhi/interface/image.inl"

  public:
    operator VkImage() const noexcept { return image; }
    operator VkImageView() const noexcept { return view; }
    operator VmaAllocation() const noexcept { return alloc; }

    operator bool() const noexcept { return image != VK_NULL_HANDLE && view != VK_NULL_HANDLE && alloc != nullptr; }

  private:
    Image(std::string name, ImageDim type, VkImage image, VkImageView view, VmaAllocation alloc, VkExtent3D extent, VkFormat format,
          VkImageUsageFlags usage);

    std::string name;
    ImageDim _dim{};
    uint8_t _mips = 1;
    uint8_t _layers = 1;
    VkImage image{};
    VkImageView view{};
    VmaAllocation alloc{};
    VkExtent3D _extent{};
    VkFormat _format{};
    ImageUsage usage;
    uint64_t textureIndex = ~0ull;
  };
} // namespace kt::rhi