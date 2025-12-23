#pragma once

#include "keptech/vulkan/helpers/device.hpp"
#include "keptech/vulkan/helpers/pipeline.hpp"
#include "keptech/vulkan/helpers/shader.hpp"
#include "keptech/vulkan/helpers/swapchain.hpp"
#include "keptech/vulkan/material.hpp"
#include "keptech/vulkan/mesh.hpp"
#include <algorithm>
#include <expected>
#include <functional>
#include <keptech/core/components/transform.hpp>
#include <keptech/core/maths/frustum.hpp>
#include <keptech/core/maths/transform.hpp>
#include <keptech/core/moveGuard.hpp>
#include <keptech/core/renderer.hpp>
#include <keptech/core/rendering/mesh.hpp>
#include <keptech/core/rendering/renderObject.hpp>
#include <keptech/core/slotmap.hpp>
#include <keptech/ecs/ecs.hpp>
#include <keptech/vulkan/structs.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vk_mem_alloc.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::core::window {
  class Window;
}

namespace keptech::vkh {
  class Renderer : public core::renderer::Renderer {
  public:
    using Shader = keptech::vkh::Shader;
    using MaterialHandle = core::rendering::Material::Handle;
    using MeshHandle = core::rendering::Mesh::Handle;

    struct Queues {
      Queue graphics;
      Queue present;
      Queue compute;
      Queue transfer;
    };

    struct Pools {
      std::shared_ptr<CommandPool> graphics;
      std::shared_ptr<CommandPool> present;
      std::shared_ptr<CommandPool> compute;

      void resetAll();
    };

    struct SyncObjects {
      vk::raii::Semaphore presentCompleteSemaphore;
      vk::raii::Semaphore renderCompleteSemaphore;
      vk::raii::Fence drawingFence;
    };

    struct FrameResources {
      SyncObjects syncObjects;
      Pools pools;
    };

    struct OldSwapchain {
      vkh::Swapchain swapchain;
      uint8_t frameIndex;
    };

    struct VulkanCore {
      vk::raii::Context context;
      vk::raii::Instance instance;
      vk::raii::SurfaceKHR surface;
      Device device;
      Queues queues;
      Swapchain swapchain;
      std::array<FrameResources, MAX_FRAMES_IN_FLIGHT> frameResources;
      CommandPool transferPool;

      std::optional<OldSwapchain> oldSwapchain = std::nullopt;
    };

    struct ImGuiVkObjects {
      vk::raii::DescriptorPool descriptorPool;
    };

    struct Frame {
      constexpr static uint8_t INVALID_INDEX = 255;

      uint8_t index = INVALID_INDEX;
      uint8_t imageIndex = INVALID_INDEX;
      std::reference_wrapper<SyncObjects> syncObjects;
      std::reference_wrapper<Pools> pools;
    };

  private:
    Renderer(core::window::Window& window, VulkanCore&& vkcore,
             vma::Allocator& allocator, ImGuiVkObjects&& imGuiObjects)
        : window(&window), vkcore(std::move(vkcore)), allocator(allocator),
          imGuiObjects(std::move(imGuiObjects)) {}

    template <typename... Args> static Renderer& addToEcs(Renderer&& renderer) {
      auto& ecs = ecs::ECS::get();
      return ecs.registerSystem<Renderer>(
          ecs.signatureFromComponents<components::Transform,
                                      core::rendering::RenderObject>(),
          std::move(renderer));
    }

  public:
    std::expected<Renderer*, std::string> static create(
        const char* const name, core::window::Window& window);

    Renderer() = delete;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) noexcept = default;
    Renderer& operator=(Renderer&&) noexcept = default;

    std::expected<core::rendering::Mesh::Handle, std::string>
    meshFromData(const std::string& name,
                 std::span<const core::rendering::Mesh::Vertex> vertices,
                 std::span<const uint32_t> indices,
                 std::vector<core::rendering::Mesh::Submesh> submeshes = {},
                 bool backgroundLoad = false);
    void unloadMesh(const std::string& name);
    std::optional<core::rendering::Mesh::Handle>
    getMesh(const std::string& name);

    std::expected<core::rendering::Material::Handle, std::string>
    createMaterial(core::rendering::Material::Stage stage,
                   GraphicsPipelineConfig&& config);
    std::expected<core::rendering::Material::Handle, std::string>
    createMaterial(core::rendering::Material::Stage stage,
                   GraphicsPipelineConfig& config);

    std::expected<Shader, std::string>
    createShader(const unsigned char* const code, size_t size);

    [[nodiscard]] const vk::Format& getSwapchainImageFormat() const {
      return vkcore.swapchain.config().format.format;
    }

    void newFrame();

    void render();

    ~Renderer() override;

  private:
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

    ObjectLists buildRenderObjectLists(const maths::Frustum& frustum);

    void checkSwapchain();
    std::expected<void, std::string> recreateSwapchain();

    Frame startFrame();
    void setupGraphicsCommandBuffer(
        const Frame& info, const vk::raii::CommandBuffer& graphicsCmdBuffer);
    void draw(const Frame& info,
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
        it->buffer.destroy(allocator);
      }
      ongoingCommandBuffers.erase(first, last);
    }

    core::MoveGuard moveGuard = core::MoveGuard{};

    core::window::Window* window;
    VulkanCore vkcore;
    vma::Allocator allocator;
    ImGuiVkObjects imGuiObjects;
    std::array<std::vector<vk::raii::CommandBuffer>, MAX_FRAMES_IN_FLIGHT>
        submittedCommandBuffers;

    uint8_t nextFrameIndex = 0;

    std::vector<OnGoingCmdTransfer> ongoingCommandBuffers = {};

    core::SlotMap<vkh::Mesh> loadedMeshes = {};
    core::SlotMap<vkh::Material> loadedMaterials = {};
    std::unordered_map<std::string, core::SlotMapWeakHandle> meshNameMap = {};
    std::unordered_map<std::string, core::SlotMapWeakHandle> materialNameMap =
        {};
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
