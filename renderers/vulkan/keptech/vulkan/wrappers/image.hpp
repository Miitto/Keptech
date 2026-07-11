#pragma once

#include "keptech/vulkan/helpers/transitions.hpp"
#include "keptech/vulkan/types.hpp"
#include <Volk/volk.h>
#include <expected>
#include <memory>
#include <string>
#include <vma/vk_mem_alloc.h>

namespace kt::vkh {
  class Image {
  public:
    constexpr Image() = default;
    static std::expected<Image, std::string> create(const VmaAllocator& allocator, const VkDevice& device, const VkImageCreateInfo& imgInfo,
                                                    const VmaAllocationCreateInfo& allocInfo, VkImageViewCreateInfo viewInfo,
                                                    const std::string& name, bool useSameFormat = false);

    void destroy(const VmaAllocator& allocator, const VkDevice& d);

    constexpr operator VkImage() const { return image; }
    constexpr operator VkImageView() const { return view; }

    [[nodiscard]] constexpr const VkExtent3D& extent() const { return _extent; }
    [[nodiscard]] constexpr VkFormat format() const { return _format; }

    [[nodiscard]] VkImageMemoryBarrier2 transition(const TransitionInfo& transition, uint32_t srcIndex = VK_QUEUE_FAMILY_IGNORED,
                                                   uint32_t dstIndex = VK_QUEUE_FAMILY_IGNORED) const {
      auto b = transition.image(image);
      b.srcQueueFamilyIndex = srcIndex;
      b.dstQueueFamilyIndex = dstIndex;
      return b;
    }

    [[nodiscard]] constexpr bool isDestroyed() const { return *destroyed; }

    [[nodiscard]] bool hasHandle() const { return _handle != INVALID_HANDLE; }
    [[nodiscard]] ImageHandle handle() const { return _handle; }

    void setHandle(ImageHandle handle) { _handle = handle; }

  private:
    constexpr Image(VkImage image, VkImageView view, VmaAllocation alloc, VkExtent3D extent, VkFormat format)
        : image(image), view(view), alloc(alloc), _extent(extent), _format(format), destroyed(std::make_shared<bool>(false)) {}

    VkImage image{};
    VkImageView view{};
    VmaAllocation alloc{};
    VkExtent3D _extent{};
    VkFormat _format{};
    ImageHandle _handle = INVALID_HANDLE;
    std::shared_ptr<bool> destroyed = std::make_shared<bool>(true);
  };
} // namespace kt::vkh