#pragma once

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace kt::vkh {
  template <size_t N> struct DescriptorPoolSet {
    VkDescriptorPool pool;
    VkDescriptorSetLayout layout;
    std::array<VkDescriptorSet, N> sets;
  };

  struct Queue {
    uint32_t index;
    std::shared_ptr<VkQueue> queue;

    VkQueue& operator*() { return *queue; }
    VkQueue* operator->() { return queue.get(); }

    operator const VkQueue&() const { return *queue; }
    operator VkQueue*() const { return queue.get(); }
    operator VkQueue&() { return *queue; }
  };

  struct CommandPool {
    VkCommandPool pool;
    Queue queue;

    VkCommandPool& operator*() { return pool; }
    VkCommandPool* operator->() { return &pool; }

    operator const VkCommandPool&() const { return pool; }
    operator const VkCommandPool*() const { return &pool; }
    operator VkCommandPool&() { return pool; }
  };

  struct AllocatedImage {
    VkImage image;
    VkImageView view;
    VmaAllocation alloc;
    VkExtent3D extent;
    VkFormat format;

    static std::expected<AllocatedImage, std::string>
    create(VmaAllocator& allocator, const VkDevice& device,
           const VkImageCreateInfo& imgInfo,
           const VmaAllocationCreateInfo& allocInfo,
           VkImageViewCreateInfo viewInfo, bool useSameFormat = false,
           std::optional<std::string> name = std::nullopt);

    void destroy(VmaAllocator& allocator, const VkDevice& d);
  };

  struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation alloc;
    VmaAllocationInfo allocInfo;

    [[nodiscard]] uint8_t* mapping(VkDeviceSize offset = 0) const {
      return static_cast<uint8_t*>(allocInfo.pMappedData) + offset;
    }

    void setDebugName(const VkDevice& device, const std::string& name) const {}

    static std::expected<AllocatedBuffer, std::string>
    create(VmaAllocator& allocator, const VkBufferCreateInfo& bufInfo,
           const VmaAllocationCreateInfo& allocInfo,
           const std::optional<std::string>& name = std::nullopt);

    inline void destroy(VmaAllocator& allocator) {
      if (alloc) {
        vmaDestroyBuffer(allocator, buffer, alloc);
        buffer = nullptr;
        alloc = nullptr;
      }
    }
  };

  struct AddressedAllocatedBuffer : public AllocatedBuffer {
    VkDeviceAddress address = 0;

    static std::expected<AddressedAllocatedBuffer, std::string>
    create(const VkDevice& device, VmaAllocator& allocator,
           const VkBufferCreateInfo& bufInfo,
           const VmaAllocationCreateInfo& allocInfo,
           const std::optional<std::string>& name = std::nullopt);

    static std::expected<AddressedAllocatedBuffer, std::string>
    fromAllocatedBuffer(const VkDevice& desvice,
                        const AllocatedBuffer& allocatedBuffer);
  };

  struct OnGoingCmdTransfer {
    VkCommandBuffer cmdBuffer;
    AllocatedBuffer buffer;
    VkFence fence;

    [[nodiscard]] bool finished(VkDevice device) const {
      auto status = vkGetFenceStatus(device, fence);
      return status == VkResult::VK_SUCCESS;
    }
  };
} // namespace kt::vkh
