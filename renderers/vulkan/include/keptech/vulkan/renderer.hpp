#pragma once

#include "keptech/core/rendering/renderer.hpp"
#include "keptech/vulkan/helpers/device.hpp"
#include "keptech/vulkan/helpers/shader.hpp"
#include "keptech/vulkan/helpers/swapchain.hpp"
#include "keptech/vulkan/material.hpp"
#include "keptech/vulkan/mesh.hpp"
#include "keptech/vulkan/texture.hpp"
#include <algorithm>
#include <expected>
#include <functional>
#include <imgui/backends/imgui_impl_vulkan.h>
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
#include <unordered_map>
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
    using Mesh = vkh::Mesh;
    using Pipeline = vkh::LoadedPipeline;
    using Texture = vkh::Texture;

    std::expected<RendererBackend, std::string> static create(
        const RendererCreateInfo& createInfo,
        const core::window::Window& window);

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

    struct InstanceData {
      glm::mat4 modelMatrix;
    };

    struct InstanceBuffers {
      AllocatedBuffer staging;
      AddressedAllocatedBuffer device;

      [[nodiscard]] size_t getSize() const { return staging.allocInfo.size; }

      std::expected<InstanceBuffers, std::string> static create(
          vma::Allocator& allocator, vk::raii::Device& device,
          size_t maxInstances);

      void destroy(vma::Allocator& allocator);
      std::expected<void, std::string> resize(vma::Allocator& allocator,
                                              vk::raii::Device& device,
                                              size_t newMaxInstances);

      std::expected<void, std::string>
      copyToDevice(vk::raii::Device& device,
                   const vk::raii::CommandBuffer& cmdBuf,
                   size_t instanceCount = 0);
    };

    struct PerFrame {
      vk::raii::Fence inFlightFence;
      vk::raii::Semaphore imageAvailableSemaphore;
      Pools pools;
      InstanceBuffers instanceBuffers;
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
    };

    struct Frame {
      constexpr static uint8_t INVALID_INDEX = 255;

      uint8_t index = 0;
      uint8_t nextIndex = 1;
      uint8_t imageIndex = INVALID_INDEX;
      PerFrame* perFrame;
      bool suboptimalSwapchain = false;
    };

    std::expected<UPipelinePtr, std::string>
    createPipeline(PipelineCreateInfo createInfo);

    std::expected<UTexPtr, std::string>
    createTexture(std::string name, glm::uvec3 size, TextureFormat format,
                  Bitflag<TextureUsage> usage, uint32_t mipLevels,
                  bool cpuAccess = false, const void* data = nullptr);
    std::expected<UTexPtr, std::string> createTexture(const core::Image& image,
                                                      TextureUsage usage,
                                                      bool cpuAccess = false);

    std::expected<UCmdBufPtr, std::string> createGraphicsCmdBuffer() final;

    void newFrame() final;
    void renderImGui(const UCmdBufPtr&) final;
    void endFrame() final;

    void initImGui() final;

    RendererBackend() = delete;
    RendererBackend(const RendererBackend&) = delete;
    RendererBackend& operator=(const RendererBackend&) = delete;
    RendererBackend(RendererBackend&&) noexcept = default;
    RendererBackend& operator=(RendererBackend&&) noexcept = default;
    ~RendererBackend() final;

  private:
    RendererBackend(
        const core::window::Window& window, VulkanCore&& vkcore,
        ImGuiVkObjects&& imGuiObjects,
        DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT>&& globalDescriptorSets)
        : window(&window), vkcore(std::move(vkcore)),
          imGuiObjects(std::move(imGuiObjects)),
          globalDescriptorSets(std::move(globalDescriptorSets)) {}

    [[nodiscard]] const vk::Format& getSwapchainImageFormat() const {
      return vkcore.swapchain.config().format.format;
    }

    void
    setupGraphicsCommandBuffer(const Frame& info,
                               const vk::raii::CommandBuffer& graphicsCmdBuffer,
                               const components::Camera& camera);

    std::expected<void, std::string> recreateSwapchain();

    void imagesToRenderable(const Frame& info,
                            const vk::raii::CommandBuffer& graphicsCmdBuffer);
    void gBufferToAttachments(const Frame& info,
                              const vk::raii::CommandBuffer& graphicsCmdBuffer);
    void drawImGui(const Frame& info,
                   const vk::raii::CommandBuffer& graphicsCmdBuffer);

    inline void registerCommandBuffer(uint8_t frameIndex,
                                      vk::raii::CommandBuffer&& commandBuffer) {
      submittedCommandBuffers[frameIndex].emplace_back(
          std::move(commandBuffer));
    }

  private:
    core::MoveGuard moveGuard = core::MoveGuard{};

    const core::window::Window* window;
    VulkanCore vkcore;
    ImGuiVkObjects imGuiObjects;

    DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT> globalDescriptorSets;

    std::array<std::vector<vk::raii::CommandBuffer>, MAX_FRAMES_IN_FLIGHT>
        submittedCommandBuffers;

    Frame frameInfo{};
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
