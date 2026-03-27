#pragma once

#include "keptech/vulkan/helpers/device.hpp"
#include "keptech/vulkan/helpers/shader.hpp"
#include "keptech/vulkan/helpers/swapchain.hpp"
#include "keptech/vulkan/material.hpp"
#include "keptech/vulkan/texture.hpp"
#include <expected>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <keptech/core/base.hpp>
#include <keptech/core/components/transform.hpp>
#include <keptech/core/image.hpp>
#include <keptech/core/maths/frustum.hpp>
#include <keptech/core/maths/transform.hpp>
#include <keptech/core/moveGuard.hpp>
#include <keptech/core/scene.hpp>
#include <keptech/core/slotmap.hpp>
#include <keptech/rendering/mesh.hpp>
#include <keptech/rendering/renderer.hpp>
#include <keptech/rendering/texture.hpp>
#include <keptech/vulkan/structs.hpp>
#include <memory>
#include <string>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace kt::vkh {
  class Renderer {
  public:
    using Shader = vkh::Shader;
    using Pipeline = vkh::LoadedPipeline;
    using Texture = vkh::Texture;

    std::expected<Renderer, std::string> static create(
        const RendererCreateInfo& createInfo,
        const core::window::Window& window);

    struct Limits {
      VkDeviceSize minUniformBufferOffsetAlignment;
    };

    struct Queues {
      Queue graphics;
      Queue present;
      Queue compute;
      Queue transfer;
    };

    struct Pools {
      std::shared_ptr<CommandPool> graphics;
      std::shared_ptr<CommandPool> compute;

      void resetAll();
    };

    struct PerFrame {
      VkFence inFlightFence;
      VkSemaphore imageAvailableSemaphore;
      VkSemaphore timelineSemaphore;
      uint64_t timelineValue = 0;
      Pools pools;
    };

    struct VulkanCore {
      VkInstance instance;
      VkSurfaceKHR surface;
      Device device;
      VmaAllocator allocator;
      Queues queues;
      Swapchain swapchain;
      std::array<PerFrame, MAX_FRAMES_IN_FLIGHT> perFrame;
      CommandPool transferPool;
    };

    struct ImGuiVkObjects {
      VkDescriptorPool descriptorPool;
      VkSampler sampler;
    };

    struct Frame {
      constexpr static uint8_t INVALID_INDEX = 255;

      uint8_t index = 0;
      uint8_t nextIndex = 1;
      uint8_t imageIndex = INVALID_INDEX;
      PerFrame* perFrame = nullptr;
      bool suboptimalSwapchain = false;
    };

    std::expected<PipelinePtr, std::string>
    createPipeline(PipelineCreateInfo createInfo);

    std::expected<std::vector<ImgPtr>, std::string>
    createImages(const std::vector<ImageCreateInfo>& imageInfos);

    std::expected<std::vector<ImgPtr>, std::string>
    createImages(const std::vector<ImageUploadInfo>& imageInfos);

    std::expected<SamplerPtr, std::string>
    createSampler(const SamplerCreateInfo&);

    void loadImGuiImageHandle(ImgPtr& texture);

    [[nodiscard]] bool canRenderToFormat(TextureFormat format) const;
    [[nodiscard]] TextureFormat backbufferFormat() const;

    void newFrame();

    std::expected<VkCommandBuffer, std::string> startFrame();

    struct SubmitInfo {
      std::vector<VkCommandBuffer> cmds;
      std::vector<VkBuffer> trackedBuffers;
      std::vector<AllocatedImage> trackedTextures;
    };
    void submitCommandBuffers(std::vector<SubmitInfo>);

    void renderImGui(VkCommandBuffer graphicsCmd);
    void endFrame(VkCommandBuffer graphicsCmd);
    void present();

    void initImGui();
    void shutdownImGui();

    void preExit();

    [[nodiscard]] bool hasMoved() const noexcept { return m.moveGuard.moved(); }

    Renderer() = delete;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) noexcept;
    Renderer& operator=(Renderer&&) noexcept;
    ~Renderer();

    struct SubmittedCommandBufferInfo {
      VkCommandBuffer buffer;
      std::vector<VkBuffer> trackedBuffers{};
    };

    struct Members {
      MoveGuard moveGuard{};

      const core::window::Window* window;
      VulkanCore vkcore;
      Limits limits;

      std::optional<ImGuiVkObjects> imGuiObjects;

      AllocatedBuffer cameraBuffer;

      DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT> globalDescriptorSets;

      std::array<std::vector<SubmittedCommandBufferInfo>, MAX_FRAMES_IN_FLIGHT>
          submittedCommandBuffers;

      Frame frameInfo{};

      std::array<std::vector<std::shared_ptr<vkh::Texture>>,
                 MAX_FRAMES_IN_FLIGHT>
          textureDescriptorsToUpdate;
      size_t nextTextureIndex = 0;
    };

  private:
    Renderer(Members&& m) : m(std::move(m)) {
      m.frameInfo.perFrame = &this->m.vkcore.perFrame[0];
    }

    [[nodiscard]] const VkFormat& getSwapchainImageFormat() const {
      return m.vkcore.swapchain.config().format.format;
    }

    std::expected<void, std::string> recreateSwapchain();

    Members m;
  };

  namespace setup {
    std::expected<Swapchain, std::string>
    createSwapchain(const VkPhysicalDevice& physicalDevice,
                    glm::ivec2 framebufferSize, const VkDevice& device,
                    const VkSurfaceKHR& surface, const Renderer::Queues& queues,
                    std::optional<VkSwapchainKHR*> oldSwapchain);
  }
} // namespace kt::vkh
