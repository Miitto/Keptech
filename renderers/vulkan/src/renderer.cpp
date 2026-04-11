#include "keptech/vulkan/renderer.hpp"

#include "keptech/components/lights.hpp"
#include "keptech/vulkan/helpers/swapchain.hpp"
#include "macros.hpp"
#include <keptech/maths/maths.hpp>

#include "keptech/vulkan/constants.hpp"
#include "setup/setup.hpp"
#include "vk-logger.hpp"
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/components/camera.hpp>
#include <keptech/core/window.hpp>
#include <keptech/rendering/structs.hpp>

namespace {
  enum class DebugView : uint8_t {
    Albedo,
    Normal,
    Emissive,
    MetRough,
    Diffuse,
    Specular,
    Combined,
  };
  static DebugView debugView = DebugView::Combined;
} // namespace

template <> struct fmt::formatter<DebugView> : fmt::formatter<std::string_view> {
  template <typename FormatContext> auto format(DebugView v, FormatContext& ctx) const {
    std::string_view name = "Unknown";
    switch (v) {
    case DebugView::Albedo:
      name = "Albedo";
      break;
    case DebugView::Normal:
      name = "Normal";
      break;
    case DebugView::Emissive:
      name = "Emissive";
      break;
    case DebugView::MetRough:
      name = "Metalness/Roughness";
      break;
    case DebugView::Diffuse:
      name = "Diffuse Light";
      break;
    case DebugView::Specular:
      name = "Specular Light";
      break;
    case DebugView::Combined:
      name = "Combined";
      break;
    }

    return fmt::formatter<std::string_view>::format(name, ctx);
  }
};

namespace kt::vkh {
  static_assert(CRenderer<Renderer>, "Renderer does not satisfy CRenderer concept");

  void Renderer::debugUi() {
    ImGui::Begin("Debug View");

    auto preview = fmt::format("{}", debugView);

    if (ImGui::BeginCombo("View", preview.c_str(), 0)) {
      if (ImGui::Selectable("Albedo", debugView == DebugView::Albedo)) {
        debugView = DebugView::Albedo;
      }
      if (ImGui::Selectable("Normal", debugView == DebugView::Normal)) {
        debugView = DebugView::Normal;
      }
      if (ImGui::Selectable("Emissive", debugView == DebugView::Emissive)) {
        debugView = DebugView::Emissive;
      }
      if (ImGui::Selectable("Metalness/Roughness", debugView == DebugView::MetRough)) {
        debugView = DebugView::MetRough;
      }
      if (ImGui::Selectable("Diffuse Light", debugView == DebugView::Diffuse)) {
        debugView = DebugView::Diffuse;
      }
      if (ImGui::Selectable("Specular Light", debugView == DebugView::Specular)) {
        debugView = DebugView::Specular;
      }
      if (ImGui::Selectable("Combined", debugView == DebugView::Combined)) {
        debugView = DebugView::Combined;
      }
      ImGui::EndCombo();
    }

    ImGui::End();
  }

  void Renderer::render() {
    startFrame();

    {
      auto view = scene->getEcs().view<components::Transform>();
      for (auto [entity, transform] : view.each()) {
        transform.recalculateGlobalTransform();
      }
    }

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

    updateCameraBuffer(cmdBuf);
    drawDeferred(cmdBuf);
    drawLights(cmdBuf);

#ifndef NDEBUG
    {
      VkImage i = nullptr;
      switch (debugView) {
      case DebugView::Albedo:
        i = m.renderTargets.gBuffer.albedo.image;
        break;
      case DebugView::Normal:
        i = m.renderTargets.gBuffer.normal.image;
        break;
      case DebugView::Emissive:
        i = m.renderTargets.gBuffer.emissive.image;
        break;
      case DebugView::MetRough:
        i = m.renderTargets.gBuffer.metRough.image;
        break;
      case DebugView::Diffuse:
        i = m.renderTargets.lights.diffuse.image;
        break;
      case DebugView::Specular:
        i = m.renderTargets.lights.specular.image;
        break;
      case DebugView::Combined:
        i = m.renderTargets.lights.combined.image;
        break;
      }
      std::array transitions{
          VkImageMemoryBarrier2{
              .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
              .srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
              .srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
              .dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
              .dstAccessMask = VK_ACCESS_2_NONE,
              .oldLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
              .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
              .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .image = i,
              .subresourceRange =
                  VkImageSubresourceRange{
                      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                      .baseMipLevel = 0,
                      .levelCount = 1,
                      .baseArrayLayer = 0,
                      .layerCount = 1,
                  },
          },
          VkImageMemoryBarrier2{
              .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
              .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
              .srcAccessMask = VK_ACCESS_2_NONE,
              .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
              .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
              .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
              .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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
          },
      };
      VkDependencyInfo dependencyInfo{
          .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = transitions.size(),
          .pImageMemoryBarriers = transitions.data(),
      };
      vkCmdPipelineBarrier2(cmdBuf, &dependencyInfo);

      VkImageBlit2 blitRegion{
          .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
          .srcSubresource =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .mipLevel = 0,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
          .srcOffsets =
              {
                  VkOffset3D{.x = 0, .y = 0, .z = 0},
                  VkOffset3D{.x = m.renderTargets.framebufferSize.x, .y = m.renderTargets.framebufferSize.y, .z = 1},
              },
          .dstSubresource =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .mipLevel = 0,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
          .dstOffsets =
              {
                  VkOffset3D{.x = 0, .y = 0, .z = 0},
                  VkOffset3D{.x = static_cast<int32_t>(m.vkcore.swapchain.config().extent.width),
                             .y = static_cast<int32_t>(m.vkcore.swapchain.config().extent.height),
                             .z = 1},
              },
      };
      VkBlitImageInfo2 blitInfo{
          .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
          .srcImage = i,
          .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          .dstImage = m.vkcore.swapchain.nImage(m.frameInfo.imageIndex),
          .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .regionCount = 1,
          .pRegions = &blitRegion,
      };
      vkCmdBlitImage2(cmdBuf, &blitInfo);
    }

    debugUi();
#endif
    renderImGui(cmdBuf);
    endFrame(cmdBuf);
  }

  void Renderer::updateCameraBuffer(VkCommandBuffer cmdBuf) {
    auto [camT, cam] = scene->getActiveCamera().getComponents<components::Transform, components::Camera>();

    cam.recalculateProjectionMatrix();
    auto projection = cam.getProjectionMatrix();
    auto invView = camT.getGlobal();
    auto viewMat = glm::inverse(invView);
    auto invProj = glm::inverse(projection);

    auto viewProj = projection * viewMat;
    auto invViewProj = glm::inverse(viewProj);

    components::Camera::Uniforms camUniforms{
        .projectionMatrix = projection,
        .viewMatrix = viewMat,
        .viewProjectionMatrix = viewProj,
        .invProjectionMatrix = invProj,
        .invViewMatrix = invView,
        .invViewProjectionMatrix = invViewProj,
        .viewportSize = {m.renderTargets.framebufferSize.x, m.renderTargets.framebufferSize.y},
    };

    size_t sizePerCamera = maths::roundToAlignment(sizeof(components::Camera::Uniforms), limits::minUniformBufferOffsetAlignment);
    size_t offset = m.frameInfo.index * sizePerCamera;

    memcpy(m.buffers.camera.mapping() + offset, &camUniforms, sizeof(components::Camera::Uniforms));
  }

  void Renderer::drawDeferred(VkCommandBuffer cmdBuf) {
    deferredToRenderable(cmdBuf);
    deferredBeginRendering(cmdBuf);

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.deferred.pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.deferred.layout, 0, 1,
                            &m.globalDescriptorSets.sets[m.frameInfo.index], 0, nullptr);
    setupViewportAndScissor(cmdBuf);

    auto view = scene->getEcs().view<components::Transform, components::Mesh>();

    constexpr VkDeviceSize vertexOffest = 0;
    for (auto [entity, transform, mesh] : view.each()) {
      vkCmdBindVertexBuffers(cmdBuf, 0, 1, &mesh.getRMesh().vertexBuffer.buffer, &vertexOffest);
      vkCmdBindIndexBuffer(cmdBuf, mesh.getRMesh().indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

      auto model = transform.getGlobal();

      vkCmdPushConstants(cmdBuf, m.pipelines.deferred.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(glm::mat4), &model);

      for (const auto& submesh : mesh.getSubmeshes()) {
        VkDeviceAddress matAddress = submesh.material.has_value() ? submesh.material->address : 0;
        vkCmdPushConstants(cmdBuf, m.pipelines.deferred.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           sizeof(glm::mat4), sizeof(VkDeviceAddress), &matAddress);

        vkCmdDrawIndexed(cmdBuf, submesh.count, 1, submesh.start, 0, 0);
      }
    }

    vkCmdEndRendering(cmdBuf);

    deferredToShaderRead(cmdBuf);
  }

  void Renderer::drawLights(VkCommandBuffer cmdBuf) {

    lightsToRenderable(cmdBuf);

    drawPointLights(cmdBuf);

    seperatedLightsToShaderRead(cmdBuf);

    combineLights(cmdBuf);

    combinedLightToShaderRead(cmdBuf);
  }

  void Renderer::drawPointLights(VkCommandBuffer cmdBuf) {
    seperatedLightsBeginRendering(cmdBuf);

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.deferredPointLight.pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.deferredPointLight.layout, 0, 1,
                            &m.globalDescriptorSets.sets[m.frameInfo.index], 0, nullptr);

    setupViewportAndScissor(cmdBuf);

    rendering::PointLight light;

    auto view = scene->getEcs().view<components::Transform, components::PointLight>();
    for (auto [entity, transform, pointLight] : view.each()) {
      auto model = transform.getGlobal();

      light.position = {model[3].x, model[3].y, model[3].z};
      light.radius = pointLight.radius;
      light.color = pointLight.color;
      light.intensity = pointLight.intensity;

      vkCmdPushConstants(cmdBuf, m.pipelines.deferredPointLight.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                         sizeof(rendering::PointLight), &light);

      vkCmdDraw(cmdBuf, 36, 1, 0, 0);
    }

    vkCmdEndRendering(cmdBuf);
  }

  void Renderer::combineLights(VkCommandBuffer cmdBuf) {
    combinedLightBeginRendering(cmdBuf);

    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.deferredCombine.pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.deferredCombine.layout, 0, 1,
                            &m.globalDescriptorSets.sets[m.frameInfo.index], 0, nullptr);

    setupViewportAndScissor(cmdBuf);

    vkCmdDraw(cmdBuf, 3, 1, 0, 0);

    vkCmdEndRendering(cmdBuf);
  }

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
    VK_ASSERT(m.frameInfo.perFrame->pools.graphics.pool != VK_NULL_HANDLE, "Graphics command pool is null");
    VK_ASSERT(m.frameInfo.perFrame->pools.compute.pool != VK_NULL_HANDLE, "Compute command pool is null");
    m.frameInfo.perFrame->pools.resetAll(m.vkcore.device.logical);

    updateTextureDescriptors();
  }

  void Renderer::renderImGui(VkCommandBuffer cmdBuf) {
    ImGui::Render();
    VkImageMemoryBarrier2 swapToRender{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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
    VkDependencyInfo swapDep{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &swapToRender,
    };
    vkCmdPipelineBarrier2(cmdBuf, &swapDep);

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

    VkSemaphoreSubmitInfo imageAvailableWaitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = m.vkcore.perFrame[m.frameInfo.index].imageAvailableSemaphore,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
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
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &imageAvailableWaitInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdBufInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalInfo,
    };

    vkResetFences(m.vkcore.device.logical, 1, &m.vkcore.perFrame[m.frameInfo.index].inFlightFence);

    vkQueueSubmit2(m.vkcore.queues.graphics.queue, 1, &submitInfo, m.vkcore.perFrame[m.frameInfo.index].inFlightFence);

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
