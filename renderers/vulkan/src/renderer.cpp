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
    std::set<vk::raii::CommandPool*> unique{graphics.get(), present.get(),
                                            compute.get(), transfer.get()};
    for (auto& pool : unique) {
      pool->reset();
    }
  }

  void Renderer::newFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();

    ImGui::NewFrame();
  }

  Renderer::Frame Renderer::startFrame() {
    checkSwapchain();

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

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
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
} // namespace keptech::vkh
