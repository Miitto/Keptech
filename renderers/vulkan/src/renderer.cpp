#include "keptech/vulkan/renderer.hpp"
#include "keptech/vulkan/helpers/swapchain.hpp"
#include "macros.hpp"
#include "vulkan/vulkan.hpp"

#include "vk-logger.hpp"
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/core/components/camera.hpp>
#include <keptech/core/renderer.hpp>
#include <keptech/core/rendering/gltf/loaded.hpp>
#include <keptech/core/window.hpp>
#include <set>

namespace keptech::vkh {
  static_assert(core::renderer::CRenderer<Renderer>,
                "Renderer must satisfy CRenderer concept");

  void Renderer::Pools::resetAll() {
    std::set<vk::raii::CommandPool*> unique{
        &graphics.get()->pool,
        &compute.get()->pool,
    };
    for (auto& pool : unique) {
      pool->reset();
    }
  }

  Renderer::ObjectLists
  Renderer::buildRenderObjectLists(core::Scene& scene,
                                   const maths::Frustum& frustum) {
    ObjectLists lists;

    auto view = scene.getEcs()
                    .view<components::Transform, components::Mesh,
                          components::Material>();

    for (auto [entity, transform, meshHandle, materialHandle] : view.each()) {

      auto materialP = loadedMaterials.get(materialHandle);
      if (!materialP) {
        VK_WARN("RenderObject has invalid material handle, skipping");
        continue;
      }

      auto meshP = loadedMeshes.get(meshHandle);
      if (!meshP) {
        VK_WARN("RenderObject has invalid mesh handle, skipping");
        continue;
      }

      auto& mesh = *meshP;
      auto& material = *materialP;

      transform.recalculateGlobalTransform();

      // TODO: Frustum cull

      struct VkRenderObject ro{
          .transform = transform.getGlobal(),
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
    checkCompletedCommandBuffers();

    auto& perFrame = vkcore.perFrame[thisFrameIndex];

    auto nextImageRes =
        vkcore.swapchain.getNextImage(vkcore.device, perFrame.inFlightFence,
                                      perFrame.imageAvailableSemaphore);

    if (!nextImageRes) {
      VK_CRITICAL("Failed to acquire next swapchain image: {}",
                  nextImageRes.error());
      abort();
    }
    auto [imageIndex, swapchainState] = nextImageRes.value();

    if (swapchainState == vkh::Swapchain::State::OutOfDate) {
      auto res = recreateSwapchain();
      if (!res) {
        VK_CRITICAL("Failed to recreate swapchain: {}", res.error());
        abort();
      }
      VK_DEBUG("Restarting frame after swapchain recreation");
      // Try again
      return startFrame();
    }

    Frame frameInfo{
        .index = thisFrameIndex,
        .imageIndex = static_cast<uint8_t>(imageIndex),
        .perFrame = std::ref(perFrame),
    };

    if (swapchainState == vkh::Swapchain::State::Suboptimal) {
      frameInfo.suboptimalSwapchain = true;
    }

    frameInfo.perFrame.get().pools.resetAll();
    submittedCommandBuffers[thisFrameIndex].clear();

    return frameInfo;
  }

  void Renderer::setupGraphicsCommandBuffer(
      const Frame& info, const vk::raii::CommandBuffer& graphicsCmdBuffer,
      const components::Camera& camera) {

    auto& viewport = camera.getViewport();
    auto& scissor = camera.getScissor();

    VK_TRACE("Setting viewport to x:{} y:{} w:{} h:{}", viewport.x, viewport.y,
             viewport.width, viewport.height);
    VK_TRACE("Setting scissor to x:{} y:{} w:{} h:{}", scissor.x, scissor.y,
             scissor.width, scissor.height);

    graphicsCmdBuffer.setViewport(0, vk::Viewport{
                                         .x = viewport.x,
                                         .y = viewport.y,
                                         .width = viewport.width,
                                         .height = viewport.height,
                                         .minDepth = viewport.minDepth,
                                         .maxDepth = viewport.maxDepth,
                                     });

    graphicsCmdBuffer.setScissor(0, vk::Rect2D{
                                        .offset =
                                            {
                                                .x = scissor.x,
                                                .y = scissor.y,
                                            },
                                        .extent =
                                            vk::Extent2D{
                                                .width = scissor.width,
                                                .height = scissor.height,
                                            },
                                    });
  }

  void Renderer::presentFrame(const Frame& info) {
    uint32_t imageIndex = info.imageIndex;

    auto& sem = vkcore.swapchain.nPresentSemaphore(imageIndex);

    vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*sem,
        .swapchainCount = 1,
        .pSwapchains = &*vkcore.swapchain.getSwapchain(),
        .pImageIndices = &imageIndex,
    };

    auto result = vkcore.queues.present.queue->presentKHR(presentInfo);
    this->thisFrameIndex = (this->thisFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    if (result == vk::Result::eErrorOutOfDateKHR ||
        result == vk::Result::eSuboptimalKHR || info.suboptimalSwapchain) {
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

    for (auto& ongoing : ongoingCommandBuffers) {
      ongoing.buffer.destroy(vkcore.allocator);
    }

    vkcore.device.logical.waitIdle();
    ongoingCommandBuffers.clear();

    vkcore.gBuffer.color.destroy(vkcore.allocator, vkcore.device.logical);
    vkcore.gBuffer.normal.destroy(vkcore.allocator, vkcore.device.logical);
    vkcore.gBuffer.depth.destroy(vkcore.allocator, vkcore.device.logical);

    for (auto& frame : vkcore.perFrame)
      frame.instanceBuffers.destroy(vkcore.allocator);

    loadedMeshes.reset();
    loadedMaterials.reset();

    cameraObjects.descriptorSet.release(); // The pool destructor will free this
    cameraObjects.uniformBuffer.destroy(vkcore.allocator);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    vkcore.allocator.destroy();
    vkcore.device.logical.waitIdle();

    VK_INFO("Vulkan renderer shut down cleanly");
  }

  std::expected<void, std::string> Renderer::recreateSwapchain() {
    VK_DEBUG("Recreating swapchain");
    VKH_MAKE(newSwapchain,
             setup::createSwapchain(vkcore.device.physical,
                                    window->getRenderSize(),
                                    vkcore.device.logical, vkcore.surface,
                                    vkcore.queues, &*vkcore.swapchain),
             "Failed to recreate swapchain");

    {
      [[maybe_unused]]
      auto oldSwapchain = std::move(vkcore.swapchain);

      vkcore.swapchain = std::move(newSwapchain);

      vkcore.queues.graphics->waitIdle();
    }

    VK_INFO("Swapchain recreated");
    return {};
  }
} // namespace keptech::vkh
