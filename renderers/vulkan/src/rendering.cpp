#include "keptech/vulkan/renderer.hpp"
#include "macros.hpp"
#include "vulkan/vulkan.hpp"
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>

namespace keptech::vkh {

  void Renderer::draw(const Frame& info,
                      const vk::raii::CommandBuffer& graphicsCmdBuffer) {
    vk::RenderingAttachmentInfo aInfo{
        .imageView = vkcore.swapchain.nImageView(info.imageIndex),
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = {
            .color = {std::array<float, 4>{0.1f, 0.1f, 0.1f, 1.0f}}}};

    vk::RenderingInfo renderingInfo{
        .renderArea =
            {
                .offset = {.x = 0, .y = 0},
                .extent = vkcore.swapchain.config().extent,
            },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &aInfo,
    };

    graphicsCmdBuffer.beginRendering(renderingInfo);

    auto objLists = buildRenderObjectLists(maths::Frustum{});

    for (auto& renderObject : objLists.forward) {
      auto& material = *renderObject.material;
      auto& mesh = *renderObject.mesh;

      graphicsCmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                     material.pipeline);

      setupGraphicsCommandBuffer(info, graphicsCmdBuffer);

      struct PushConstantData {
        vk::DeviceAddress vertexBufferAddress;
      } pushConstantData{
          .vertexBufferAddress = mesh.vertexBuffer.address,
      };

      graphicsCmdBuffer.pushConstants<PushConstantData>(
          material.pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0,
          pushConstantData);

      for (const auto& submesh : mesh.submeshes) {
        if (mesh.indexBuffer.has_value()) {
          graphicsCmdBuffer.bindIndexBuffer(mesh.indexBuffer->buffer, 0,
                                            vk::IndexType::eUint32);
          graphicsCmdBuffer.drawIndexed(submesh.indexCount, 1,
                                        submesh.indexOffset, 0, 0);
        } else {
          graphicsCmdBuffer.draw(submesh.indexCount, 1, 0, 0);
        }
      }
    }

    graphicsCmdBuffer.endRendering();
  }

  void Renderer::render() {
    Frame info = startFrame();

    vk::CommandBufferAllocateInfo cmdBufAllocInfo{
        .commandPool =
            *vkcore.frameResources[info.index].pools.graphics.get()->pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };
    auto graphicsCmdBuffers_res =
        vkcore.device->allocateCommandBuffers(cmdBufAllocInfo);
    if (!graphicsCmdBuffers_res.has_value()) {
      VK_CRITICAL("Failed to allocate graphics command buffer: {}",
                  vk::to_string(graphicsCmdBuffers_res.result));
      abort();
    }
    vk::raii::CommandBuffer graphicsCmdBuffer =
        std::move(graphicsCmdBuffers_res.value.front());

    graphicsCmdBuffer.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

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
        .image = vkcore.swapchain.nImage(info.imageIndex),
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
        .pImageMemoryBarriers = &toDrawableBarrier,
    });

    draw(info, graphicsCmdBuffer);

    drawImGui(info, graphicsCmdBuffer);

    vk::ImageMemoryBarrier2 toPresentBarrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentRead |
                         vk::AccessFlagBits2::eColorAttachmentWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
        .dstAccessMask = vk::AccessFlagBits2::eNone,
        .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .newLayout = vk::ImageLayout::ePresentSrcKHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vkcore.swapchain.nImage(info.imageIndex),
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
        .pImageMemoryBarriers = &toPresentBarrier,
    });

    graphicsCmdBuffer.end();

    vk::SemaphoreSubmitInfo waitSemaphoreSubmitInfo{
        .semaphore = *info.syncObjects.get().presentCompleteSemaphore,
        .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .deviceIndex = 0,
    };

    vk::CommandBufferSubmitInfo commandBufferSubmitInfo{
        .commandBuffer = graphicsCmdBuffer, .deviceMask = 0};

    vk::SemaphoreSubmitInfo signalSemaphoreSubmitInfo{
        .semaphore = *info.syncObjects.get().renderCompleteSemaphore,
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
        .deviceIndex = 0,
    };

    vk::SubmitInfo2 graphicsSubmitInfo{
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &waitSemaphoreSubmitInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commandBufferSubmitInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalSemaphoreSubmitInfo,
    };

    auto result = vkcore.queues.graphics.queue->submit2(
        graphicsSubmitInfo, *info.syncObjects.get().drawingFence);
    if (result != vk::Result::eSuccess) {
      VK_CRITICAL("Failed to submit graphics command buffer: {}",
                  vk::to_string(result));
      abort();
    }

    registerCommandBuffer(info.index, std::move(graphicsCmdBuffer));

    presentFrame(info);

    endFrame();
  }

  void Renderer::drawImGui(const Frame& info,
                           const vk::raii::CommandBuffer& graphicsCmdBuffer) {
    ImGui::Render();

    vk::RenderingAttachmentInfo aInfo{
        .imageView = vkcore.swapchain.nImageView(info.imageIndex),
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

    graphicsCmdBuffer.beginRendering(renderingInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *graphicsCmdBuffer);

    graphicsCmdBuffer.endRendering();
  }
} // namespace keptech::vkh
