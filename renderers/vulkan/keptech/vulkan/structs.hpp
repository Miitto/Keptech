#pragma once

#include "keptech/vulkan/wrappers/queue.hpp"
#include <Volk/volk.h>
#include <vector>
#include <vk_mem_alloc.h>

namespace kt::vkh {

  struct RenderFormats {
    VkFormat albedo = VK_FORMAT_B8G8R8A8_SRGB;
    VkFormat position = VK_FORMAT_UNDEFINED;
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

  template <size_t N> struct DescriptorPoolSet {
    VkDescriptorPool pool;
    VkDescriptorSetLayout layout;
    std::array<VkDescriptorSet, N> sets;
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
} // namespace kt::vkh
