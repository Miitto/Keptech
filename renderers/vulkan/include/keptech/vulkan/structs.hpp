#pragma once

#include <expected>
#include <vk_mem_alloc.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {

  template <size_t N> struct DescriptorPoolSet {
    vk::raii::DescriptorPool pool;
    vk::raii::DescriptorSetLayout layout;
    std::array<vk::raii::DescriptorSet, N> sets;
  };

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

    static std::expected<AllocatedImage, std::string>
    create(vma::Allocator& allocator, const vk::raii::Device& device,
           const vk::ImageCreateInfo& imgInfo,
           const vma::AllocationCreateInfo& allocInfo,
           vk::ImageViewCreateInfo viewInfo, bool useSameFormat = false,
           std::optional<std::string> name = std::nullopt);

    void destroy(vma::Allocator& allocator, const vk::raii::Device& d);
  };

  struct AllocatedBuffer {
    vk::Buffer buffer;
    vma::Allocation alloc;
    vma::AllocationInfo allocInfo;

    [[nodiscard]] uint8_t* mapping(vk::DeviceSize offset = 0) const {
      return static_cast<uint8_t*>(allocInfo.pMappedData) + offset;
    }

    void setDebugName(const vk::raii::Device& device,
                      const std::string& name) const {}

    static std::expected<AllocatedBuffer, std::string>
    create(vma::Allocator& allocator, const vk::BufferCreateInfo& bufInfo,
           const vma::AllocationCreateInfo& allocInfo,
           const std::optional<std::string>& name = std::nullopt);

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
           const vma::AllocationCreateInfo& allocInfo,
           const std::optional<std::string>& name = std::nullopt);

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
