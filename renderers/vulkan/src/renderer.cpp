#include "keptech/vulkan/renderer.hpp"

#include "keptech/core/moveGuard.hpp"
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
#include <vk_mem_alloc_enums.hpp>

namespace kt::vkh {

  void Renderer::Pools::resetAll() {
    std::set<vk::raii::CommandPool*> unique{
        &graphics.get()->pool,
        &compute.get()->pool,
    };
    for (auto& pool : unique) {
      pool->reset();
    }
  }

  void Renderer::initImGui() {
    auto res = setup::setupImGui(*m.window, m.vkcore);
    if (!res.has_value()) {
      VK_CRITICAL("Failed to initialize ImGui: {}", res.error());
      abort();
    }
    m.imGuiObjects = std::move(res.value());
  }

  void Renderer::newFrame() {
    ImGui_ImplVulkan_NewFrame();
    auto& perFrame = m.vkcore.perFrame[m.frameInfo.index];

    auto nextImageRes =
        m.vkcore.swapchain.getNextImage(m.vkcore.device, perFrame.inFlightFence,
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

    m.frameInfo.imageIndex = imageIndex;
    m.frameInfo.perFrame = &perFrame;

    if (swapchainState == vkh::Swapchain::State::Suboptimal) {
      m.frameInfo.suboptimalSwapchain = true;
    }
  }

  std::expected<CmdBufPtr, std::string> Renderer::startFrame() {
    vk::Result res = vk::Result::eTimeout;
    uint64_t waitValue = m.frameInfo.perFrame->timelineValue;

    bool logged = false;
    while (res = m.vkcore.device->waitSemaphores(
               vk::SemaphoreWaitInfo{
                   .semaphoreCount = 1,
                   .pSemaphores = &*m.frameInfo.perFrame->timelineSemaphore,
                   .pValues = &waitValue,
               },
               0),
           res == vk::Result::eTimeout) {
      if (!logged)
        VK_DEBUG("Waiting for timeline semaphore for frame {} (value {})",
                 m.frameInfo.index, waitValue);
      logged = true;

      using namespace std::chrono_literals;
      std::this_thread::sleep_for(100ms);
    }

    if (res != vk::Result::eSuccess) {
      VK_CRITICAL("Failed to wait for command buffer semaphore: {}",
                  vk::to_string(res));
      abort();
    }

    m.submittedCommandBuffers[m.frameInfo.index].clear();
    m.frameInfo.perFrame->pools.resetAll();

    auto& textureUpdates = m.textureDescriptorsToUpdate[m.frameInfo.index];
    if (!textureUpdates.empty()) {
      auto& globalDescSet = m.globalDescriptorSets.sets[m.frameInfo.index];
      std::vector<vk::DescriptorImageInfo> imageInfos;
      imageInfos.reserve(textureUpdates.size());
      std::vector<vk::WriteDescriptorSet> descriptorWrites;
      descriptorWrites.reserve(textureUpdates.size());
      for (auto& update : textureUpdates) {
        auto imageIndex = update->getIndex();

        imageInfos.push_back({
            .sampler = m.imGuiObjects->sampler,
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
               descriptorWrites.size(), m.frameInfo.index);
      m.vkcore.device->updateDescriptorSets(descriptorWrites, {});
      textureUpdates.clear();
    }

    vk::CommandBufferAllocateInfo cmdBufAllocInfo{
        .commandPool = *m.frameInfo.perFrame->pools.graphics.get()->pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };
    VK_MAKE(cmdBuffers,
            m.vkcore.device->allocateCommandBuffers(cmdBufAllocInfo),
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
        .image = m.vkcore.swapchain.nImage(m.frameInfo.imageIndex),
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
  void
  Renderer::writeCameraMatrices(const CmdBufPtr& cmdBuf,
                                const components::Camera::Uniforms& uniforms) {
    vk::raii::CommandBuffer& vkCmdBuf =
        dynamic_cast<CommandBuffer*>(cmdBuf.get())->get();

    size_t alignedSize =
        maths::roundToAlignment(sizeof(components::Camera::Uniforms),
                                m.limits.minUniformBufferOffsetAlignment);

    size_t dstOffset = alignedSize * m.frameInfo.index;

    // Check if can write to camera buffer. ReBAR etc.
    if (m.cameraBuffer.mapping() != nullptr) {
      memcpy(m.cameraBuffer.mapping() + dstOffset, &uniforms,
             sizeof(components::Camera::Uniforms));
      return;
    }

    auto res = AllocatedBuffer::create(
        m.vkcore.allocator, {.usage = vk::BufferUsageFlagBits::eTransferSrc},
        {.flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
                  vma::AllocationCreateFlagBits::eMapped},
        "Camera Staging Buffer");

    VK_ASSERT(res, "Failed to create camera staging buffer: {}", res.error());
    if (!res) {
      VK_ERROR("Failed to create camera staging buffer: {}", res.error());
      return;
    }

    vkCmdBuf.copyBuffer(res.value().buffer, m.cameraBuffer.buffer,
                        vk::BufferCopy{
                            .srcOffset = 0,
                            .dstOffset = dstOffset,
                            .size = sizeof(components::Camera::Uniforms),
                        });
  }

  void Renderer::bindGlobalDescriptorSets(const CmdBufPtr& cmdBuf,
                                          const IPipeline& pipeline,
                                          Bitflag<shaders::ShaderStages>) {
    CommandBuffer& vkCmdBuf = *dynamic_cast<CommandBuffer*>(cmdBuf.get());
    const LoadedPipeline& vkPipeline =
        static_cast<const LoadedPipeline&>(pipeline);

    vkCmdBuf.get().bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, vkPipeline.pipelineLayout, 0,
        {*m.globalDescriptorSets.sets[m.frameInfo.index]}, {});
  }

  void Renderer::renderImGui(const CmdBufPtr& cmdBuf) {
    ImGui::Render();

    vk::RenderingAttachmentInfo aInfo{
        .imageView = m.vkcore.swapchain.nView(m.frameInfo.imageIndex),
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eLoad,
        .storeOp = vk::AttachmentStoreOp::eStore,
    };

    vk::RenderingInfo renderingInfo{
        .renderArea =
            vk::Rect2D{
                .offset = vk::Offset2D{.x = 0, .y = 0},
                .extent = m.vkcore.swapchain.config().extent,
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

  void Renderer::submitCommandBuffers(std::vector<SubmitInfo> cmdBuffers) {
    if (cmdBuffers.empty()) {
      return;
    }

    uint64_t waitValue = m.frameInfo.perFrame->timelineValue++;
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

    auto& perFrame = m.vkcore.perFrame[m.frameInfo.index];

    auto& thisFrameCmdBufs = m.submittedCommandBuffers[m.frameInfo.index];

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
            m.frameInfo.imageIndex == Frame::INVALID_INDEX ? 1u : 2u,
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
      queue = m.vkcore.queues.graphics.queue.get();
      break;
    case CmdBufType::Compute:
      queue = m.vkcore.queues.compute.queue.get();
      break;
    case CmdBufType::Transfer:
      queue = m.vkcore.queues.transfer.queue.get();
      break;
    }

    queue->submit2(submitInfo);

    thisFrameCmdBufs.reserve(thisFrameCmdBufs.size() +
                             submittedCmdBufInfos.size());
    for (auto& info : submittedCmdBufInfos) {
      thisFrameCmdBufs.emplace_back(std::move(info));
    }

    VK_DEBUG("Submitted {} command buffers at {}, waiting for {}",
             submittedCmdBufInfos.size(), m.frameInfo.index, signalValue);
  }

  void Renderer::endFrame(
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
        .image = m.vkcore.swapchain.nImage(m.frameInfo.imageIndex),
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

    auto& sem = m.vkcore.swapchain.nPresentSemaphore(m.frameInfo.imageIndex);

    vk::SemaphoreSubmitInfo waitInfo{
        .semaphore = m.vkcore.perFrame[m.frameInfo.index].timelineSemaphore,
        .value = m.vkcore.perFrame[m.frameInfo.index].timelineValue,
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .deviceIndex = 0,
    };

    vk::SemaphoreSubmitInfo imageAvailableWaitInfo{
        .semaphore =
            m.vkcore.perFrame[m.frameInfo.index].imageAvailableSemaphore,
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

    m.vkcore.device->resetFences(
        *m.vkcore.perFrame[m.frameInfo.index].inFlightFence);

    m.vkcore.queues.graphics->submit2(
        submitInfo, m.vkcore.perFrame[m.frameInfo.index].inFlightFence);

    m.submittedCommandBuffers[m.frameInfo.index].emplace_back(
        SubmittedCommandBufferInfo{
            .buffer = std::move(vkCmdBuf),
        });

    present();
  }

  void Renderer::present() {
    uint32_t imageIndex = m.frameInfo.imageIndex;
    auto& sem = m.vkcore.swapchain.nPresentSemaphore(imageIndex);

    vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*sem,
        .swapchainCount = 1,
        .pSwapchains = &*m.vkcore.swapchain.getSwapchain(),
        .pImageIndices = &imageIndex,
    };

    auto result = m.vkcore.queues.present.queue->presentKHR(presentInfo);

    m.frameInfo.index = m.frameInfo.nextIndex; // Advance to next frame index

    m.frameInfo.nextIndex = (m.frameInfo.nextIndex + 1) % MAX_FRAMES_IN_FLIGHT;

#ifndef NDEBUG
    if (result == vk::Result::eErrorOutOfDateKHR) {
      VK_DEBUG("Swapchain is out of date during present");
    } else if (result == vk::Result::eSuboptimalKHR) {
      VK_DEBUG("Swapchain is suboptimal during present");
    } else if (m.frameInfo.suboptimalSwapchain) {
      VK_DEBUG("Swapchain was suboptimal at image aquire");
    }
#endif

    if (result == vk::Result::eErrorOutOfDateKHR ||
        result == vk::Result::eSuboptimalKHR ||
        m.frameInfo.suboptimalSwapchain) {
      auto res = recreateSwapchain();
      if (!res) {
        VK_CRITICAL("Failed to recreate swapchain: {}", res.error());
        abort();
      }
    }
  }

  Renderer::Renderer(Renderer&& o) noexcept : m(std::move(o.m)) {
    m.frameInfo.perFrame = &m.vkcore.perFrame[m.frameInfo.index];
  }

  Renderer& Renderer::operator=(Renderer&& o) noexcept {
    if (this == &o)
      return *this;

    m = std::move(o.m);
    return *this;
  }

  void Renderer::preExit() { m.vkcore.device.logical.waitIdle(); }

  void Renderer::shutdownImGui() {
    ImGui_ImplVulkan_Shutdown();
    VK_DEBUG("Shut down ImGui Vulkan backend");
  }

  Renderer::~Renderer() {
    if (m.moveGuard.moved()) {
      return;
    }

    m.vkcore.device.logical.waitIdle();

    m.cameraBuffer.destroy(m.vkcore.allocator);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      m.submittedCommandBuffers[i].clear();
      m.textureDescriptorsToUpdate[i].clear();
    }

    m.vkcore.allocator.destroy();

    m.vkcore.device.logical.waitIdle();
    VK_INFO("Vulkan renderer shut down cleanly");
  }

  std::expected<void, std::string> Renderer::recreateSwapchain() {
    VK_TRACE("Recreating swapchain");
    VKH_MAKE(newSwapchain,
             setup::createSwapchain(m.vkcore.device.physical,
                                    m.window->getRenderSize(),
                                    m.vkcore.device.logical, m.vkcore.surface,
                                    m.vkcore.queues, &*m.vkcore.swapchain),
             "Failed to recreate swapchain");

    {
      [[maybe_unused]]
      auto oldSwapchain = std::move(m.vkcore.swapchain);

      m.vkcore.swapchain = std::move(newSwapchain);

      m.vkcore.queues.graphics->waitIdle();
    }

    m.frameInfo.suboptimalSwapchain = false;

    return {};
  }

} // namespace kt::vkh
