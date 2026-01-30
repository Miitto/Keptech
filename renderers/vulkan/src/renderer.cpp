#include "keptech/vulkan/renderer.hpp"
#include "keptech/core/moveGuard.hpp"
#include "keptech/vulkan/buffer.hpp"
#include "keptech/vulkan/commandBuffer.hpp"
#include "keptech/vulkan/helpers/swapchain.hpp"
#include "macros.hpp"
#include "vulkan/vulkan.hpp"
#include <keptech/core/maths/maths.hpp>

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

  std::expected<CmdBufPtr, std::string> RendererBackend::startFrame() {
    vk::Result res = vk::Result::eTimeout;
    uint64_t waitValue = frameInfo.perFrame->timelineValue;

    while (res = vkcore.device->waitSemaphores(
               vk::SemaphoreWaitInfo{
                   .semaphoreCount = 1,
                   .pSemaphores = &*frameInfo.perFrame->timelineSemaphore,
                   .pValues = &waitValue,
               },
               UINT64_MAX),
           res == vk::Result::eTimeout) {
    }

    if (res != vk::Result::eSuccess) {
      VK_CRITICAL("Failed to wait for command buffer semaphore: {}",
                  vk::to_string(res));
      abort();
    }

    submittedCommandBuffers[frameInfo.index].clear();
    frameInfo.perFrame->pools.resetAll();

    auto& textureUpdates = textureDescriptorsToUpdate[frameInfo.index];
    if (!textureUpdates.empty()) {
      auto& globalDescSet = globalDescriptorSets.sets[frameInfo.index];
      std::vector<vk::DescriptorImageInfo> imageInfos;
      imageInfos.reserve(textureUpdates.size());
      std::vector<vk::WriteDescriptorSet> descriptorWrites;
      descriptorWrites.reserve(textureUpdates.size());
      for (auto& update : textureUpdates) {
        auto imageIndex = update->getIndex();

        imageInfos.push_back({
            .sampler = imGuiObjects->sampler,
            .imageView = update->getImage().view,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        });

        descriptorWrites.push_back({
            .dstSet = *globalDescSet,
            .dstBinding = 1,
            .dstArrayElement = imageIndex,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &imageInfos.back(),
        });
      }

      VK_DEBUG("Updating {} texture descriptors for frame {}",
               descriptorWrites.size(), frameInfo.index);
      vkcore.device->updateDescriptorSets(descriptorWrites, {});
      textureUpdates.clear();
    }

    vk::CommandBufferAllocateInfo cmdBufAllocInfo{
        .commandPool = *frameInfo.perFrame->pools.graphics.get()->pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };
    VK_MAKE(cmdBuffers, vkcore.device->allocateCommandBuffers(cmdBufAllocInfo),
            "Failed to allocate graphics command buffer");
    auto cmdBuffer = std::move(cmdBuffers_res.value.front());

    cmdBuffer.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });

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

    cmdBuffer.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    });

    return std::make_unique<CommandBuffer>(std::move(cmdBuffer),
                                           CmdBufType::Graphics);
  }
  void RendererBackend::writeCameraMatrices(const CmdBufPtr& cmdBuf,
                                            const BufPtr& stagingBuffer) {
    vk::raii::CommandBuffer& vkCmdBuf =
        dynamic_cast<CommandBuffer*>(cmdBuf.get())->get();
    Buffer& vkStagingBuffer = *dynamic_cast<Buffer*>(stagingBuffer.get());

    uint64_t offset = 0;
    uint64_t size = sizeof(components::Camera::Uniforms);

    for (size_t i = 0; i < frameInfo.index; ++i) {
      offset += size;
      offset = maths::roundToAlignment(offset, 256);
    }

    vkCmdBuf.copyBuffer(vkStagingBuffer.getBuffer().buffer, cameraBuffer.buffer,
                        vk::BufferCopy{
                            .srcOffset = 0,
                            .dstOffset = offset,
                            .size = size,
                        });
  }

  void
  RendererBackend::bindGlobalDescriptorSets(const CmdBufPtr& cmdBuf,
                                            const IPipeline& pipeline,
                                            Bitflag<shaders::ShaderStages>) {
    CommandBuffer& vkCmdBuf = *dynamic_cast<CommandBuffer*>(cmdBuf.get());
    const LoadedPipeline& vkPipeline =
        static_cast<const LoadedPipeline&>(pipeline);

    vkCmdBuf.get().bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, vkPipeline.pipelineLayout, 0,
        {*globalDescriptorSets.sets[frameInfo.index]}, {});
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
  RendererBackend::submitCommandBuffers(std::vector<SubmitInfo> cmdBuffers) {
    if (cmdBuffers.empty()) {
      return;
    }

    uint64_t waitValue = frameInfo.perFrame->timelineValue++;
    uint64_t signalValue = waitValue + 1;

    std::vector<SubmittedCommandBufferInfo> submittedCmdBufInfos;
    submittedCmdBufInfos.reserve(cmdBuffers.size());
    for (auto& cmdBuf : cmdBuffers) {
      auto& vkCmdBuf =
          dynamic_cast<CommandBuffer*>(cmdBuf.commandBuffer.get())->get();
      submittedCmdBufInfos.emplace_back(SubmittedCommandBufferInfo{
          .buffer = std::move(vkCmdBuf),
          .trackedBuffers = std::move(cmdBuf.trackedBuffers),
      });
    }

    auto& perFrame = vkcore.perFrame[frameInfo.index];

    auto& thisFrameCmdBufs = submittedCommandBuffers[frameInfo.index];

    vk::SemaphoreSubmitInfo nextFrameWaitInfo{
        .semaphore = perFrame.imageAvailableSemaphore,
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .deviceIndex = 0,
    };

    vk::SemaphoreSubmitInfo prevSumbitWaitInfo{
        .semaphore = perFrame.timelineSemaphore,
        .value = waitValue,
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .deviceIndex = 0,
    };

    std::array<vk::SemaphoreSubmitInfo, 2> waitInfos{
        prevSumbitWaitInfo,
        nextFrameWaitInfo,
    };

    vk::SemaphoreSubmitInfo signalInfo{
        .semaphore = perFrame.timelineSemaphore,
        .value = signalValue,
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .deviceIndex = 0,
    };

    std::vector<vk::CommandBufferSubmitInfo> cmdBufInfos;
    cmdBufInfos.reserve(submittedCmdBufInfos.size());
    for (auto& info : submittedCmdBufInfos) {
      cmdBufInfos.emplace_back(vk::CommandBufferSubmitInfo{
          .commandBuffer = *info.buffer,
          .deviceMask = 0,
      });
    }

    vk::SubmitInfo2 submitInfo{
        .waitSemaphoreInfoCount =
            frameInfo.imageIndex == Frame::INVALID_INDEX ? 1u : 2u,
        .pWaitSemaphoreInfos = waitInfos.data(),
        .commandBufferInfoCount = static_cast<uint32_t>(cmdBufInfos.size()),
        .pCommandBufferInfos = cmdBufInfos.data(),
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalInfo,
    };

    vk::raii::Queue* queue = nullptr;

    // FIXME: Assumes all command buffers are of the same type
    switch (cmdBuffers.front().commandBuffer->getType()) {
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

    thisFrameCmdBufs.reserve(thisFrameCmdBufs.size() +
                             submittedCmdBufInfos.size());
    for (auto& info : submittedCmdBufInfos) {
      thisFrameCmdBufs.emplace_back(std::move(info));
    }

    VK_DEBUG("Submitted {} command buffers at {}, waiting for {}",
             submittedCmdBufInfos.size(), frameInfo.index, signalValue);
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

    vkCmdBuf.end();

    auto& sem = vkcore.swapchain.nPresentSemaphore(frameInfo.imageIndex);

    vk::SemaphoreSubmitInfo waitInfo{
        .semaphore = vkcore.perFrame[frameInfo.index].timelineSemaphore,
        .value = vkcore.perFrame[frameInfo.index].timelineValue,
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .deviceIndex = 0,
    };

    vk::SemaphoreSubmitInfo imageAvailableWaitInfo{
        .semaphore = vkcore.perFrame[frameInfo.index].imageAvailableSemaphore,
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
    };

    std::array<vk::SemaphoreSubmitInfo, 2> waitSemaphores{
        waitInfo, imageAvailableWaitInfo};

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
        .waitSemaphoreInfoCount = 2,
        .pWaitSemaphoreInfos = waitSemaphores.data(),
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdBufInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalInfo,
    };

    vkcore.device->resetFences(*vkcore.perFrame[frameInfo.index].inFlightFence);

    vkcore.queues.graphics->submit2(
        submitInfo, vkcore.perFrame[frameInfo.index].inFlightFence);

    submittedCommandBuffers[frameInfo.index].emplace_back(
        SubmittedCommandBufferInfo{
            .buffer = std::move(vkCmdBuf),
        });

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
        window{o.window}, cameraBuffer{o.cameraBuffer},
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
    cameraBuffer = o.cameraBuffer;
    globalDescriptorSets = std::move(o.globalDescriptorSets);
    imGuiObjects = std::move(o.imGuiObjects);
    frameInfo = o.frameInfo;
    frameInfo.perFrame = &vkcore.perFrame[frameInfo.index];

    return *this;
  }

  void RendererBackend::preExit() { vkcore.device.logical.waitIdle(); }

  RendererBackend::~RendererBackend() {
    if (moveGuard.moved()) {
      return;
    }

    vkcore.device.logical.waitIdle();

    cameraBuffer.destroy(vkcore.allocator);

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
