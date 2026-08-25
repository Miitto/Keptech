#pragma once

#include "keptech/rhi/helpers/descriptorPoolSet.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include "keptech/rhi/vk/constants.hpp"
#include "keptech/rhi/vk/core.hpp"
#include <Volk/volk.h>
#include <expected>
#include <set>
#include <vma/vk_mem_alloc.h>
#include "keptech/rhi/result.hpp"

#ifdef KT_PROFILE
#include <tracy/TracyVulkan.hpp>
#endif

namespace kt {
  struct RendererCreateInfo;
  class Window;

  namespace shaders {
    struct Shader;
  }

  namespace maths {
    class Frustum;
  }
} // namespace kt

namespace kt::rhi {
  class BufferCreateInfo;
  class ImageCreateInfo;
  class CommandBuffer;
  class ImageRef;
  class BufferRef;
  class Buffer;
  class DescriptorLayout;
  struct DescriptorInfo;
  class DescriptorPool;
  struct DescriptorPoolInfo;

  struct LoadedImage {
    VkImage image;
    VkImageView view;
    VmaAllocation alloc;
  };

  struct Members {
    const Window* window;
    VulkanCore vkcore;

    VkDescriptorPool imGuiDescriptorPool;

    DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT> bindlessDescriptorSets;

    uint8_t frameIndex = 0;
    uint8_t imageIndex = 0;

    bool swapchainSuboptimal = false;

    std::vector<LoadedImage> loadedTextures{};

#ifdef KT_PROFILE
    TracyVkCtx tracyGraphicsContext;
    TracyVkCtx tracyComputeContext;
#endif
  };

  class RHI {
#include "keptech/rhi/interface/rhi.inl"

  public:
    Device& vkGetDevice() { return m.vkcore.device; }

  private:
    std::expected<void, std::string> initInternal(const RendererCreateInfo& createInfo, const Window& window);
    std::expected<void, std::string> initVulkanCore(const RendererCreateInfo& createInfo, const Window& window);
    std::expected<std::set<uint32_t>, std::string> initDevice(const RendererCreateInfo& createInfo);
    std::expected<void, std::string> initPhysicalDevice(const RendererCreateInfo& createInfo);
    std::expected<void, std::string> initLogicalDevice(const RendererCreateInfo& createInfo, const std::set<uint32_t>& uniqueQueueFamilies);
    std::expected<void, std::string> initCommandPools(const std::set<uint32_t>& uniqueQueueFamilies);
    std::expected<void, std::string> initSync();
    std::expected<void, std::string> initSamplers();
    std::expected<void, std::string> initImGui();
    std::expected<void, std::string> initDescriptors();
    std::expected<void, std::string> initBuffers();
    std::expected<void, std::string> initFormats();
    void writeDescriptors();

    void updateTextureDescriptors();
    void updateBufferPointers() const;

    void renderImGui(VkCommandBuffer cmdBuf);

    std::expected<void, std::string> recreateSwapchain();

  private:
    Members m;
  };

} // namespace kt::rhi
