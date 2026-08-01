#pragma once

#include "keptech/core/fwd.hpp"
#include "keptech/render/interface/image.hpp"
#include "keptech/render/types.hpp"
#include <Volk/volk.h>
#include <string>
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

    operator VkImage() const;
    operator VkImageView() const;

    [[nodiscard]] const glm::uvec3 extent() const;
    [[nodiscard]] VkFormat format() const;
    ImageType type() const;
    uint8_t mips() const;
    uint8_t layers() const;
    VkImageUsageFlags getUsage() const;
    const std::string& getName() const { return name; }

    [[nodiscard]] VkImageMemoryBarrier2 transition(const TransitionInfo& transition, uint32_t srcIndex = VK_QUEUE_FAMILY_IGNORED,
                                                   uint32_t dstIndex = VK_QUEUE_FAMILY_IGNORED) const;

    [[nodiscard]] bool hasHandle() const;
    [[nodiscard]] ImageHandle handle() const;

    void setHandle(ImageHandle handle);

    [[nodiscard]] VkImageSubresourceRange getSubresourceRange() const;

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;
    ~Image();

    VmaAllocation getAllocation() const;

  private:
    Image(std::string name, ImageType type, VkImage image, VkImageView view, VmaAllocation alloc, VkExtent3D extent, VkFormat format,
          VkImageUsageFlags usage);

    std::string name;
    Type _type{};
    uint8_t _mips = 1;
    uint8_t _layers = 1;
    VkImage image{};
    VkImageView view{};
    VmaAllocation alloc{};
    VkExtent3D _extent{};
    VkFormat _format{};
    ImageHandle _handle = INVALID_HANDLE;
    VkImageUsageFlags usage = 0;
  };
} // namespace kt::rdr