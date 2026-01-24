#pragma once

#include "keptech/vulkan/helpers/device.hpp"
#include "keptech/vulkan/helpers/shader.hpp"
#include "keptech/vulkan/helpers/swapchain.hpp"
#include "keptech/vulkan/material.hpp"
#include "keptech/vulkan/mesh.hpp"
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
#include <keptech/core/renderer.hpp>
#include <keptech/core/rendering/mesh.hpp>
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
  class Renderer {
  public:
    using Shader = keptech::vkh::Shader;
    using Mesh = vkh::Mesh;
    using Pipeline = vkh::LoadedPipeline;

    static inline constexpr const char* getName() { return "VulkanRenderer"; }

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

    struct GBuffers {
      /// RGB8Unorm albedo, SV_Target0
      AllocatedImage color;
      /// RGB8Unorm normal, SV_Target1
      AllocatedImage normal;
      AllocatedImage depth;
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

    struct ImGuiGBufferHandles {
      VkDescriptorSet albedo;
      VkDescriptorSet normal;
      VkDescriptorSet depth;
    };

    struct ImGuiVkObjects {
      vk::raii::DescriptorPool descriptorPool;
      vk::raii::Sampler gBufferSampler;
      ImGuiGBufferHandles gBufferHandles;
    };

    struct Frame {
      constexpr static uint8_t INVALID_INDEX = 255;

      uint8_t index = INVALID_INDEX;
      uint8_t imageIndex = INVALID_INDEX;
      std::reference_wrapper<PerFrame> perFrame;
      bool suboptimalSwapchain = false;
    };

  private:
    Renderer(const core::window::Window& window, VulkanCore&& vkcore,
             ImGuiVkObjects&& imGuiObjects, AllocatedBuffer cameraBuffer,
             GBuffers gBuffer,
             DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT>&& globalDescriptorSets)
        : window(&window), vkcore(std::move(vkcore)),
          imGuiObjects(std::move(imGuiObjects)), cameraBuffer(cameraBuffer),
          gBuffer(gBuffer),
          globalDescriptorSets(std::move(globalDescriptorSets)) {}

  public:
    std::expected<Renderer, std::string> static create(
        const core::renderer::CreateInfo& createInfo,
        const core::window::Window& window);

    Renderer() = delete;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) noexcept = default;
    Renderer& operator=(Renderer&&) noexcept = default;

    inline core::rendering::AttachmentConfig
    deferredPipelineAttachmentConfig() const {
      using ac = core::rendering::AttachmentConfig;
      using F = core::rendering::Texture::Format;
      return ac{.colorFormats = {F::RGBA8, F::RGBA8},
                .depthFormat = F::Depth16};
    }

    inline const glm::vec2 getGBufferSize() const {
      return glm::vec2{gBuffer.color.extent.width, gBuffer.color.extent.height};
    }

    inline const ImGuiGBufferHandles& getImGuiGBufferHandles() const {
      return imGuiObjects.gBufferHandles;
    }

    std::expected<std::vector<core::rendering::Mesh::Handle>, std::string>
    loadMesh(const std::string_view path, bool backgroundLoad = false);
    std::expected<core::rendering::Mesh::Handle, std::string>
    meshFromData(const core::rendering::MeshData& meshData,
                 bool backgroundLoad = false);
    void unloadMesh(const core::rendering::Mesh::Handle mesh);

    vkh::Mesh* getMeshData(const core::rendering::Mesh::Handle mesh);

    core::rendering::Mesh::SmartHandle
    toSmartHandle(const core::rendering::Mesh::Handle handle) {
      return {core::SlotMapRawSmartHandle(
          handle, [this, handle]() { unloadMesh(handle); })};
    }

    template <typename Func>
    void operateOnAllMeshes(Func func)
      requires(
          std::is_invocable_v<Func, core::rendering::Mesh::Handle, vkh::Mesh&>)
    {
      for (auto handle : loadedMeshes.handles()) {
        auto mesh = loadedMeshes.get(handle);
        func(handle, *mesh);
      }
    }

    std::expected<core::rendering::Pipeline::Handle, std::string>
    createPipeline(core::rendering::Pipeline::CreateInfo createInfo);

    void unloadPipeline(const core::rendering::Pipeline::Handle handle);

    vkh::LoadedPipeline*
    getPipelineData(const core::rendering::Pipeline::Handle handle);

    core::rendering::Pipeline::SmartHandle
    toSmartHandle(const core::rendering::Pipeline::Handle handle) {
      return {core::SlotMapRawSmartHandle(
          handle, [this, handle]() { unloadPipeline(handle); })};
    }

    template <typename Func>
    void operateOnAllPipelines(Func func)
      requires(std::is_invocable_v<Func, core::rendering::Pipeline::Handle,
                                   vkh::LoadedPipeline&>)
    {
      for (auto handle : loadedPipelines.handles()) {
        auto material = loadedPipelines.get(handle);
        func(handle, *material);
      }
    }

    std::expected<core::rendering::TextureHandle, std::string>
    createTexture(glm::uvec3 size, core::rendering::Texture::Format format,
                  core::Bitflag<core::rendering::Texture::Usage> usage,
                  uint32_t mipLevels, bool cpuAccess = false,
                  const void* data = nullptr);
    std::expected<core::rendering::TextureHandle, std::string>
    createTexture(const core::Image& image,
                  core::rendering::Texture::Usage usage,
                  bool cpuAccess = false);

    void newFrame();

    void setScene(core::Scene& scene) { frameScene = &scene; }

    void render();

    ~Renderer();

  private:
    [[nodiscard]] const vk::Format& getSwapchainImageFormat() const {
      return vkcore.swapchain.config().format.format;
    }

    struct VkRenderObject {
      keptech::maths::Transform transform;
      vkh::LoadedPipeline* pipeline = nullptr;
      vkh::Mesh* mesh = nullptr;
      std::span<uint8_t> instanceData;
    };

    struct ObjectLists {
      std::vector<VkRenderObject> deferred;
      std::vector<VkRenderObject> forward;
      std::vector<VkRenderObject> transparent;
    };

    struct PrimaryDrawData {
      ObjectLists objLists;
      components::Camera* camera = nullptr;
      components::Transform* cameraTransform = nullptr;
    };

    ObjectLists buildRenderObjectLists(core::Scene& scene,
                                       const maths::Frustum& frustum);
    void
    setupGraphicsCommandBuffer(const Frame& info,
                               const vk::raii::CommandBuffer& graphicsCmdBuffer,
                               const components::Camera& camera);

    std::expected<void, std::string> recreateSwapchain();

    Frame startFrame();
    PrimaryDrawData setupFrameData(const Frame& info,
                                   vk::raii::CommandBuffer& graphicsCmdBuffer);
    void imagesToRenderable(const Frame& info,
                            const vk::raii::CommandBuffer& graphicsCmdBuffer);
    void drawDeferred(const Frame& info, const PrimaryDrawData& drawData,
                      const vk::raii::CommandBuffer& graphicsCmdBuffer);
    void gBufferToAttachments(const Frame& info,
                              const vk::raii::CommandBuffer& graphicsCmdBuffer);
    void drawImGui(const Frame& info,
                   const vk::raii::CommandBuffer& graphicsCmdBuffer);
    void presentFrame(const Frame& info);
    void endFrame();

    inline void registerCommandBuffer(uint8_t frameIndex,
                                      vk::raii::CommandBuffer&& commandBuffer) {
      submittedCommandBuffers[frameIndex].emplace_back(
          std::move(commandBuffer));
    }

    inline void checkCompletedCommandBuffers() {
      auto [first, last] = std::ranges::remove_if(
          ongoingCommandBuffers,
          [](const OnGoingCmdTransfer& ongoing) { return ongoing.finished(); });

      for (auto it = first; it != last; ++it) {
        it->buffer.destroy(vkcore.allocator);
      }
      ongoingCommandBuffers.erase(first, last);
    }

  private:
    core::MoveGuard moveGuard = core::MoveGuard{};

    const core::window::Window* window;
    VulkanCore vkcore;
    ImGuiVkObjects imGuiObjects;

    DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT> globalDescriptorSets;

    AllocatedBuffer cameraBuffer;

    GBuffers gBuffer;

    std::array<std::vector<vk::raii::CommandBuffer>, MAX_FRAMES_IN_FLIGHT>
        submittedCommandBuffers;

    uint8_t thisFrameIndex = 0;

    std::vector<OnGoingCmdTransfer> ongoingCommandBuffers = {};

    core::SlotMap<vkh::Mesh> loadedMeshes = {};
    core::SlotMap<vkh::LoadedPipeline> loadedPipelines = {};
    core::SlotMap<AllocatedImage> loadedTextures = {};

    core::Scene* frameScene = nullptr;
  };

  namespace setup {
    std::expected<Swapchain, std::string>
    createSwapchain(const vk::raii::PhysicalDevice& physicalDevice,
                    glm::ivec2 framebufferSize, const vk::raii::Device& device,
                    const vk::raii::SurfaceKHR& surface,
                    const Renderer::Queues& queues,
                    std::optional<vk::raii::SwapchainKHR*> oldSwapchain);
  }
} // namespace keptech::vkh
