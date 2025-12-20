#include "keptech/vulkan/renderer.hpp"
#include "macros.hpp"

#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/core/renderer.hpp>
#include <keptech/core/window.hpp>
#include <set>

namespace keptech::vkh {
  static_assert(keptech::core::renderer::Renderer<Renderer>,
                "Renderer does not satisfy Renderer concept");

  void Renderer::Pools::resetAll() {
    std::set<vk::raii::CommandPool*> unique{
        &graphics.get()->pool,
        &present.get()->pool,
        &compute.get()->pool,
    };
    for (auto& pool : unique) {
      pool->reset();
    }
  }

  void Renderer::newFrame() {
    renderObjects.clear();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();

    ImGui::NewFrame();
  }

  Renderer::Frame Renderer::startFrame() {
    checkSwapchain();
    checkCompletedCommandBuffers();

    auto& sync = vkcore.frameResources[frameIndex].syncObjects;
    auto nextImageRes = vkcore.swapchain.getNextImage(
        vkcore.device, sync.drawingFence, sync.presentCompleteSemaphore);

    if (!nextImageRes) {
      VK_CRITICAL("Failed to acquire next swapchain image: {}",
                  nextImageRes.error());
      abort();
    }
    auto [imageIndex, swapchainState] = nextImageRes.value();

    if (swapchainState == vkh::Swapchain::State::OutOfDate ||
        swapchainState == vkh::Swapchain::State::Suboptimal) {
      auto res = recreateSwapchain();
      if (!res) {
        VK_CRITICAL("Failed to recreate swapchain: {}", res.error());
        abort();
      }
      // Try again
      return startFrame();
    }

    Frame frameInfo{
        .index = frameIndex,
        .imageIndex = static_cast<uint8_t>(imageIndex),
        .syncObjects = std::ref(vkcore.frameResources[frameIndex].syncObjects),
        .pools = std::ref(vkcore.frameResources[frameIndex].pools),
    };

    frameInfo.pools.get().resetAll();
    submittedCommandBuffers[frameIndex].clear();

    this->frameIndex = (this->frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

    return frameInfo;
  }

  void Renderer::setupGraphicsCommandBuffer(
      const Frame& info, const vk::raii::CommandBuffer& graphicsCmdBuffer) {
    graphicsCmdBuffer.setViewport(
        0,
        vk::Viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(vkcore.swapchain.config().extent.width),
            .height =
                static_cast<float>(vkcore.swapchain.config().extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        });

    graphicsCmdBuffer.setScissor(0,
                                 vk::Rect2D{
                                     .offset = {.x = 0, .y = 0},
                                     .extent = vkcore.swapchain.config().extent,
                                 });
  }

  void Renderer::presentFrame(const Frame& info) {
    auto& sync = info.syncObjects.get();

    uint32_t imageIndex = info.imageIndex;

    vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*sync.renderCompleteSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &*vkcore.swapchain.getSwapchain(),
        .pImageIndices = &imageIndex,
    };

    auto result = vkcore.queues.present.queue->presentKHR(presentInfo);
    if (result == vk::Result::eErrorOutOfDateKHR ||
        result == vk::Result::eSuboptimalKHR) {
      auto res = recreateSwapchain();
      if (!res) {
        VK_CRITICAL("Failed to recreate swapchain: {}", res.error());
        abort();
      }
    }
  }

  void Renderer::endFrame() { ImGui::EndFrame(); }

  Renderer::~Renderer() {
    if (moveGuard.moved()) {
      return;
    }

    vkcore.device.logical.waitIdle();

    auto meshes = loadedMeshes.values();
    checkCompletedCommandBuffers();

    for (auto& mesh : meshes) {
      mesh->destroy(allocator);
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    allocator.destroy();
    vkcore.device.logical.waitIdle();
  }

  std::expected<void, std::string> Renderer::recreateSwapchain() {

    VKH_MAKE(newSwapchain,
             setup::createSwapchain(vkcore.device.physical,
                                    window->getRenderSize(),
                                    vkcore.device.logical, vkcore.surface,
                                    vkcore.queues, &*vkcore.swapchain),
             "Failed to recreate swapchain");

    std::optional<OldSwapchain> oldSwapchain = OldSwapchain{
        .swapchain = std::move(vkcore.swapchain),
        .frameIndex = frameIndex,
    };

    vkcore.swapchain = std::move(newSwapchain);

    return {};
  }

  void Renderer::checkSwapchain() {
    if (!vkcore.oldSwapchain.has_value()) {
      return;
    }

    auto& oldSwapchain = vkcore.oldSwapchain.value();

    if (vkcore.device.logical.waitForFences(
            {vkcore.frameResources[oldSwapchain.frameIndex]
                 .syncObjects.drawingFence},
            VK_TRUE, UINT64_MAX) == vk::Result::eSuccess) {
      vkcore.oldSwapchain.reset();
    }
  }

  std::expected<core::SlotMap<Mesh>::Handle, std::string>
  Renderer::meshFromData(std::span<const Vertex> vertices,
                         std::span<const uint32_t> indices,
                         std::vector<vkh::Mesh::Submesh> submeshes,
                         bool backgroundLoad) {
    Pools& pools =
        vkcore.frameResources[frameIndex].pools; // Use current frame pools

    auto res = vkh::Mesh::fromData(vkcore.device.logical, allocator,
                                   backgroundLoad ? vkcore.transferPool
                                                  : *pools.graphics,
                                   vertices, indices, std::move(submeshes));

    if (!res) {
      return std::unexpected(res.error());
    }

    ongoingCommandBuffers.push_back(std::move(res.value().second));

    auto handle = loadedMeshes.emplace(std::move(res.value().first));

    return handle;
  }

  std::expected<Material, std::string>
  Renderer::createMaterial(Material::Stage stage,
                           GraphicsPipelineConfig&& config) {
    return createMaterial(stage, static_cast<GraphicsPipelineConfig&>(config));
  }

  std::expected<Material, std::string>
  Renderer::createMaterial(Material::Stage stage,
                           GraphicsPipelineConfig& config) {
    auto vkLayoutInfo = config.layout.build();

    VK_MAKE(pipelineLayout,
            vkcore.device.logical.createPipelineLayout(vkLayoutInfo),
            "Failed to create pipeline layout");

    auto vkConfig = config.build();
    vkConfig.layout = *pipelineLayout;

    VK_MAKE(pipeline,
            vkcore.device.logical.createGraphicsPipeline(nullptr, vkConfig),
            "Failed to create graphics pipeline");

    return Material{
        .stage = stage,
        .pipeline = std::move(pipeline),
        .pipelineLayout = std::move(pipelineLayout),
    };
  }

  std::expected<Shader, std::string>
  Renderer::createShader(const unsigned char* const code, size_t size) {
    return Shader::create(vkcore.device.logical, code, size);
  }
} // namespace keptech::vkh
