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
    using MaterialHandle = core::rendering::Material::Handle;
    using MeshHandle = core::rendering::Mesh::Handle;

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

    struct PerFrame {
      vk::raii::Fence inFlightFence;
      vk::raii::Semaphore imageAvailableSemaphore;
      Pools pools;
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
      GBuffers gBuffer;
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

    struct CameraObjects {
      vk::raii::DescriptorSetLayout layout;
      vk::raii::DescriptorPool pool;
      vk::raii::DescriptorSet descriptorSet;
      AllocatedBuffer uniformBuffer;
    };

    struct Frame {
      constexpr static uint8_t INVALID_INDEX = 255;

      uint8_t index = INVALID_INDEX;
      uint8_t imageIndex = INVALID_INDEX;
      std::reference_wrapper<PerFrame> perFrame;
      bool suboptimalSwapchain = false;
    };

    struct InstanceData {
      glm::mat4 modelMatrix;
    };

    struct InstanceBuffers {
      AllocatedBuffer staging;
      AddressedAllocatedBuffer device;

      [[nodiscard]] size_t maxInstances() const {
        return staging.allocInfo.size / sizeof(InstanceData);
      }

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

  private:
    Renderer(const core::window::Window& window, VulkanCore&& vkcore,
             ImGuiVkObjects&& imGuiObjects, CameraObjects&& cameraObjects,
             InstanceBuffers instanceBuffers)
        : window(&window), vkcore(std::move(vkcore)),
          imGuiObjects(std::move(imGuiObjects)),
          cameraObjects(std::move(cameraObjects)),
          instanceBuffers(instanceBuffers) {}

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
      return glm::vec2{vkcore.gBuffer.color.extent.width,
                       vkcore.gBuffer.color.extent.height};
    }

    inline const ImGuiGBufferHandles& getImGuiGBufferHandles() const {
      return imGuiObjects.gBufferHandles;
    }

    std::expected<std::vector<core::rendering::Mesh::Handle>, std::string>
    loadMesh(const std::string_view path, bool backgroundLoad = false);
    std::expected<core::rendering::Mesh::Handle, std::string>
    meshFromData(const core::rendering::MeshData& meshData,
                 bool backgroundLoad = false);
    void unloadMesh(const std::string& name);
    std::optional<core::rendering::Mesh::Handle>
    getMesh(const std::string& name);

    vkh::Mesh* getMeshData(const core::rendering::Mesh::Handle& handle);

    std::expected<core::rendering::Material::Handle, std::string>
    createMaterial(std::string name, const Material::CreateInfo& createInfo);

    vkh::Material*
    getMaterialData(const core::rendering::Material::Handle& handle);

    std::expected<Shader, std::string>
    createShader(const unsigned char* const code, size_t size);

    std::expected<core::rendering::Texture::Handle, std::string>
    createTexture(glm::uvec3 size, core::rendering::Texture::Format format,
                  core::Bitflag<core::rendering::Texture::Usage> usage,
                  uint32_t mipLevels, bool cpuAccess = false,
                  const void* data = nullptr);
    std::expected<core::rendering::Texture::Handle, std::string>
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
      vkh::Material* material = nullptr;
      vkh::Mesh* mesh = nullptr;
    };

    struct ObjectLists {
      std::vector<VkRenderObject> deferred;
      std::vector<VkRenderObject> forward;
      std::vector<VkRenderObject> transparent;
    };

    ObjectLists buildRenderObjectLists(core::Scene& scene,
                                       const maths::Frustum& frustum);

    std::expected<void, std::string> recreateSwapchain();

    Frame startFrame();
    void
    setupGraphicsCommandBuffer(const Frame& info,
                               const vk::raii::CommandBuffer& graphicsCmdBuffer,
                               const components::Camera& camera);
    void imagesToRenderable(const Frame& info,
                            const vk::raii::CommandBuffer& graphicsCmdBuffer);
    void drawDeferred(const Frame& info,
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
    CameraObjects cameraObjects;

    InstanceBuffers instanceBuffers;

    std::array<std::vector<vk::raii::CommandBuffer>, MAX_FRAMES_IN_FLIGHT>
        submittedCommandBuffers;

    uint8_t thisFrameIndex = 0;

    std::vector<OnGoingCmdTransfer> ongoingCommandBuffers = {};

    core::SlotMap<vkh::Mesh> loadedMeshes = {};
    core::SlotMap<vkh::Material> loadedMaterials = {};
    core::SlotMap<AllocatedImage> loadedTextures = {};
    std::unordered_map<std::string, core::SlotMapWeakHandle> meshNameMap = {};

    core::Scene* frameScene;
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
