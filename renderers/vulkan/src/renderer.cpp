#include "keptech/vulkan/renderer.hpp"

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

namespace kt::vkh {

  static_assert(CRenderer<Renderer>, "Renderer does not satisfy CRenderer concept");

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

    {
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

    drawDeferred(cmdBuf);

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
            .image = m.renderTargets.gBuffer.albedo.image,
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
        .srcImage = m.renderTargets.gBuffer.albedo.image,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage = m.vkcore.swapchain.nImage(m.frameInfo.imageIndex),
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &blitRegion,
    };
    vkCmdBlitImage2(cmdBuf, &blitInfo);

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

    renderImGui(cmdBuf);
    endFrame(cmdBuf);
  }

  void Renderer::drawDeferred(VkCommandBuffer cmdBuf) {

    {
      std::array transitions{
          VkImageMemoryBarrier2{
              .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
              .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
              .srcAccessMask = 0,
              .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
              .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
              .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
              .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .image = m.renderTargets.gBuffer.albedo.image,
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
              .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
              .srcAccessMask = 0,
              .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
              .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
              .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
              .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .image = m.renderTargets.gBuffer.normal.image,
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
              .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
              .srcAccessMask = 0,
              .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
              .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
              .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
              .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .image = m.renderTargets.gBuffer.emissive.image,
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
              .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
              .srcAccessMask = 0,
              .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
              .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
              .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
              .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .image = m.renderTargets.gBuffer.metRough.image,
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
              .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
              .srcAccessMask = 0,
              .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
              .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
              .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
              .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
              .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .image = m.renderTargets.gBuffer.depth.image,
              .subresourceRange =
                  VkImageSubresourceRange{
                      .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
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
    }

    {
      std::array colorAttachments = {
          VkRenderingAttachmentInfo{
              .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
              .imageView = m.renderTargets.gBuffer.albedo.view,
              .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
              .clearValue =
                  VkClearValue{
                      .color = VkClearColorValue{.float32{0.0f, 0.0f, 0.0f, 1.0f}},
                  },
          },
          VkRenderingAttachmentInfo{
              .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
              .imageView = m.renderTargets.gBuffer.normal.view,
              .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
              .clearValue =
                  VkClearValue{
                      .color = VkClearColorValue{.float32{0.0f, 0.0f, 0.0f, 1.0f}},
                  },
          },
          VkRenderingAttachmentInfo{
              .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
              .imageView = m.renderTargets.gBuffer.emissive.view,
              .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
              .clearValue =
                  VkClearValue{
                      .color = VkClearColorValue{.float32{0.0f, 0.0f, 0.0f, 1.0f}},
                  },
          },
          VkRenderingAttachmentInfo{
              .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
              .imageView = m.renderTargets.gBuffer.metRough.view,
              .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
              .clearValue =
                  VkClearValue{
                      .color = VkClearColorValue{.float32{0.0f, 0.0f, 0.0f, 1.0f}},
                  },
          },
      };

      VkRenderingAttachmentInfo depthAttachment{
          .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
          .imageView = m.renderTargets.gBuffer.depth.view,
          .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
          .clearValue =
              VkClearValue{
                  .depthStencil = VkClearDepthStencilValue{.depth = 1.f, .stencil = 0},
              },
      };

      VkRenderingInfo renderingInfo{
          .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
          .renderArea =
              VkRect2D{
                  .offset = VkOffset2D{.x = 0, .y = 0},
                  .extent = VkExtent2D{.width = static_cast<uint32_t>(m.renderTargets.framebufferSize.x),
                                       .height = static_cast<uint32_t>(m.renderTargets.framebufferSize.y)},
              },
          .layerCount = 1,
          .colorAttachmentCount = colorAttachments.size(),
          .pColorAttachments = colorAttachments.data(),
          .pDepthAttachment = &depthAttachment,
      };
      vkCmdBeginRendering(cmdBuf, &renderingInfo);
    }

    {
      vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.deferred.pipeline);
      vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m.pipelines.deferred.layout, 0, 1,
                              &m.globalDescriptorSets.sets[m.frameInfo.index], 0, nullptr);
      VkRect2D scissor{
          .offset = VkOffset2D{.x = 0, .y = 0},
          .extent =
              VkExtent2D{
                  .width = static_cast<uint32_t>(m.renderTargets.framebufferSize.x),
                  .height = static_cast<uint32_t>(m.renderTargets.framebufferSize.y),
              },
      };
      VkViewport viewport{
          .x = 0.0f,
          .y = 0.0f,
          .width = static_cast<float>(m.renderTargets.framebufferSize.x),
          .height = static_cast<float>(m.renderTargets.framebufferSize.y),
          .minDepth = 0.0f,
          .maxDepth = 1.0f,
      };
      vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
      vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
    }

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

    {
      std::array backTransitions{
          VkImageMemoryBarrier2{
              .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
              .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
              .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
              .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
              .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
              .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .image = m.renderTargets.gBuffer.albedo.image,
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
              .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
              .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
              .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
              .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
              .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .image = m.renderTargets.gBuffer.normal.image,
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
              .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
              .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
              .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
              .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
              .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .image = m.renderTargets.gBuffer.emissive.image,
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
              .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
              .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
              .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
              .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
              .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .image = m.renderTargets.gBuffer.metRough.image,
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
              .srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
              .srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
              .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
              .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
              .oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
              .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
              .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
              .image = m.renderTargets.gBuffer.depth.image,
              .subresourceRange =
                  VkImageSubresourceRange{
                      .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                      .baseMipLevel = 0,
                      .levelCount = 1,
                      .baseArrayLayer = 0,
                      .layerCount = 1,
                  },
          },
      };

      VkDependencyInfo backDepInfo{
          .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
          .imageMemoryBarrierCount = backTransitions.size(),
          .pImageMemoryBarriers = backTransitions.data(),
      };
      vkCmdPipelineBarrier2(cmdBuf, &backDepInfo);
    }
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
