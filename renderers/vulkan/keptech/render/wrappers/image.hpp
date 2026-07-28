#pragma once

#include "keptech/core/fwd.hpp"
#include "keptech/render/types.hpp"
#include "keptech/rendering/interface/image.hpp"
#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>

namespace kt::rdr {
  struct TransitionInfo;
  class Device;

  class ImageCreateInfo;

  class Image {
  public:
    using Type = ImageType;
    using Format = VkFormat;
    using TransitionType = VkImageMemoryBarrier2;
    using TransitionInfoType = TransitionInfo;

    static constexpr VkImageLayout UNDEFINED_LAYOUT = VK_IMAGE_LAYOUT_UNDEFINED;

    constexpr Image() = default;
    static kt::Result<Image, VkResult, VK_SUCCESS> create(const ImageCreateInfo& info);

    void destroy();

    constexpr operator VkImage() const { return image; }
    constexpr operator VkImageView() const { return view; }

    [[nodiscard]] const glm::uvec3 extent() const;
    [[nodiscard]] constexpr VkFormat format() const { return _format; }
    constexpr ImageType type() const { return _type; }
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
        : _type(other._type), _mips(other._mips), _layers(other._layers), image(other.image), view(other.view), alloc(other.alloc),
          _extent(other._extent), _format(other._format), _handle(other._handle) {
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
    ~Image() { destroy(); }

    [[nodiscard]]
    VmaAllocation getAllocation() const {
      return alloc;
    }

  private:
    constexpr Image(ImageType type, VkImage image, VkImageView view, VmaAllocation alloc, VkExtent3D extent, VkFormat format)
        : _type(type), image(image), view(view), alloc(alloc), _extent(extent), _format(format) {}

    Type _type{};
    uint8_t _mips = 1;
    uint8_t _layers = 1;
    VkImage image{};
    VkImageView view{};
    VmaAllocation alloc{};
    VkExtent3D _extent{};
    VkFormat _format{};
    ImageHandle _handle = INVALID_HANDLE;
  };
} // namespace kt::rdr