#pragma once

#include <Volk/volk.h>
#include <expected>
#include <memory>
#include <string>
#include <vma/vk_mem_alloc.h>

namespace kt::vkh {
  class Image {
  public:
    Image() = default;
    static std::expected<Image, std::string> create(const VmaAllocator& allocator, const VkDevice& device, const VkImageCreateInfo& imgInfo,
                                                    const VmaAllocationCreateInfo& allocInfo, VkImageViewCreateInfo viewInfo,
                                                    const std::string& name, bool useSameFormat = false);

    void destroy(const VmaAllocator& allocator, const VkDevice& d);

    operator VkImage() const { return image; }
    operator VkImageView() const { return view; }

    [[nodiscard]] const VkExtent3D& extent() const { return _extent; }
    [[nodiscard]] VkFormat format() const { return _format; }

  private:
    Image(VkImage image, VkImageView view, VmaAllocation alloc, VkExtent3D extent, VkFormat format)
        : image(image), view(view), alloc(alloc), _extent(extent), _format(format), destroyed(std::make_shared<bool>(false)) {}

    VkImage image{};
    VkImageView view{};
    VmaAllocation alloc{};
    VkExtent3D _extent{};
    VkFormat _format{};
    std::shared_ptr<bool> destroyed = std::make_shared<bool>(true);
  };
} // namespace kt::vkh