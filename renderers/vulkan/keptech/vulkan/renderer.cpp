#include "keptech/vulkan/renderer.hpp"

#include "interface.hpp"
#include "keptech/components/lights.hpp"
#include "keptech/maths/frustum.hpp"
#include "keptech/vulkan/wrappers/swapchain.hpp"
#include "macros.hpp"
#include <keptech/maths/maths.hpp>

#include "keptech/vulkan/constants.hpp"
#include "passes/global.hpp"
#include "profile.hpp"
#include "setup/setup.hpp"
#include "vk-logger.hpp"
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/components/camera.hpp>
#include <keptech/core/profile.hpp>
#include <keptech/core/window.hpp>
#include <keptech/rendering/structs.hpp>

namespace kt::vkh {
  static_assert(CRenderer<Renderer>, "Renderer does not satisfy CRenderer concept");

  constexpr VkDeviceSize NO_VERTEX_OFFSET = 0;

  void Renderer::debugUi() {
    ImGui::Begin("Debug View");

    auto camera = scene->getActiveCamera();
    auto& camT = camera.getComponents<components::Transform>();
    auto camPos = camT.getGlobal()[3];

    ImGui::Text("Camera Position: %.2f, %.2f, %.2f", camPos.x, camPos.y, camPos.z);

    ImGui::Text("Culled Lights: %zu", m.frameInfo.culledLights);

    ImGui::End();
  }

  void Renderer::render() {
    KT_PROFILE_FUNCTION
    VK_TRACE("Frame Start");
    startFrame();

    components::Transform::recalcAllTransforms(scene->getEcs());
    auto frustum = passes::writeCameraData(m.buffers, scene->getActiveCamera(), m.renderTargets.framebufferSize, m.frameInfo.index);

    auto cmdBuf = m.frameInfo.perFrame->pools.graphics.allocate(m.vkcore.device.logical);

    cmdBuf.begin();

#ifndef NDEBUG
    debugUi();
#endif
    layoutTransitions<1>(cmdBuf, {layoutTransition(m.vkcore.swapchain.nImage(m.frameInfo.imageIndex),
                                                   TransitionInfo(ImageType::Color, ImageLayout::Undefined, ImageLayout::RenderTarget))});
    renderImGui(cmdBuf);

    layoutTransitions<1>(cmdBuf, {layoutTransition(m.vkcore.swapchain.nImage(m.frameInfo.imageIndex),
                                                   TransitionInfo(ImageType::Color, ImageLayout::RenderTarget, ImageLayout::Present))});

    cmdBuf.end();
    endFrame(cmdBuf);
  }

  void Renderer::newFrame() {
    KT_PROFILE_FUNCTION
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
    KT_PROFILE_FUNCTION
    VK_ASSERT(m.frameInfo.perFrame->pools.graphics.pool != VK_NULL_HANDLE, "Graphics command pool is null");
    VK_ASSERT(m.frameInfo.perFrame->pools.compute.pool != VK_NULL_HANDLE, "Compute command pool is null");
    m.frameInfo.perFrame->pools.resetAll(m.vkcore.device.logical);

    m.frameInfo.culledDraws = 0;
    m.frameInfo.culledShadowDraws = 0;
    m.frameInfo.culledLights = 0;

    updateTextureDescriptors();

    BufferPointers bufferAddresses{
        .vertexPositions = m.buffers.vertexPositions->buffer.address(),
        .vertexAttribs = m.buffers.vertexAttribs->buffer.address(),
        .indices = m.buffers.indices->buffer.address(),
        .meshlets = m.buffers.meshlets->buffer.address(),
        .meshletVertices = m.buffers.meshletVertices->buffer.address(),
        .meshletTriangles = m.buffers.meshletTriangles->buffer.address(),
    };

    auto frameIndex = m.frameInfo.index;
    size_t size = maths::roundToAlignment(sizeof(BufferPointers), limits::minUniformBufferOffsetAlignment);
    memcpy(m.buffers.addresses->mapping() + (size * frameIndex), &bufferAddresses, sizeof(BufferPointers));
  }

  void Renderer::renderImGui(VkCommandBuffer cmdBuf) {
    KT_PROFILE_FUNCTION
    KT_VK_ZONE(m.tracyGraphicsContext, cmdBuf, "Render ImGui");
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

  void Renderer::endFrame(VkCommandBuffer cmdBuf) {
    KT_PROFILE_FUNCTION
    auto& sem = m.vkcore.swapchain.nPresentSemaphore(m.frameInfo.imageIndex);

    std::array<VkSemaphoreSubmitInfo, 2> waitInfo{
        VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = m.vkcore.perFrame[m.frameInfo.index].imageAvailableSemaphore,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        },
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = m.vkcore.timelineSemaphore,
            .value = m.frameInfo.ssaoTimelineSubmit,
            .stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
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

    vkResetFences(m.vkcore.device.logical, 1, &m.vkcore.perFrame[m.frameInfo.index].inFlightFence);

    auto res = vkQueueSubmit2(m.vkcore.queues.graphics.queue, 1, &submitInfo, m.vkcore.perFrame[m.frameInfo.index].inFlightFence);
    VK_ASSERT(res == VK_SUCCESS, "Failed to submit command buffer: {}", res);

    present();
  }

  void Renderer::present() {
    KT_PROFILE_FUNCTION
    uint32_t imageIndex = m.frameInfo.imageIndex;
    auto& sem = m.vkcore.swapchain.nPresentSemaphore(imageIndex);

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
      result = vkQueuePresentKHR(m.vkcore.queues.present.queue, &presentInfo);
    }

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

    KT_MARK_FRAME;
  }

  std::expected<void, std::string> Renderer::recreateSwapchain() {
    KT_PROFILE_FUNCTION
    VK_TRACE("Recreating swapchain");
    VKH_MAKE(newSwapchain,
             setup::createSwapchain(m.vkcore.device.physical, m.window->getRenderSize(), m.vkcore.device.logical, m.vkcore.surface,
                                    m.vkcore.queues, *m.vkcore.swapchain),
             "Failed to recreate swapchain");

    {
      [[maybe_unused]]
      auto oldSwapchain = std::move(m.vkcore.swapchain);

      m.vkcore.swapchain = std::move(newSwapchain);

      VK_DEBUG("Waiting for device idle after swapchain recreation");
      vkDeviceWaitIdle(m.vkcore.device.logical);
    }

    m.frameInfo.suboptimalSwapchain = false;

    VK_DEBUG("Swapchain recreated.");
    return {};
  }

} // namespace kt::vkh
