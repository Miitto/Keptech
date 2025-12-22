#include "keptech/vulkan/renderer.hpp"
#include "macros.hpp"

#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/core/renderer.hpp>
#include <keptech/core/window.hpp>
#include <set>

namespace keptech::vkh {
  static_assert(core::renderer::CRenderer<Renderer>,
                "Renderer does not satisfy CRenderer concept");

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

  Renderer::ObjectLists
  Renderer::buildRenderObjectLists(const maths::Frustum& frustum) {
    ObjectLists lists;

    auto& ecs = ecs::ECS::get();

    for (auto& entity : entities) {
      auto transform = ecs.getComponentRef<components::Transform>(entity);
      auto renderObj =
          ecs.getComponentRef<core::rendering::RenderObject>(entity);

      auto meshP = loadedMeshes.get(renderObj.mesh);
      if (!meshP) {
        VK_WARN("RenderObject has invalid mesh handle, skipping");
        continue;
      }

      auto materialP = loadedMaterials.get(renderObj.material);
      if (!materialP) {
        VK_WARN("RenderObject has invalid material handle, skipping");
        continue;
      }

      auto& mesh = *meshP;
      auto& material = *materialP;

      transform.recalculateGlobalTransform();

      // TODO: Frustum cull

      struct VkRenderObject ro{
          .transform = transform.global,
          .material = &material,
          .mesh = &mesh,
      };

      switch (material.stage) {
      case Material::Stage::Deferred:
        lists.deferred.push_back(ro);
        break;
      case Material::Stage::Forward:
        lists.forward.push_back(ro);
        break;
      case Material::Stage::Transparent:
        lists.transparent.push_back(ro);
        break;
      }
    }
    return lists;
  }

  void Renderer::newFrame() {
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

    loadedMeshes.reset();

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

  std::expected<core::rendering::Mesh::Handle, std::string>
  Renderer::meshFromData(
      std::span<const core::rendering::Mesh::Vertex> vertices,
      std::span<const uint32_t> indices,
      std::vector<vkh::Mesh::Submesh> submeshes, bool backgroundLoad) {
    Pools& pools =
        vkcore.frameResources[frameIndex].pools; // Use current frame pools

    auto res = vkh::Mesh::fromData(vkcore.device.logical, allocator,
                                   backgroundLoad ? vkcore.transferPool
                                                  : *pools.graphics,
                                   vertices, indices, std::move(submeshes));

    if (!res) {
      return std::unexpected(res.error());
    }

    if (backgroundLoad)
      ongoingCommandBuffers.push_back(std::move(res.value().second));
    else {
      auto waitRes = vkcore.device.logical.waitForFences(
          *res.value().second.fence, VK_TRUE, UINT64_MAX);
      if (waitRes != vk::Result::eSuccess) {
        return std::unexpected("Failed to wait for mesh upload fence");
      }
    }

    auto handle = loadedMeshes.emplace(std::move(res.value().first));

    core::rendering::Mesh::Handle meshHandle(handle, loadedMeshes);

    return meshHandle;
  }

  std::expected<Renderer::MaterialHandle, std::string> Renderer::createMaterial(
      Material::Stage stage,
      GraphicsPipelineConfig&& config) { // NOLINT: Allow passing temporaries
    return createMaterial(stage, static_cast<GraphicsPipelineConfig&>(config));
  }

  std::expected<Renderer::MaterialHandle, std::string>
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

    Material mat{
        .pipeline = std::move(pipeline),
        .pipelineLayout = std::move(pipelineLayout),
    };
    mat.stage = stage;

    auto handle = loadedMaterials.emplace(std::move(mat));
    return MaterialHandle(handle, loadedMaterials);
  }

  std::expected<Shader, std::string>
  Renderer::createShader(const unsigned char* const code, size_t size) {
    return Shader::create(vkcore.device.logical, code, size);
  }
} // namespace keptech::vkh
