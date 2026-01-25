#include "keptech/vulkan/renderer.hpp"
#include "keptech/core/moveGuard.hpp"
#include "keptech/vulkan/commandBuffer.hpp"
#include "keptech/vulkan/helpers/swapchain.hpp"
#include "macros.hpp"
#include "vulkan/vulkan.hpp"

#include "setup/imgui.hpp"
#include "vk-logger.hpp"
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/core/components/camera.hpp>
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

  void RendererBackend::initImGui() {
    auto res = setup::setupImGui(*window, vkcore);
    if (!res.has_value()) {
      VK_CRITICAL("Failed to initialize ImGui: {}", res.error());
      abort();
    }
    imGuiObjects = std::move(res.value());
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
    submittedCommandBuffers[frameInfo.index].clear();
  }

  std::expected<CmdBufPtr, std::string>
  RendererBackend::createCmdBuffer(CmdBufType t) {
    vk::raii::CommandPool* pool = nullptr;
    switch (t) {
    case CmdBufType::Graphics:
      pool = &frameInfo.perFrame->pools.graphics.get()->pool;
      break;
    case CmdBufType::Compute:
      pool = &frameInfo.perFrame->pools.compute.get()->pool;
      break;
    case CmdBufType::Transfer:
      pool = &vkcore.transferPool.pool;
      break;
    }

    vk::CommandBufferAllocateInfo cmdBufAllocInfo{
        .commandPool = *pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };

    VK_MAKE(cmdBuffers, vkcore.device->allocateCommandBuffers(cmdBufAllocInfo),
            "Failed to allocate graphics command buffer");

    vk::raii::CommandBuffer cmdBuffer = std::move(cmdBuffers_res.value.front());

    return std::make_unique<CommandBuffer>(std::move(cmdBuffer), t);
  }

  void RendererBackend::startFrame(const CmdBufPtr& cmdBuf) {
    auto& graphicsCmdBuffer = dynamic_cast<CommandBuffer*>(cmdBuf.get())->get();

    vk::ImageMemoryBarrier2 barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
        .srcAccessMask = vk::AccessFlagBits2::eNone,
        .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
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

    graphicsCmdBuffer.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    });
  }

  void RendererBackend::renderImGui(const CmdBufPtr& cmdBuf) {
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

  void
  RendererBackend::submitCommandBuffers(std::vector<CmdBufPtr> cmdBuffers) {
    if (cmdBuffers.empty()) {
      return;
    }

    std::vector<vk::raii::CommandBuffer> vkCmdBufs;
    vkCmdBufs.reserve(cmdBuffers.size());
    for (auto& cmdBuf : cmdBuffers) {
      auto& vkCmdBuf = dynamic_cast<CommandBuffer*>(cmdBuf.get())->get();
      vkCmdBufs.emplace_back(std::move(vkCmdBuf));
    }

    auto& perFrame = vkcore.perFrame[frameInfo.index];

    auto& thisFrameCmdBufs = submittedCommandBuffers[frameInfo.index];

    vk::SemaphoreSubmitInfo waitInfo{
        .semaphore = perFrame.imageAvailableSemaphore,
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .deviceIndex = 0,
    };

    vk::SemaphoreSubmitInfo signalInfo{
        .semaphore = perFrame.timelineSemaphore,
        .value = thisFrameCmdBufs.size() + vkCmdBufs.size(),
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .deviceIndex = 0,
    };

    std::vector<vk::CommandBufferSubmitInfo> cmdBufInfos;
    cmdBufInfos.reserve(vkCmdBufs.size());
    for (auto& vkCmdBuf : vkCmdBufs) {
      cmdBufInfos.emplace_back(vk::CommandBufferSubmitInfo{
          .commandBuffer = *vkCmdBuf,
          .deviceMask = 0,
      });
    }

    vk::SubmitInfo2 submitInfo{
        .waitSemaphoreInfoCount =
            frameInfo.imageIndex == Frame::INVALID_INDEX ? 0u : 1u,
        .pWaitSemaphoreInfos = &waitInfo,
        .commandBufferInfoCount = static_cast<uint32_t>(cmdBufInfos.size()),
        .pCommandBufferInfos = cmdBufInfos.data(),
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalInfo,
    };

    vk::raii::Queue* queue = nullptr;

    // FIXME: Assumes all command buffers are of the same type
    switch (cmdBuffers.front()->getType()) {
    case CmdBufType::Graphics:
      queue = vkcore.queues.graphics.queue.get();
      break;
    case CmdBufType::Compute:
      queue = vkcore.queues.compute.queue.get();
      break;
    case CmdBufType::Transfer:
      queue = vkcore.queues.transfer.queue.get();
      break;
    }

    queue->submit2(submitInfo);

    thisFrameCmdBufs.reserve(thisFrameCmdBufs.size() + vkCmdBufs.size());
    for (auto& vkCmdBuf : vkCmdBufs) {
      thisFrameCmdBufs.emplace_back(std::move(vkCmdBuf));
    }

    VK_DEBUG("Submitted {} command buffers at {}, waiting for {}",
             vkCmdBufs.size(), frameInfo.index, thisFrameCmdBufs.size());
  }

  void RendererBackend::endFrame(
      CmdBufPtr&& cmdBuf) { // NOLINT - The item in the UPtr is moved
    vk::raii::CommandBuffer vkCmdBuf =
        std::move(dynamic_cast<CommandBuffer*>(cmdBuf.get())->get());

    vk::ImageMemoryBarrier2 barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
        .dstAccessMask = vk::AccessFlagBits2::eNone,
        .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .newLayout = vk::ImageLayout::ePresentSrcKHR,
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

    vkCmdBuf.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    });

    auto& sem = vkcore.swapchain.nPresentSemaphore(frameInfo.imageIndex);

    VK_DEBUG("Present {} waiting for {}", frameInfo.index,
             submittedCommandBuffers[frameInfo.index].size());

    vk::SemaphoreSubmitInfo waitInfo{
        .semaphore = vkcore.perFrame[frameInfo.index].timelineSemaphore,
        .value = submittedCommandBuffers[frameInfo.index].size(),
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .deviceIndex = 0,
    };

    vk::SemaphoreSubmitInfo signalInfo{
        .semaphore = sem,
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .deviceIndex = 0,
    };

    vk::CommandBufferSubmitInfo cmdBufInfo{
        .commandBuffer = vkCmdBuf,
        .deviceMask = 0,
    };

    vk::SubmitInfo2 submitInfo{
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &waitInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdBufInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalInfo,
    };

    vkcore.device->resetFences(*vkcore.perFrame[frameInfo.index].inFlightFence);

    vkcore.queues.graphics->submit2(
        submitInfo, vkcore.perFrame[frameInfo.index].inFlightFence);

    submittedCommandBuffers[frameInfo.index].emplace_back(std::move(vkCmdBuf));

    present();
  }

  void RendererBackend::present() {
    uint32_t imageIndex = frameInfo.imageIndex;
    auto& sem = vkcore.swapchain.nPresentSemaphore(imageIndex);

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

#ifndef NDEBUG
    if (result == vk::Result::eErrorOutOfDateKHR) {
      VK_WARN("Swapchain is out of date during present");
    } else if (result == vk::Result::eSuboptimalKHR) {
      VK_WARN("Swapchain is suboptimal during present");
    } else if (frameInfo.suboptimalSwapchain) {
      VK_WARN("Swapchain was suboptimal at image aquire");
    }
#endif

    if (result == vk::Result::eErrorOutOfDateKHR ||
        result == vk::Result::eSuboptimalKHR || frameInfo.suboptimalSwapchain) {
      auto res = recreateSwapchain();
      if (!res) {
        VK_CRITICAL("Failed to recreate swapchain: {}", res.error());
        abort();
      }
    }
  }

  RendererBackend::RendererBackend(RendererBackend&& o) noexcept
      : moveGuard(std::move(o.moveGuard)), vkcore{std::move(o.vkcore)},
        window{o.window},
        globalDescriptorSets{std::move(o.globalDescriptorSets)},
        imGuiObjects{std::move(o.imGuiObjects)}, frameInfo{o.frameInfo} {
    frameInfo.perFrame = &vkcore.perFrame[frameInfo.index];
  }

  RendererBackend& RendererBackend::operator=(RendererBackend&& o) noexcept {
    if (this == &o)
      return *this;

    moveGuard = std::move(o.moveGuard);
    vkcore = std::move(o.vkcore);
    window = o.window;
    globalDescriptorSets = std::move(o.globalDescriptorSets);
    imGuiObjects = std::move(o.imGuiObjects);
    frameInfo = o.frameInfo;
    frameInfo.perFrame = &vkcore.perFrame[frameInfo.index];

    return *this;
  }

  RendererBackend::~RendererBackend() {
    if (moveGuard.moved()) {
      return;
    }

    vkcore.device.logical.waitIdle();

    vkcore.device.logical.waitIdle();

    ImGui_ImplVulkan_Shutdown();

    vkcore.allocator.destroy();
    vkcore.device.logical.waitIdle();

    VK_INFO("Vulkan renderer shut down cleanly");
  }

  std::expected<void, std::string> RendererBackend::recreateSwapchain() {
    VK_TRACE("Recreating swapchain");
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

    frameInfo.suboptimalSwapchain = false;

    return {};
  }

} // namespace keptech::vkh
