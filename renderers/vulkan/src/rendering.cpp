#include "keptech/vulkan/renderer.hpp"

#include "vk-logger.hpp"
#include "vulkan/vulkan.hpp"
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/core/components/camera.hpp>
#include <vulkan/vulkan_core.h>

namespace keptech::vkh {

  void Renderer::imagesToRenderable(
      const Frame& info, const vk::raii::CommandBuffer& graphicsCmdBuffer) {
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

    vk::ImageMemoryBarrier2 gBufferColorToDrawableBarrier = toDrawableBarrier;
    gBufferColorToDrawableBarrier.image = vkcore.gBuffer.color.image;
    vk::ImageMemoryBarrier2 gBufferNormalToDrawableBarrier = toDrawableBarrier;
    gBufferNormalToDrawableBarrier.image = vkcore.gBuffer.normal.image;

    vk::ImageMemoryBarrier2 gBufferDepthToDrawableBarrier = toDrawableBarrier;
    gBufferDepthToDrawableBarrier.image = vkcore.gBuffer.depth.image;
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

  void
  Renderer::drawDeferred(const Frame& info,
                         const vk::raii::CommandBuffer& graphicsCmdBuffer) {

    vk::RenderingAttachmentInfo deferredAlbedoAttachmentInfo{
        .imageView = vkcore.gBuffer.color.view,
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = {.color = {std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}},
    };
    vk::RenderingAttachmentInfo deferredNormalAttachmentInfo{
        .imageView = vkcore.gBuffer.normal.view,
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = {.color = {std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}},
    };
    vk::RenderingAttachmentInfo deferredDepthAttachmentInfo{
        .imageView = vkcore.gBuffer.depth.view,
        .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = {.depthStencil = {.depth = 1.0f, .stencil = 0}},
    };
    std::array<vk::RenderingAttachmentInfo, 2> deferredColorAttachments{
        deferredAlbedoAttachmentInfo,
        deferredNormalAttachmentInfo,
    };

    auto& scene = *frameScene;
    auto view =
        scene.getEcs().view<components::Transform, components::Camera>();
    for (auto [entity, transform, camera] : view.each()) {
      transform.recalculateGlobalTransform();
      glm::mat4 invViewMatrix = transform.getGlobal().toMatrix(true);
      auto cPos = transform.getGlobal().pos();
      glm::mat4 viewMatrix = glm::inverse(invViewMatrix);

      glm::mat4 projectionMatrix = camera.getProjectionMatrix();
      glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix;
      glm::mat4 invProjectionMatrix = glm::inverse(projectionMatrix);
      glm::mat4 invViewProjectionMatrix = invViewMatrix * invProjectionMatrix;

      components::Camera::Uniforms uniforms{
          .projectionMatrix = projectionMatrix,
          .viewMatrix = viewMatrix,
          .viewProjectionMatrix = viewProjectionMatrix,
          .invProjectionMatrix = invProjectionMatrix,
          .invViewMatrix = invViewMatrix,
          .invViewProjectionMatrix = invViewProjectionMatrix,
      };

      {
        vk::BufferMemoryBarrier2 cameraBufferBarrier{
            .srcStageMask = vk::PipelineStageFlagBits2::eVertexShader,
            .srcAccessMask = vk::AccessFlagBits2::eShaderRead,
            .dstStageMask = vk::PipelineStageFlagBits2::eHost,
            .dstAccessMask = vk::AccessFlagBits2::eHostWrite,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = cameraObjects.uniformBuffer.buffer,
            .offset = 0,
            .size = sizeof(components::Camera::Uniforms),
        };

        graphicsCmdBuffer.pipelineBarrier2(vk::DependencyInfo{
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &cameraBufferBarrier,
        });
      }

      memcpy(cameraObjects.uniformBuffer.mapping(), &uniforms,
             sizeof(components::Camera::Uniforms));

      {
        vk::BufferMemoryBarrier2 cameraBufferBarrier{
            .srcStageMask = vk::PipelineStageFlagBits2::eHost,
            .srcAccessMask = vk::AccessFlagBits2::eHostWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eVertexShader,
            .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = cameraObjects.uniformBuffer.buffer,
            .offset = 0,
            .size = sizeof(components::Camera::Uniforms),
        };

        graphicsCmdBuffer.pipelineBarrier2(vk::DependencyInfo{
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &cameraBufferBarrier,
        });
      }

      vk::RenderingInfo renderingInfo{
          .renderArea =
              {
                  .offset = {.x = 0, .y = 0},
                  .extent = {.width = vkcore.gBuffer.color.extent.width,
                             .height = vkcore.gBuffer.color.extent.height},
              },
          .layerCount = 1,
          .colorAttachmentCount = 2,
          .pColorAttachments = deferredColorAttachments.data(),
          .pDepthAttachment = &deferredDepthAttachmentInfo,
      };

      auto objLists = buildRenderObjectLists(
          scene,
          maths::Frustum::fromViewProjectionMatrix(viewProjectionMatrix));

      size_t maxObjTypeCount =
          std::max({objLists.deferred.size(), objLists.forward.size(),
                    objLists.transparent.size()});

      VK_TRACE("Drawing {} deferred objects", objLists.deferred.size());

      auto instanceBufferSize = instanceBuffers.maxInstances();
      if (instanceBufferSize < maxObjTypeCount) {
        auto res = instanceBuffers.resize(
            vkcore.allocator, vkcore.device.logical, maxObjTypeCount);
        if (!res.has_value()) {
          VK_ERROR("Failed to resize instance buffers: {}", res.error());
          return;
        }
      }

      {
        size_t objIndex = 0;
        for (; objIndex < objLists.deferred.size(); ++objIndex) {
          auto& transform = objLists.deferred[objIndex].transform;

          InstanceData data{
              .modelMatrix = transform.toMatrix(),
          };

          memcpy(instanceBuffers.staging.mapping() +
                     (objIndex * sizeof(InstanceData)),
                 &data, sizeof(InstanceData));
        }

        for (int i = 0; i < objLists.forward.size(); ++i, ++objIndex) {
          auto& transform = objLists.forward[i].transform;

          InstanceData data{
              .modelMatrix = transform.toMatrix(),
          };

          memcpy(instanceBuffers.staging.mapping() +
                     (objIndex * sizeof(InstanceData)),
                 &data, sizeof(InstanceData));
        }

        for (int i = 0; i < objLists.transparent.size(); ++i, ++objIndex) {
          auto& transform = objLists.transparent[i].transform;

          InstanceData data{
              .modelMatrix = transform.toMatrix(),
          };

          memcpy(instanceBuffers.staging.mapping() +
                     (objIndex * sizeof(InstanceData)),
                 &data, sizeof(InstanceData));
        }
      }

      auto instanceDataTransferRes = instanceBuffers.copyToDevice(
          vkcore.device.logical, graphicsCmdBuffer, maxObjTypeCount);
      if (!instanceDataTransferRes) {
        VK_ERROR("Failed to copy instance data to device: {}",
                 instanceDataTransferRes.error());
        return;
      }

      graphicsCmdBuffer.beginRendering(renderingInfo);
      size_t instanceOffset = 0;
      for (auto& renderObject : objLists.deferred) {
        auto& material = *renderObject.material;
        auto& mesh = *renderObject.mesh;

        graphicsCmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                       material.pipeline);

        setupGraphicsCommandBuffer(info, graphicsCmdBuffer, camera);

        graphicsCmdBuffer.bindDescriptorSets2({
            .stageFlags = vk::ShaderStageFlagBits::eVertex |
                          vk::ShaderStageFlagBits::eFragment,
            .layout = material.pipelineLayout,
            .firstSet = 0,
            .descriptorSetCount = 1,
            .pDescriptorSets = &*cameraObjects.descriptorSet,
        });

        struct PushConstantData {
          vk::DeviceAddress vertexBufferAddress;
          vk::DeviceAddress transformAddress;
          uint32_t instanceOffset;
        } pushConstantData{
            .vertexBufferAddress = mesh.vertexBuffer.address,
            .transformAddress = instanceBuffers.device.address,
            .instanceOffset = static_cast<uint32_t>(instanceOffset),
        };

        graphicsCmdBuffer.pushConstants<PushConstantData>(
            material.pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0,
            pushConstantData);

        for (const auto& submesh : mesh.getSubmeshes()) {
          if (mesh.indexBuffer.has_value()) {
            graphicsCmdBuffer.bindIndexBuffer(mesh.indexBuffer->buffer, 0,
                                              vk::IndexType::eUint32);
            graphicsCmdBuffer.drawIndexed(submesh.indexCount, 1,
                                          submesh.indexOffset, 0, 0);
          } else {
            graphicsCmdBuffer.draw(submesh.indexCount, 1, 0, 0);
          }
        }
        ++instanceOffset;
      }

      graphicsCmdBuffer.endRendering();
    }
  }

  void Renderer::gBufferToAttachments(
      const Frame& info, const vk::raii::CommandBuffer& graphicsCmdBuffer) {
    vk::ImageMemoryBarrier2 colors{
        .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentRead |
                         vk::AccessFlagBits2::eColorAttachmentWrite,
        .dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
        .dstAccessMask = vk::AccessFlagBits2::eShaderRead,
        .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vkcore.gBuffer.color.image,
        .subresourceRange =
            vk::ImageSubresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vk::ImageMemoryBarrier2 normals = colors;
    normals.image = vkcore.gBuffer.normal.image;

    vk::ImageMemoryBarrier2 depths = colors;
    depths.image = vkcore.gBuffer.depth.image;
    depths.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    depths.oldLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    depths.srcStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                          vk::PipelineStageFlagBits2::eLateFragmentTests;
    depths.srcAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentRead |
                           vk::AccessFlagBits2::eDepthStencilAttachmentWrite;

    std::array<vk::ImageMemoryBarrier2, 3> barriers{
        colors,
        normals,
        depths,
    };

    graphicsCmdBuffer.pipelineBarrier2(vk::DependencyInfo{
        .imageMemoryBarrierCount = barriers.size(),
        .pImageMemoryBarriers = barriers.data(),
    });
  }

  void Renderer::render() {
    VK_TRACE("Rendering {} scenes", frameScenes.size());
    Frame info = startFrame();

    vk::CommandBufferAllocateInfo cmdBufAllocInfo{
        .commandPool = *info.perFrame.get().pools.graphics.get()->pool,
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

    imagesToRenderable(info, graphicsCmdBuffer);

    drawDeferred(info, graphicsCmdBuffer);

    gBufferToAttachments(info, graphicsCmdBuffer);

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

    vk::PipelineStageFlags waitDestinationStageMask(
        vk::PipelineStageFlagBits::eColorAttachmentOutput);
    vk::SubmitInfo graphicsSubmitInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*info.perFrame.get().imageAvailableSemaphore,
        .pWaitDstStageMask = &waitDestinationStageMask,
        .commandBufferCount = 1,
        .pCommandBuffers = &*graphicsCmdBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores =
            &*vkcore.swapchain.nPresentSemaphore(info.imageIndex),
    };

    auto& inFlightFence = info.perFrame.get().inFlightFence;

    vkcore.device.logical.resetFences({inFlightFence});

    auto result =
        vkcore.queues.graphics.queue->submit(graphicsSubmitInfo, inFlightFence);
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
        .imageView = vkcore.swapchain.nView(info.imageIndex),
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
