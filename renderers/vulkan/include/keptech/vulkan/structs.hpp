#pragma once

#include <expected>
#include <vk_mem_alloc.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {
  struct Queue {
    uint32_t index;
    std::shared_ptr<vk::raii::Queue> queue;
  };

  struct AllocatedImage {
    vk::Image image;
    vk::ImageView view;
    vma::Allocation alloc;
    vk::Extent3D extent;
    vk::Format format;

    void destroy(vma::Allocator& allocator, const vk::raii::Device& d) {
      if (image) {
        allocator.destroyImage(image, alloc);
        d.getDispatcher()->vkDestroyImageView(
            static_cast<VkDevice>(*d), static_cast<VkImageView>(view), nullptr);
        image = nullptr;
        view = nullptr;
        alloc = nullptr;
      }
    }
  };

  struct AllocatedBuffer {
    vk::Buffer buffer;
    vma::Allocation alloc;
    vma::AllocationInfo allocInfo;

    static std::expected<AllocatedBuffer, std::string>
    create(vma::Allocator& allocator, const vk::BufferCreateInfo& bufInfo,
           const vma::AllocationCreateInfo& allocInfo);

    inline void destroy(vma::Allocator& allocator) {
      if (alloc) {
        allocator.destroyBuffer(buffer, alloc);
        buffer = nullptr;
        alloc = nullptr;
      }
    }
  };

} // namespace keptech::vkh
