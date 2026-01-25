#include "keptech/vulkan/renderer.hpp"
#include "keptech/vulkan/commandBuffer.hpp"
#include "keptech/vulkan/helpers/swapchain.hpp"
#include "macros.hpp"
#include "vulkan/vulkan.hpp"

#include "vk-logger.hpp"
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/core/components/camera.hpp>
#include <keptech/core/rendering/gltf/loaded.hpp>
#include <keptech/core/window.hpp>
#include <set>

namespace keptech::vkh {
  void RendererBackend::Pools::resetAll() {
    std::set<vk::raii::CommandPool*> unique{
        &graphics.get()->pool,
        &compute.get()->pool,
    };
    for (auto& pool : unique) {
      pool->reset();
    }
  }

  void RendererBackend::newFrame() {
    ImGui_ImplVulkan_NewFrame();
    auto& perFrame = vkcore.perFrame[frameInfo.index];

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
      newFrame();
    }

    frameInfo.imageIndex = imageIndex;
    frameInfo.perFrame = &perFrame;

    if (swapchainState == vkh::Swapchain::State::Suboptimal) {
      frameInfo.suboptimalSwapchain = true;
    }

    frameInfo.perFrame->pools.resetAll();
  }

  std::expected<UCmdBufPtr, std::string>
  RendererBackend::createGraphicsCmdBuffer() {
    vk::CommandBufferAllocateInfo cmdBufAllocInfo{
        .commandPool = *frameInfo.perFrame->pools.graphics.get()->pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };

    VK_MAKE(graphicsCmdBuffers,
            vkcore.device->allocateCommandBuffers(cmdBufAllocInfo),
            "Failed to allocate graphics command buffer");

    vk::raii::CommandBuffer graphicsCmdBuffer =
        std::move(graphicsCmdBuffers_res.value.front());

    std::unique_ptr<CommandBuffer> buffer =
        std::make_unique<CommandBuffer>(std::move(graphicsCmdBuffer));

    return std::move(buffer);
  }

  void RendererBackend::renderImGui(const UCmdBufPtr& cmdBuf) {
    ImGui::Render();

    vk::RenderingAttachmentInfo aInfo{
        .imageView = vkcore.swapchain.nView(frameInfo.imageIndex),
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eStore,
    };

    vk::RenderingInfo renderingInfo{
        .renderArea =
            vk::Rect2D{
                .offset = vk::Offset2D{.x = 0, .y = 0},
                .extent = vkcore.swapchain.config().extent,
            },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &aInfo,
    };

    auto& graphicsCmdBuffer = dynamic_cast<CommandBuffer*>(cmdBuf.get())->get();

    graphicsCmdBuffer.beginRendering(renderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *graphicsCmdBuffer);
    graphicsCmdBuffer.endRendering();
  }

  void RendererBackend::beginDeferredPass() {
    vk::ImageMemoryBarrier2 toDrawableBarrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
        .srcAccessMask = vk::AccessFlagBits2::eNone,
        .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentRead |
                         vk::AccessFlagBits2::eColorAttachmentWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vkcore.swapchain.nImage(frameInfo.imageIndex),
        .subresourceRange =
            vk::ImageSubresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vk::ImageMemoryBarrier2 gBufferColorToDrawableBarrier = toDrawableBarrier;
    gBufferColorToDrawableBarrier.image = gBuffer.color.image;
    vk::ImageMemoryBarrier2 gBufferNormalToDrawableBarrier = toDrawableBarrier;
    gBufferNormalToDrawableBarrier.image = gBuffer.normal.image;

    vk::ImageMemoryBarrier2 gBufferDepthToDrawableBarrier = toDrawableBarrier;
    gBufferDepthToDrawableBarrier.image = gBuffer.depth.image;
    gBufferDepthToDrawableBarrier.newLayout =
        vk::ImageLayout::eDepthStencilAttachmentOptimal;
    gBufferDepthToDrawableBarrier.dstStageMask =
        vk::PipelineStageFlagBits2::eEarlyFragmentTests |
        vk::PipelineStageFlagBits2::eLateFragmentTests;
    gBufferDepthToDrawableBarrier.dstAccessMask =
        vk::AccessFlagBits2::eDepthStencilAttachmentRead |
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
    gBufferDepthToDrawableBarrier.subresourceRange.aspectMask =
        vk::ImageAspectFlagBits::eDepth;

    std::array<vk::ImageMemoryBarrier2, 4> barriers{
        toDrawableBarrier,
        gBufferColorToDrawableBarrier,
        gBufferNormalToDrawableBarrier,
        gBufferDepthToDrawableBarrier,
    };

    graphicsCmdBuffer.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = barriers.size(),
        .pImageMemoryBarriers = barriers.data(),
    });
  }

  void RendererBackend::setupGraphicsCommandBuffer(
      const vk::raii::CommandBuffer& graphicsCmdBuffer,
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

  void RendererBackend::endFrame() {
    auto& sem = vkcore.swapchain.nPresentSemaphore(frameInfo.imageIndex);

    uint32_t imageIndex = frameInfo.imageIndex;

    vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*sem,
        .swapchainCount = 1,
        .pSwapchains = &*vkcore.swapchain.getSwapchain(),
        .pImageIndices = &imageIndex,
    };

    auto result = vkcore.queues.present.queue->presentKHR(presentInfo);

    frameInfo.index = frameInfo.nextIndex; // Advance to next frame index

    frameInfo.nextIndex = (frameInfo.nextIndex + 1) % MAX_FRAMES_IN_FLIGHT;

    if (result == vk::Result::eErrorOutOfDateKHR ||
        result == vk::Result::eSuboptimalKHR || frameInfo.suboptimalSwapchain) {
      auto res = recreateSwapchain();
      if (!res) {
        VK_CRITICAL("Failed to recreate swapchain: {}", res.error());
        abort();
      }
    }
  }

  RendererBackend::~RendererBackend() {
    if (moveGuard.moved()) {
      return;
    }

    vkcore.device.logical.waitIdle();

    vkcore.device.logical.waitIdle();

    for (auto& frame : vkcore.perFrame)
      frame.instanceBuffers.destroy(vkcore.allocator);

    ImGui_ImplVulkan_Shutdown();

    vkcore.allocator.destroy();
    vkcore.device.logical.waitIdle();

    VK_INFO("Vulkan renderer shut down cleanly");
  }

  std::expected<void, std::string> RendererBackend::recreateSwapchain() {
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
