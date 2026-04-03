#include "keptech/vulkan/renderer.hpp"

#include "keptech/core/moveGuard.hpp"
#include "keptech/vulkan/helpers/swapchain.hpp"
#include "macros.hpp"
#include "vulkan/vulkan.h"
#include <keptech/maths/maths.hpp>

#include "setup/setup.hpp"
#include "vk-logger.hpp"
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/components/camera.hpp>
#include <keptech/core/window.hpp>
#include <set>

namespace kt::vkh {

  static_assert(CRenderer<Renderer>, "Renderer does not satisfy CRenderer concept");

  void Renderer::render() {
    startFrame();
    VkCommandBuffer cmdBuf = nullptr;
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m.frameInfo.perFrame->pools.graphics.pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VK_CHECK(vkAllocateCommandBuffers(m.vkcore.device.logical, &allocInfo, &cmdBuf), "Failed to allocate command buffer for frame");
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmdBuf, &beginInfo);
    VkImageMemoryBarrier2 swapToRender{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m.vkcore.swapchain.nImage(m.frameInfo.imageIndex),
        .subresourceRange =
            VkImageSubresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    VkDependencyInfo dependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &swapToRender,
    };

    vkCmdPipelineBarrier2(cmdBuf, &dependencyInfo);

    renderImGui(cmdBuf);
    endFrame(cmdBuf);
  }

  void Renderer::drawDeferred(VkCommandBuffer cmdBuf) {}

  void Renderer::newFrame() {
    imGuiNewFrame();
    auto& perFrame = m.vkcore.perFrame[m.frameInfo.index];

    auto nextImageRes = m.vkcore.swapchain.getNextImage(m.vkcore.device, perFrame.inFlightFence, perFrame.imageAvailableSemaphore);

    if (!nextImageRes) {
      VK_CRITICAL("Failed to acquire next swapchain image: {}", nextImageRes.error());
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

  void Renderer::startFrame() {
    VkResult res = VkResult::VK_TIMEOUT;
    uint64_t waitValue = m.frameInfo.perFrame->timelineValue;

    VkSemaphoreWaitInfo waitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores = &m.frameInfo.perFrame->timelineSemaphore,
        .pValues = &waitValue,
    };
    ;
    while (res = vkWaitSemaphores(m.vkcore.device.logical, &waitInfo, 0), res == VkResult::VK_TIMEOUT) {
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(100ms);
    }

    if (res != VK_SUCCESS) {
      VK_CRITICAL("Failed to wait for command buffer semaphore");
      abort();
    }

    m.frameInfo.perFrame->submittedCmds.clear();
    VK_ASSERT(m.frameInfo.perFrame->pools.graphics.pool != VK_NULL_HANDLE, "Graphics command pool is null");
    VK_ASSERT(m.frameInfo.perFrame->pools.compute.pool != VK_NULL_HANDLE, "Compute command pool is null");
    m.frameInfo.perFrame->pools.resetAll(m.vkcore.device.logical);

    auto& textureUpdates = m.frameInfo.perFrame->texToUpdate;
    if (!textureUpdates.empty()) {
      auto& globalDescSet = m.globalDescriptorSets.sets[m.frameInfo.index];
      std::vector<VkDescriptorImageInfo> imageInfos;
      imageInfos.reserve(textureUpdates.size());
      std::vector<VkWriteDescriptorSet> descriptorWrites;
      descriptorWrites.reserve(textureUpdates.size());
      for (auto& update : textureUpdates) {
        auto imageIndex = update.indexInDescriptorSet;

        imageInfos.push_back(VkDescriptorImageInfo{
            .sampler = m.imGuiObjects.sampler,
            .imageView = update.texture.view,
            .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        });

        descriptorWrites.push_back(VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = globalDescSet,
            .dstBinding = 1,
            .dstArrayElement = static_cast<uint32_t>(imageIndex),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfos.back(),
        });
      }

      VK_DEBUG("Updating {} texture descriptors for frame {}", descriptorWrites.size(), m.frameInfo.index);
      vkUpdateDescriptorSets(m.vkcore.device.logical, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
      textureUpdates.clear();
    }
  }

  void Renderer::renderImGui(VkCommandBuffer cmdBuf) {
    ImGui::Render();

    VkRenderingAttachmentInfo aInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = m.vkcore.swapchain.nView(m.frameInfo.imageIndex),
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

  void Renderer::submitCommandBuffers(std::vector<SubmitInfo>&& cmdBuffers) {
    if (cmdBuffers.empty()) {
      return;
    }

    uint64_t waitValue = m.frameInfo.perFrame->timelineValue++;
    uint64_t signalValue = waitValue + 1;
  }

  void Renderer::endFrame(VkCommandBuffer cmdBuf) { // NOLINT - The item in the UPtr is moved

    VkImageMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        .dstAccessMask = VK_ACCESS_2_NONE,
        .oldLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m.vkcore.swapchain.nImage(m.frameInfo.imageIndex),
        .subresourceRange =
            VkImageSubresourceRange{
                .aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VkDependencyInfo dependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };

    vkCmdPipelineBarrier2(cmdBuf, &dependencyInfo);

    vkEndCommandBuffer(cmdBuf);

    auto& sem = m.vkcore.swapchain.nPresentSemaphore(m.frameInfo.imageIndex);

    VkSemaphoreSubmitInfo waitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = m.vkcore.perFrame[m.frameInfo.index].timelineSemaphore,
        .value = m.vkcore.perFrame[m.frameInfo.index].timelineValue,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .deviceIndex = 0,
    };

    VkSemaphoreSubmitInfo imageAvailableWaitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = m.vkcore.perFrame[m.frameInfo.index].imageAvailableSemaphore,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    };

    std::array<VkSemaphoreSubmitInfo, 2> waitSemaphores{waitInfo, imageAvailableWaitInfo};

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
        .waitSemaphoreInfoCount = 2,
        .pWaitSemaphoreInfos = waitSemaphores.data(),
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdBufInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalInfo,
    };

    vkResetFences(m.vkcore.device.logical, 1, &m.vkcore.perFrame[m.frameInfo.index].inFlightFence);

    vkQueueSubmit2(m.vkcore.queues.graphics.queue, 1, &submitInfo, m.vkcore.perFrame[m.frameInfo.index].inFlightFence);

    m.frameInfo.perFrame->submittedCmds.emplace_back(SubmittedCommandBufferInfo{
        .buffer = cmdBuf,
    });

    present();
  }

  void Renderer::present() {
    uint32_t imageIndex = m.frameInfo.imageIndex;
    auto& sem = m.vkcore.swapchain.nPresentSemaphore(imageIndex);

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &sem,
        .swapchainCount = 1,
        .pSwapchains = &m.vkcore.swapchain.getSwapchain(),
        .pImageIndices = &imageIndex,
    };

    auto result = vkQueuePresentKHR(m.vkcore.queues.present.queue, &presentInfo);

    m.frameInfo.index = m.frameInfo.nextIndex; // Advance to next frame index

    m.frameInfo.nextIndex = (m.frameInfo.nextIndex + 1) % MAX_FRAMES_IN_FLIGHT;

#ifndef NDEBUG
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      VK_DEBUG("Swapchain is out of date during present");
    } else if (result == VK_SUBOPTIMAL_KHR) {
      VK_DEBUG("Swapchain is suboptimal during present");
    } else if (m.frameInfo.suboptimalSwapchain) {
      VK_DEBUG("Swapchain was suboptimal at image aquire");
    }
#endif

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m.frameInfo.suboptimalSwapchain) {
      auto res = recreateSwapchain();
      if (!res) {
        VK_CRITICAL("Failed to recreate swapchain: {}", res.error());
        abort();
      }
    }
  }

  std::expected<void, std::string> Renderer::recreateSwapchain() {
    VK_TRACE("Recreating swapchain");
    VKH_MAKE(newSwapchain,
             setup::createSwapchain(m.vkcore.device.physical, m.window->getRenderSize(), m.vkcore.device.logical, m.vkcore.surface,
                                    m.vkcore.queues, m.vkcore.swapchain.getSwapchain()),
             "Failed to recreate swapchain");

    {
      [[maybe_unused]]
      auto oldSwapchain = std::move(m.vkcore.swapchain);

      m.vkcore.swapchain = std::move(newSwapchain);

      vkQueueWaitIdle(m.vkcore.queues.graphics.queue);
    }

    m.frameInfo.suboptimalSwapchain = false;

    return {};
  }

} // namespace kt::vkh
