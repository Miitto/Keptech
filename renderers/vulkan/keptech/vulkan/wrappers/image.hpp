#pragma once

#include "keptech/core/fwd.hpp"
#include "keptech/rendering/interface/image.hpp"
#include "keptech/vulkan/types.hpp"
#include "keptech/vulkan/wrappers/device.hpp"
#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>

namespace kt::vkh {
  struct TransitionInfo;
  class Device;

  class ImageCreateInfo {
  public:
    constexpr ImageCreateInfo(VkImageType imageType, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage, uint32_t mipLevels = 1,
                              uint32_t arrayLayers = 1, const char* name = nullptr) noexcept
        : imageInfo{
              .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
              .pNext = nullptr,
              .flags = 0,
              .imageType = imageType,
              .format = format,
              .extent = extent,
              .mipLevels = mipLevels,
              .arrayLayers = arrayLayers,
              .samples = VK_SAMPLE_COUNT_1_BIT,
              .tiling = VK_IMAGE_TILING_OPTIMAL,
              .usage = usage,
              .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
              .queueFamilyIndexCount = 0,
              .pQueueFamilyIndices = nullptr,
              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          },
          viewInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                   .pNext = nullptr,
                   .flags = 0,
                   .image = nullptr,
                   .viewType = VK_IMAGE_VIEW_TYPE_2D,
                   .format = format,
                   .components = {.r = VK_COMPONENT_SWIZZLE_R,
                                  .g = VK_COMPONENT_SWIZZLE_G,
                                  .b = VK_COMPONENT_SWIZZLE_B,
                                  .a = VK_COMPONENT_SWIZZLE_A},
                   .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                        .baseMipLevel = 0,
                                        .levelCount = mipLevels,
                                        .baseArrayLayer = 0,
                                        .layerCount = arrayLayers}},
          allocInfo{.flags = 0,
                    .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                    .requiredFlags = 0,
                    .preferredFlags = 0,
                    .memoryTypeBits = 0,
                    .pool = nullptr,
                    .pUserData = nullptr,
                    .priority = 0.f},
          name(name) {
      switch (imageType) {
      case VK_IMAGE_TYPE_1D:
        viewInfo.viewType = arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
        break;
      case VK_IMAGE_TYPE_2D:
        if (arrayLayers == 6) {
          imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
          viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        } else {
          viewInfo.viewType = arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        }
        break;
      default:
        break;
      }

      switch (format) {
      case VK_FORMAT_D16_UNORM:
      case VK_FORMAT_X8_D24_UNORM_PACK32:
      case VK_FORMAT_D32_SFLOAT:
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        break;
      case VK_FORMAT_S8_UINT:
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
        break;
      case VK_FORMAT_D16_UNORM_S8_UINT:
      case VK_FORMAT_D24_UNORM_S8_UINT:
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        break;
      default:
        break;
      }
    }

    void setImageType(VkImageType type) noexcept { imageInfo.imageType = type; }
    void setFormat(VkFormat format) noexcept {
      imageInfo.format = format;
      viewInfo.format = format;
    }
    void setExtent(VkExtent3D extent) noexcept { imageInfo.extent = extent; }
    void setMipLevels(uint32_t mipLevels) noexcept {
      imageInfo.mipLevels = mipLevels;
      viewInfo.subresourceRange.levelCount = mipLevels;
    }
    void setArrayLayers(uint32_t arrayLayers) noexcept {
      imageInfo.arrayLayers = arrayLayers;
      viewInfo.subresourceRange.layerCount = arrayLayers;
    }
    void setSamples(VkSampleCountFlagBits samples) noexcept { imageInfo.samples = samples; }
    void setTiling(VkImageTiling tiling) noexcept { imageInfo.tiling = tiling; }
    void setUsage(VkImageUsageFlags usage) noexcept { imageInfo.usage = usage; }
    void setSharingMode(VkSharingMode sharingMode) noexcept { imageInfo.sharingMode = sharingMode; }
    void setInitialLayout(VkImageLayout layout) noexcept { imageInfo.initialLayout = layout; }
    void setViewType(VkImageViewType viewType) noexcept { viewInfo.viewType = viewType; }
    void setAspectMask(VkImageAspectFlags aspectMask) noexcept { viewInfo.subresourceRange.aspectMask = aspectMask; }
    void setAllocFlags(VmaAllocationCreateFlags flags) noexcept { allocInfo.flags = flags; }
    void setMemoryUsage(VmaMemoryUsage usage) noexcept { allocInfo.usage = usage; }

    [[nodiscard]]
    const VkImageCreateInfo& getImageInfo() const noexcept {
      return imageInfo;
    }
    [[nodiscard]]
    const VkImageViewCreateInfo& getViewInfo() const noexcept {
      return viewInfo;
    }
    [[nodiscard]]
    const VmaAllocationCreateInfo& getAllocInfo() const noexcept {
      return allocInfo;
    }

    [[nodiscard]]
    const char* getName() const noexcept {
      return name;
    }

  private:
    VkImageCreateInfo imageInfo;
    VkImageViewCreateInfo viewInfo;
    VmaAllocationCreateInfo allocInfo;
    const char* name = nullptr;
  };

  class Image {
  public:
    using Type = rendering::ImageType;
    using Format = VkFormat;
    using TransitionType = VkImageMemoryBarrier2;
    using TransitionInfoType = TransitionInfo;

    static constexpr VkImageLayout UNDEFINED_LAYOUT = VK_IMAGE_LAYOUT_UNDEFINED;

    constexpr Image() = default;
    static kt::Result<Image, VkResult, VK_SUCCESS> create(const Device& device, const ImageCreateInfo& info);

    void destroy();

    constexpr operator VkImage() const { return image; }
    constexpr operator VkImageView() const { return view; }

    [[nodiscard]] const glm::uvec3 extent() const;
    [[nodiscard]] constexpr VkFormat format() const { return _format; }
    constexpr rendering::ImageType type() const { return _type; }
    constexpr uint8_t mips() const { return _mips; }
    constexpr uint8_t layers() const { return _layers; }

    [[nodiscard]] VkImageMemoryBarrier2 transition(const TransitionInfo& transition, uint32_t srcIndex = VK_QUEUE_FAMILY_IGNORED,
                                                   uint32_t dstIndex = VK_QUEUE_FAMILY_IGNORED) const;

    [[nodiscard]] bool hasHandle() const { return _handle != INVALID_HANDLE; }
    [[nodiscard]] ImageHandle handle() const { return _handle; }

    void setHandle(ImageHandle handle) { _handle = handle; }

    [[nodiscard]] VkImageSubresourceRange getSubresourceRange() const;

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&& other) noexcept
        : _type(other._type), _mips(other._mips), _layers(other._layers), image(other.image), view(other.view), device(other.device),
          alloc(other.alloc), _extent(other._extent), _format(other._format), _handle(other._handle) {
      other.image = VK_NULL_HANDLE;
      other.view = VK_NULL_HANDLE;
      other.alloc = nullptr;
      other._handle = INVALID_HANDLE;
    }
    Image& operator=(Image&& other) noexcept {
      if (this != &other) {
        _type = other._type;
        _mips = other._mips;
        _layers = other._layers;
        image = other.image;
        view = other.view;
        device = other.device;
        alloc = other.alloc;
        _extent = other._extent;
        _format = other._format;
        _handle = other._handle;

        other.image = VK_NULL_HANDLE;
        other.view = VK_NULL_HANDLE;
        other.alloc = nullptr;
        other._handle = INVALID_HANDLE;
      }
      return *this;
    }
    ~Image() {
      if (device)
        destroy();
    }

    [[nodiscard]]
    VmaAllocation getAllocation() const {
      return alloc;
    }

  private:
    constexpr Image(Device device, rendering::ImageType type, VkImage image, VkImageView view, VmaAllocation alloc, VkExtent3D extent,
                    VkFormat format)
        : _type(type), image(image), view(view), device(device), alloc(alloc), _extent(extent), _format(format) {}

    Type _type{};
    uint8_t _mips = 1;
    uint8_t _layers = 1;
    VkImage image{};
    VkImageView view{};
    Device device{};
    VmaAllocation alloc{};
    VkExtent3D _extent{};
    VkFormat _format{};
    ImageHandle _handle = INVALID_HANDLE;
  };
} // namespace kt::vkh