#pragma once

#include "keptech/core/image.hpp"
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace kt::vkh {

  struct RenderFormats {
    VkFormat albedo = VK_FORMAT_B8G8R8A8_SRGB;
    VkFormat normal = VK_FORMAT_UNDEFINED;
    VkFormat emissive = VK_FORMAT_UNDEFINED;
    VkFormat metRought = VK_FORMAT_UNDEFINED;
    VkFormat depth = VK_FORMAT_UNDEFINED;
    VkFormat hdr = VK_FORMAT_UNDEFINED;
  };

  struct TextureFormats {
    VkFormat albedo = VK_FORMAT_UNDEFINED;
    VkFormat normal = VK_FORMAT_UNDEFINED;
    VkFormat metRough = VK_FORMAT_UNDEFINED;
    VkFormat emissive = VK_FORMAT_UNDEFINED;
  };

  struct Formats {
    RenderFormats render{};
    TextureFormats texture{};
    VkFormat swapchain = VK_FORMAT_UNDEFINED;
  };

  struct VertexInput {
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
  };

  struct Pipeline {
    VkPipelineLayout layout;
    VkPipeline pipeline;
  };

  struct ImageCreateInfo {
    VkFormat format;
    VkExtent3D extent;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    VkImageUsageFlags usage;
  };

  struct ImageUploadInfo {
    const Image* image;
    VkImageUsageFlags usage;
    uint32_t mipLevels;
  };

  template <size_t N> struct DescriptorPoolSet {
    VkDescriptorPool pool;
    VkDescriptorSetLayout layout;
    std::array<VkDescriptorSet, N> sets;
  };

  struct Queue {
    uint32_t index = ~0;
    VkQueue queue = nullptr;
  };

  struct CommandPool {
    VkCommandPool pool = nullptr;
    Queue queue{};
  };

  struct Queues {
    Queue graphics;
    Queue present;
    Queue compute;
    Queue transfer;
  };

  struct Pools {
    CommandPool graphics{};
    CommandPool compute{};

    void resetAll(VkDevice device);
  };

  struct AllocatedImage {
    VkImage image;
    VkImageView view;
    VmaAllocation alloc;
    VkExtent3D extent;
    VkFormat format;

    static std::expected<AllocatedImage, std::string> create(const VmaAllocator& allocator, const VkDevice& device,
                                                             const VkImageCreateInfo& imgInfo, const VmaAllocationCreateInfo& allocInfo,
                                                             VkImageViewCreateInfo viewInfo, bool useSameFormat = false,
                                                             std::optional<std::string> name = std::nullopt);

    void destroy(const VmaAllocator& allocator, const VkDevice& d);
  };

  struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation alloc;
    VmaAllocationInfo allocInfo;
    std::shared_ptr<bool> destroyed = std::make_shared<bool>(true); // If it hasn't been created yet, it's already "destroyed"

    [[nodiscard]] bool isMapped() const { return allocInfo.pMappedData != nullptr; }
    [[nodiscard]] uint8_t* mapping(VkDeviceSize offset = 0) const { return static_cast<uint8_t*>(allocInfo.pMappedData) + offset; }

    void setDebugName(const VkDevice& device, const std::string& name) const {}

    static std::expected<AllocatedBuffer, std::string> create(const VmaAllocator& allocator, VkDevice device,
                                                              const VkBufferCreateInfo& bufInfo, const VmaAllocationCreateInfo& allocInfo,
                                                              const std::optional<std::string>& name = std::nullopt);

    inline void destroy(VmaAllocator& allocator) {
      if (alloc && !*destroyed) {
        vmaDestroyBuffer(allocator, buffer, alloc);
        buffer = nullptr;
        alloc = nullptr;
        *destroyed = true;
      }
    }
  };

  struct AddressedAllocatedBuffer : public AllocatedBuffer {
    VkDeviceAddress address = 0;

    static std::expected<AddressedAllocatedBuffer, std::string> create(const VkDevice& device, const VmaAllocator& allocator,
                                                                       const VkBufferCreateInfo& bufInfo,
                                                                       const VmaAllocationCreateInfo& allocInfo,
                                                                       const std::optional<std::string>& name = std::nullopt);

    static std::expected<AddressedAllocatedBuffer, std::string> fromAllocatedBuffer(const VkDevice& desvice,
                                                                                    const AllocatedBuffer& allocatedBuffer);

    AllocatedBuffer downcast() { return AllocatedBuffer{.buffer = buffer, .alloc = alloc, .allocInfo = allocInfo}; }
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

  struct RendererMesh {
    AddressedAllocatedBuffer vertexBuffer;
    AddressedAllocatedBuffer indexBuffer;
  };
} // namespace kt::vkh
