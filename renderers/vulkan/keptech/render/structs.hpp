#pragma once

#include <Volk/volk.h>
#include <vk_mem_alloc.h>

namespace kt::rdr {

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

  template <size_t N> struct DescriptorPoolSet {
    VkDescriptorPool pool;
    VkDescriptorSetLayout layout;
    std::array<VkDescriptorSet, N> sets;
  };

} // namespace kt::rdr
