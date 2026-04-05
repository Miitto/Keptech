#pragma once

#include "keptech/rendering/gltf/data.hpp"
#include "keptech/rendering/texture.hpp"
#include "keptech/vulkan/helpers/device.hpp"
#include "keptech/vulkan/helpers/swapchain.hpp"
#include <expected>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <keptech/components/transform.hpp>
#include <keptech/core/base.hpp>
#include <keptech/core/image.hpp>
#include <keptech/core/moveGuard.hpp>
#include <keptech/core/scene.hpp>
#include <keptech/core/slotmap.hpp>
#include <keptech/maths/frustum.hpp>
#include <keptech/maths/transform.hpp>
#include <keptech/rendering/gltf/scene.hpp>
#include <keptech/rendering/mesh.hpp>
#include <keptech/rendering/renderer.hpp>
#include <keptech/shaders/shader.h>
#include <keptech/vulkan/structs.hpp>
#include <string>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace kt::vkh {

  class Renderer {
    // Structs
  public:
    struct ImGuiVkObjects {
      VkDescriptorPool descriptorPool;
      VkSampler sampler;
    };

    struct TextureUpdateInfo {
      AllocatedImage texture;
      size_t indexInDescriptorSet;
    };

    struct PerFrame {
      VkFence inFlightFence;
      VkSemaphore imageAvailableSemaphore;
      Pools pools;
      std::vector<TextureUpdateInfo> texToUpdate;
    };

    struct Frame {
      constexpr static uint8_t INVALID_INDEX = 255;

      uint8_t index = 0;
      uint8_t nextIndex = 1;
      uint8_t imageIndex = INVALID_INDEX;
      PerFrame* perFrame = nullptr;
      bool suboptimalSwapchain = false;
    };

    struct Buffers {
      using B = AddressedAllocatedBuffer;
      B camera;
    };

    struct Pipelines {
      Pipeline basic;
      Pipeline deferred;
      Pipeline deferredPointLight;
      Pipeline deferredCombine;
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

    struct Members {
      MoveGuard moveGuard{};

      const core::window::Window* window;
      VulkanCore vkcore;

      ImGuiVkObjects imGuiObjects;

      Buffers buffers;
      Pipelines pipelines;

      DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT> globalDescriptorSets;

      Frame frameInfo{};

      size_t nextTextureIndex = 0;

      std::vector<AllocatedImage> loadedTextures{};
      std::vector<AllocatedBuffer> loadedBuffers{};
    };

    // Creation and destruction
  public:
    std::expected<Renderer, std::string> static create(const RendererCreateInfo& createInfo, const core::window::Window& window);

    std::expected<gltf::Scene, std::string> loadMesh(std::string_view path);
    std::expected<std::vector<Texture>, std::string> createImages(const std::vector<ImageUploadInfo>& imageInfos);

    void loadImGuiImageHandle(AllocatedImage& texture);

    std::expected<std::vector<Mesh>, std::string> uploadMeshes(const std::vector<gltf::MeshData>& meshes);

    void setScene(Scene& scene) { this->scene = &scene; }

    Renderer() = delete;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) noexcept;
    Renderer& operator=(Renderer&&) noexcept;
    ~Renderer();

    // Util
  public:
    [[nodiscard]] bool canRenderToFormat(VkFormat format) const;
    [[nodiscard]] VkFormat backbufferFormat() const;
    [[nodiscard]] bool hasMoved() const noexcept { return m.moveGuard.moved(); }

    // Render
  public:
    void newFrame();
    void render();

  private:
    Renderer(Members&& m) : m(std::move(m)) { m.frameInfo.perFrame = &this->m.vkcore.perFrame[0]; }

    void startFrame();

    void drawDeferred(VkCommandBuffer cmdBuf);

    void renderImGui(VkCommandBuffer graphicsCmd);
    void endFrame(VkCommandBuffer graphicsCmd);
    void present();

    void imGuiNewFrame() const;
    void shutdownImGui();
    [[nodiscard]] const VkFormat& getSwapchainImageFormat() const { return m.vkcore.swapchain.config().format.format; }

    std::expected<void, std::string> recreateSwapchain();

  private:
    Members m;
    Scene* scene = nullptr;
  };

} // namespace kt::vkh
