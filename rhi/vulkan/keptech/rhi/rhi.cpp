#include "rhi.hpp"

#include "keptech/rhi/cmdBuf.hpp"
#include "keptech/rhi/imgui.hpp"
#include "profile.hpp"
#include "setup/setup.hpp"
#include "vk/macros.hpp"
#include "vk/swapchain.hpp"
#include "vk/vk-logger.hpp"
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/components/camera.hpp>
#include <keptech/core/profile.hpp>
#include <keptech/core/window.hpp>
#include <keptech/maths/maths.hpp>

namespace kt::rhi {
  RHI RHI::singleton{};
  bool RHI::isInitialized = false;

  void RHI::newFrame() {
    KT_PROFILE_FUNCTION
    VK_TRACE("Starting frame {}", m.frameIndex);
    ImGui_ImplVulkan_NewFrame();
    imgui::newFrame();
    auto& perFrame = m.vkcore.perFrame[m.frameIndex];

    auto nextImageRes = m.vkcore.swapchain.getNextImage(m.vkcore.device, perFrame.inFlightFence, perFrame.imageAvailableSemaphore);

    if (!nextImageRes) {
      VK_CRITICAL("Failed to acquire next swapchain image: {}", nextImageRes.error());
      abort();
    }
    auto [imageIndex, swapchainState] = nextImageRes.value();

    if (swapchainState == rhi::Swapchain::State::OutOfDate) {
      auto res = recreateSwapchain();
      if (!res) {
        VK_CRITICAL("Failed to recreate swapchain: {}", res.error());
        abort();
      }
      VK_DEBUG("Restarting frame after swapchain recreation");
      // Try again
      newFrame();
    }

    m.imageIndex = static_cast<uint8_t>(imageIndex);

    if (swapchainState == rhi::Swapchain::State::Suboptimal) {
      m.swapchainSuboptimal = true;
    }
  }

  void RHI::startFrame() {
    KT_PROFILE_FUNCTION

    auto& pools = m.vkcore.perFrame[m.frameIndex].pools;
    vkResetCommandPool(m.vkcore.device, pools.graphics, 0);
    vkResetCommandPool(m.vkcore.device, pools.compute, 0);

    updateTextureDescriptors();
    updateBufferPointers();
  }

  void RHI::renderImGui(VkCommandBuffer cmdBuf) {
    KT_PROFILE_FUNCTION
    KT_VK_ZONE(m.tracyGraphicsContext, cmdBuf, "Render ImGui");
    ImGui::Render();

    VkRenderingAttachmentInfo aInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = m.vkcore.swapchain.nView(m.imageIndex),
        .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    VkRenderingInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea =
            VkRect2D{
                .offset = VkOffset2D{.x = 0, .y = 0},
                .extent = m.vkcore.swapchain.config().extent,
            },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &aInfo,
    };

    vkCmdBeginRendering(cmdBuf, &renderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuf);
    vkCmdEndRendering(cmdBuf);
  }

  void RHI::endFrame(CommandBuffer& cmdBuf) {
    KT_PROFILE_FUNCTION

    VK_TRACE("RHI::endFrame. Waiting for {}.", m.vkcore.mainSemaphore.value);

#ifndef NDEBUG
    debugUi();
#endif

    renderImGui(cmdBuf);

    VkImageMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        .dstAccessMask = VK_ACCESS_2_NONE,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = m.vkcore.swapchain.nImage(m.imageIndex),
        .subresourceRange =
            VkImageSubresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    VkDependencyInfo depInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(cmdBuf, &depInfo);

    cmdBuf.end();

    auto& sem = m.vkcore.swapchain.nPresentSemaphore(m.imageIndex);

    std::array<VkSemaphoreSubmitInfo, 2> waitInfo{
        VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = m.vkcore.perFrame[m.frameIndex].imageAvailableSemaphore,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        },
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = m.vkcore.mainSemaphore.semaphore,
            .value = m.vkcore.mainSemaphore.value,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        },
    };

    VkSemaphoreSubmitInfo signalInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = sem,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .deviceIndex = 0,
    };

    VkCommandBufferSubmitInfo cmdBufInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmdBuf,
        .deviceMask = 0,
    };

    VkSubmitInfo2 submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = waitInfo.size(),
        .pWaitSemaphoreInfos = waitInfo.data(),
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdBufInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalInfo,
    };

    VK_TRACE("Resetting fence for frame {}.", m.frameInfo.index);
    vkResetFences(m.vkcore.device, 1, &m.vkcore.perFrame[m.frameIndex].inFlightFence);

    auto res = vkQueueSubmit2(m.vkcore.queues.graphics.queue, 1, &submitInfo, m.vkcore.perFrame[m.frameIndex].inFlightFence);
    VK_ASSERT(res == VK_SUCCESS, "Failed to submit command buffer: {}", res);

    present();
  }

  void RHI::present() {
    KT_PROFILE_FUNCTION
    uint32_t imageIndex = m.imageIndex;
    auto sem = m.vkcore.swapchain.nPresentSemaphore(imageIndex);

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &sem,
        .swapchainCount = 1,
        .pSwapchains = &*m.vkcore.swapchain,
        .pImageIndices = &imageIndex,
    };

    VkResult result = VK_SUCCESS;
    {
      KT_PROFILE_SCOPE("vkQueuePresentKHR");
      VK_TRACE("Presenting swapchain image {} for frame {}.", imageIndex, m.frameInfo.index);
      result = vkQueuePresentKHR(m.vkcore.queues.present.queue, &presentInfo);
    }

    m.frameIndex = getNextFrameIndex();

#if RHI_LOG_LEVEL <= SPDLOG_LEVEL_DEBUG
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      VK_DEBUG("Swapchain is out of date during present");
    } else if (result == VK_SUBOPTIMAL_KHR || m.swapchainSuboptimal) {
      VK_DEBUG("Swapchain is suboptimal during present");
    }
#endif

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m.swapchainSuboptimal) {
      auto res = recreateSwapchain();
      if (!res) {
        VK_CRITICAL("Failed to recreate swapchain: {}", res.error());
        abort();
      }
    }

    KT_MARK_FRAME;
  }

  std::expected<void, std::string> RHI::recreateSwapchain() {
    KT_PROFILE_FUNCTION
    VK_TRACE("Recreating swapchain");
    VKH_MAKE(newSwapchain,
             setup::createSwapchain(m.vkcore.device, m.vkcore.device, m.window->getRenderSize(), m.vkcore.surface, m.vkcore.queues,
                                    *m.vkcore.swapchain),
             "Failed to recreate swapchain");

    {
      [[maybe_unused]]
      auto oldSwapchain = std::move(m.vkcore.swapchain);

      m.vkcore.swapchain = std::move(newSwapchain);

      VK_DEBUG("Waiting for device idle after swapchain recreation");
      vkDeviceWaitIdle(m.vkcore.device);
    }

    m.swapchainSuboptimal = false;

    VK_DEBUG("Swapchain recreated.");
    return {};
  }
} // namespace kt::rhi
