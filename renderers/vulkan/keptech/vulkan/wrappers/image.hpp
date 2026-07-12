#pragma once

#include "keptech/rendering/interface/image.hpp"
#include "keptech/vulkan/types.hpp"
#include <Volk/volk.h>
#include <expected>
#include <memory>
#include <string>
#include <vma/vk_mem_alloc.h>

namespace kt::vkh {
  struct TransitionInfo;

  class Image {
  public:
    using Type = rendering::ImageType;
    using Format = VkFormat;
    using TransitionType = VkImageMemoryBarrier2;
    using TransitionInfoType = TransitionInfo;
    static constexpr VkImageLayout UNDEFINED_LAYOUT = VK_IMAGE_LAYOUT_UNDEFINED;

    constexpr Image() = default;
    static std::expected<Image, std::string> create(const VmaAllocator& allocator, const VkDevice& device, const VkImageCreateInfo& imgInfo,
                                                    const VmaAllocationCreateInfo& allocInfo, VkImageViewCreateInfo viewInfo,
                                                    const std::string& name, bool useSameFormat = false);

    void destroy(const VmaAllocator& allocator, const VkDevice& d);

    constexpr operator VkImage() const { return image; }
    constexpr operator VkImageView() const { return view; }

    [[nodiscard]] const glm::uvec3 extent() const;
    [[nodiscard]] constexpr VkFormat format() const { return _format; }
    constexpr rendering::ImageType type() const { return _type; }
    constexpr uint8_t mips() const { return _mips; }
    constexpr uint8_t layers() const { return _layers; }

    [[nodiscard]] VkImageMemoryBarrier2 transition(const TransitionInfo& transition, uint32_t srcIndex = VK_QUEUE_FAMILY_IGNORED,
                                                   uint32_t dstIndex = VK_QUEUE_FAMILY_IGNORED) const;

    [[nodiscard]] constexpr bool isDestroyed() const { return *destroyed; }

    [[nodiscard]] bool hasHandle() const { return _handle != INVALID_HANDLE; }
    [[nodiscard]] ImageHandle handle() const { return _handle; }

    void setHandle(ImageHandle handle) { _handle = handle; }

  private:
    constexpr Image(rendering::ImageType type, VkImage image, VkImageView view, VmaAllocation alloc, VkExtent3D extent, VkFormat format)
        : _type(type), image(image), view(view), alloc(alloc), _extent(extent), _format(format), destroyed(std::make_shared<bool>(false)) {}

    Type _type{};
    uint8_t _mips = 1;
    uint8_t _layers = 1;
    VkImage image{};
    VkImageView view{};
    VmaAllocation alloc{};
    VkExtent3D _extent{};
    VkFormat _format{};
    ImageHandle _handle = INVALID_HANDLE;
    std::shared_ptr<bool> destroyed = std::make_shared<bool>(true);
  };
} // namespace kt::vkh