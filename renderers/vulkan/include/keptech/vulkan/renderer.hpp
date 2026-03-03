#pragma once

#include "keptech/core/rendering/buffer.hpp"
#include "keptech/core/rendering/commandBuffer.hpp"
#include "keptech/core/rendering/renderer.hpp"
#include "keptech/vulkan/helpers/device.hpp"
#include "keptech/vulkan/helpers/shader.hpp"
#include "keptech/vulkan/helpers/swapchain.hpp"
#include "keptech/vulkan/material.hpp"
#include "keptech/vulkan/texture.hpp"
#include <expected>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <keptech/core/base.hpp>
#include <keptech/core/components/renderObject.hpp>
#include <keptech/core/components/transform.hpp>
#include <keptech/core/image.hpp>
#include <keptech/core/maths/frustum.hpp>
#include <keptech/core/maths/transform.hpp>
#include <keptech/core/moveGuard.hpp>
#include <keptech/core/rendering/mesh.hpp>
#include <keptech/core/rendering/renderer.hpp>
#include <keptech/core/rendering/texture.hpp>
#include <keptech/core/scene.hpp>
#include <keptech/core/slotmap.hpp>
#include <keptech/vulkan/structs.hpp>
#include <memory>
#include <string>
#include <vk_mem_alloc.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::core::window {
  class Window;
}

namespace keptech::components {
  class Camera;
}

namespace keptech::vkh {
  class RendererBackend final : public keptech::IRendererBackend {
  public:
    using Shader = keptech::vkh::Shader;
    using Pipeline = vkh::LoadedPipeline;
    using Texture = vkh::Texture;

    std::expected<RendererBackend, std::string> static create(
        const RendererCreateInfo& createInfo,
        const core::window::Window& window);

    struct Limits {
      vk::DeviceSize minUniformBufferOffsetAlignment;
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
      vk::raii::Fence inFlightFence;
      vk::raii::Semaphore imageAvailableSemaphore;
      vk::raii::Semaphore timelineSemaphore;
      uint64_t timelineValue = 0;
      Pools pools;
    };

    struct VulkanCore {
      vk::raii::Context context;
      vk::raii::Instance instance;
      vk::raii::SurfaceKHR surface;
      Device device;
      vma::Allocator allocator;
      Queues queues;
      Swapchain swapchain;
      std::array<PerFrame, MAX_FRAMES_IN_FLIGHT> perFrame;
      CommandPool transferPool;
    };

    struct ImGuiVkObjects {
      vk::raii::DescriptorPool descriptorPool;
      vk::raii::Sampler sampler;
    };

    struct Frame {
      constexpr static uint8_t INVALID_INDEX = 255;

      uint8_t index = 0;
      uint8_t nextIndex = 1;
      uint8_t imageIndex = INVALID_INDEX;
      PerFrame* perFrame = nullptr;
      bool suboptimalSwapchain = false;
    };

    std::expected<BufPtr, std::string>
    createBuffer(const BufferCreateInfo&) final;

    std::expected<PipelinePtr, std::string>
    createPipeline(PipelineCreateInfo createInfo) final;

    std::expected<std::vector<ImgPtr>, std::string>
    createImages(const std::vector<ImageCreateInfo>& imageInfos) final;

    std::expected<std::vector<ImgPtr>, std::string>
    createImages(const std::vector<ImageUploadInfo>& imageInfos) final;

    std::expected<SamplerPtr, std::string>
    createSampler(const SamplerCreateInfo&) final;

    void loadImGuiImageHandle(ImgPtr& texture) final;

    std::expected<CmdBufPtr, std::string> createCmdBuffer(CmdBufType) final;

    void textureLayoutTransition(const CmdBufPtr&,
                                 const std::vector<TextureTransition>&) final;

    [[nodiscard]] bool canRenderToFormat(TextureFormat format) const final;
    [[nodiscard]] TextureFormat backbufferFormat() const final;

    void waitIdle() final { m.vkcore.device.logical.waitIdle(); }

    void newFrame() final;

    std::expected<CmdBufPtr, std::string> startFrame() final;

    void writeCameraMatrices(const CmdBufPtr&,
                             const components::Camera::Uniforms&) final;

    void bindGlobalDescriptorSets(const CmdBufPtr&, const IPipeline& pipeline,
                                  Bitflag<shaders::ShaderStages>) final;

    void submitCommandBuffers(std::vector<SubmitInfo>) final;

    void renderImGui(const CmdBufPtr&) final;
    void endFrame(CmdBufPtr&& graphicsCmdBuffer) final;
    void present();

    void initImGui() final;
    void shutdownImGui() final;

    void preExit() final;

    [[nodiscard]] bool hasMoved() const noexcept { return m.moveGuard.moved(); }

    RendererBackend() = delete;
    RendererBackend(const RendererBackend&) = delete;
    RendererBackend& operator=(const RendererBackend&) = delete;
    RendererBackend(RendererBackend&&) noexcept;
    RendererBackend& operator=(RendererBackend&&) noexcept;
    ~RendererBackend() final;

    struct SubmittedCommandBufferInfo {
      vk::raii::CommandBuffer buffer;
      std::vector<BufPtr> trackedBuffers{};
    };

    struct Members {
      core::MoveGuard moveGuard = core::MoveGuard{};

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
    RendererBackend(Members&& m) : m(std::move(m)) {
      m.frameInfo.perFrame = &this->m.vkcore.perFrame[0];
    }

    [[nodiscard]] const vk::Format& getSwapchainImageFormat() const {
      return m.vkcore.swapchain.config().format.format;
    }

    std::expected<void, std::string> recreateSwapchain();

    Members m;
  };

  namespace setup {
    std::expected<Swapchain, std::string>
    createSwapchain(const vk::raii::PhysicalDevice& physicalDevice,
                    glm::ivec2 framebufferSize, const vk::raii::Device& device,
                    const vk::raii::SurfaceKHR& surface,
                    const RendererBackend::Queues& queues,
                    std::optional<vk::raii::SwapchainKHR*> oldSwapchain);
  }
} // namespace keptech::vkh
