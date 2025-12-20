#pragma once

#include <expected>
#include <vk_mem_alloc.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {

  struct Queue {
    uint32_t index;
    std::shared_ptr<vk::raii::Queue> queue;

    vk::raii::Queue& operator*() { return *queue; }
    vk::raii::Queue* operator->() { return queue.get(); }

    operator const vk::raii::Queue&() const { return *queue; }
    operator vk::raii::Queue*() const { return queue.get(); }
    operator vk::raii::Queue&() { return *queue; }
  };

  struct CommandPool {
    vk::raii::CommandPool pool;
    Queue queue;

    vk::raii::CommandPool& operator*() { return pool; }
    vk::raii::CommandPool* operator->() { return &pool; }

    operator const vk::raii::CommandPool&() const { return pool; }
    operator const vk::raii::CommandPool*() const { return &pool; }
    operator vk::raii::CommandPool&() { return pool; }
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

    [[nodiscard]] uint8_t* mapping(vk::DeviceSize offset = 0) const {
      return static_cast<uint8_t*>(allocInfo.pMappedData) + offset;
    }

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

  struct AddressedAllocatedBuffer : public AllocatedBuffer {
    vk::DeviceAddress address = 0;

    static std::expected<AddressedAllocatedBuffer, std::string>
    create(const vk::raii::Device& device, vma::Allocator& allocator,
           const vk::BufferCreateInfo& bufInfo,
           const vma::AllocationCreateInfo& allocInfo);

    static std::expected<AddressedAllocatedBuffer, std::string>
    fromAllocatedBuffer(const vk::raii::Device& desvice,
                        const AllocatedBuffer& allocatedBuffer);
  };

  struct OnGoingCmdTransfer {
    vk::raii::CommandBuffer cmdBuffer;
    AllocatedBuffer buffer;
    vk::raii::Fence fence;

    [[nodiscard]] bool finished() const {
      auto status = fence.getStatus();
      return status == vk::Result::eSuccess;
    }
  };
} // namespace keptech::vkh
