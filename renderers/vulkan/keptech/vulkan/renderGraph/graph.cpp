#include "graph.hpp"
#include "helpers/transitions.hpp"
#include "renderer.hpp"
#include <vector>

namespace kt::vkh {
  void RenderGraph::execute() {
    VK_ASSERT(renderer, "Renderer must be set before executing the render graph.");

    renderer->startFrame();
    auto& m = renderer->getMembers();

    auto cmds = m.frameInfo.perFrame->pools.graphics.allocate<2>(m.vkcore.device);

    {
      auto& cmdBuf = cmds[0];
      cmdBuf.label(m.vkcore.device, "RenderGraph::execute");
      cmdBuf.begin();
      for (auto& pass : passes) {
        VK_TRACE("Executing pass '{}'", pass.getName());
        pass.prepare(*this);

        pipelineBarrier(pass.getBarriers(), cmdBuf);

        switch (pass.getQueue()) {
        case QueueType::Graphics:
          executeGraphicsPass(pass, cmdBuf);
          break;
        case QueueType::Compute:
        case QueueType::AsyncCompute: // TODO: This needs a different CmdBuf and we need to submit with a semaphore increment
          executeComputePass(pass, cmdBuf);
          break;
        case QueueType::Cpu:
          pass.execute(cmdBuf);
          break;
        }
      }

      cmdBuf.end();

      VkCommandBufferSubmitInfo cmdBufInfo{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
          .commandBuffer = cmdBuf,
          .deviceMask = 0,
      };

      VkSubmitInfo2 submitInfo{
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
          .commandBufferInfoCount = 1,
          .pCommandBufferInfos = &cmdBufInfo,
      };

      VK_TRACE("Submitting render graph command buffer");
      vkQueueSubmit2(m.vkcore.queues.graphics.queue, 1, &submitInfo, nullptr);
    }

    {
      auto& cmdBuf = cmds[1];
      cmdBuf.label(m.vkcore.device, "RenderGraph::swapchain");
      cmdBuf.begin();
      auto backbuffer = getBackbufferImage();
      auto swapchainImg = m.vkcore.swapchain.nImage(m.frameInfo.imageIndex);

      layoutTransitions<2>(cmdBuf, {
                                       layoutTransition(backbuffer,
                                                        {
                                                            rendering::ImageType::Color,
                                                            rendering::ImageLayout::RenderTarget,
                                                            rendering::ImageLayout::TransferSrc,
                                                        }),
                                       layoutTransition(swapchainImg,
                                                        {
                                                            rendering::ImageType::Color,
                                                            rendering::ImageLayout::Undefined,
                                                            rendering::ImageLayout::TransferDst,
                                                        }),
                                   });

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
                  VkOffset3D{
                      .x = static_cast<int32_t>(backbuffer.extent().x),
                      .y = static_cast<int32_t>(backbuffer.extent().y),
                      .z = 1,
                  },
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
                  VkOffset3D{
                      .x = static_cast<int32_t>(m.vkcore.swapchain.config().extent.width),
                      .y = static_cast<int32_t>(m.vkcore.swapchain.config().extent.height),
                      .z = 1,
                  },
              },
      };
      VkBlitImageInfo2 blitInfo{
          .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
          .srcImage = backbuffer,
          .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          .dstImage = swapchainImg,
          .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .regionCount = 1,
          .pRegions = &blitRegion,
      };

      vkCmdBlitImage2(cmdBuf, &blitInfo);

      renderer->endFrame(cmdBuf);
    }
  }

  void RenderGraph::executeGraphicsPass(RenderPass& pass, CommandBuffer& cmd) {
    beginRendering(pass, cmd);

    pass.execute(cmd);

    cmd.endRendering();
  }

  void RenderGraph::executeComputePass(RenderPass& pass, CommandBuffer& cmd) { pass.execute(cmd); }

  void RenderGraph::pipelineBarrier(const Barriers& barriers, const CommandBuffer& cmd) const {
    std::vector<VkImageMemoryBarrier2> imageBarriers;
    imageBarriers.reserve(barriers.image.size());
    for (auto& barrier : barriers.image) {
      auto& img = images[barrier.resourceId];
      VkImageMemoryBarrier2 barrierInfo{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = barrier.srcStages,
          .srcAccessMask = barrier.srcAccess,
          .dstStageMask = barrier.dstStages,
          .dstAccessMask = barrier.dstAccess,
          .oldLayout = barrier.oldLayout,
          .newLayout = barrier.newLayout,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, // TODO: Queue Family handoff
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = img,
          .subresourceRange = img.getSubresourceRange(),
      };
    }
    std::vector<VkBufferMemoryBarrier2> bufferBarriers;
    bufferBarriers.reserve(barriers.buffer.size());
    for (auto& barrier : barriers.buffer) {
      auto& buf = buffers[barrier.resourceId];
      VkBufferMemoryBarrier2 barrierInfo{
          .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
          .srcStageMask = barrier.srcStages,
          .srcAccessMask = barrier.srcAccess,
          .dstStageMask = barrier.dstStages,
          .dstAccessMask = barrier.dstAccess,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, // TODO: Queue Family handoff
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .buffer = buf,
          .offset = 0,
          .size = buf.size(),
      };
    }

    VkDependencyInfo dependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = static_cast<uint32_t>(bufferBarriers.size()),
        .pBufferMemoryBarriers = bufferBarriers.data(),
        .imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size()),
        .pImageMemoryBarriers = imageBarriers.data(),
    };
    cmd.barrier(dependencyInfo);
  }

  void RenderGraph::beginRendering(const RenderPass& pass, const CommandBuffer& cmd) const {
    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    colorAttachments.reserve(pass.getColorAttachments().size());
    for (const auto& [idx, attachment] : pass.getColorAttachments() | std::views::enumerate) {
      auto& img = images[attachment.resourceId];
      VkClearColorValue clearValue{};
      pass.getClearColor(idx, &clearValue);
      VkRenderingAttachmentInfo attachmentInfo{
          .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
          .imageView = img,
          .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .loadOp = attachment.loadOp,
          .storeOp = attachment.storeOp,
          .clearValue = VkClearValue{.color = clearValue},
      };
      colorAttachments.push_back(attachmentInfo);
    }

    VkRenderingAttachmentInfo depthStencilAttachment{};
    VkRenderingAttachmentInfo* depthStencilAttachmentPtr = nullptr;
    auto ds = pass.getDepthStencilAttachment();
    if (ds.resourceId.used()) {
      auto& img = images[ds.resourceId];
      VkClearDepthStencilValue clearValue{};
      pass.getClearDepthStencil(&clearValue);
      depthStencilAttachment = VkRenderingAttachmentInfo{
          .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
          .imageView = img,
          .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
          .loadOp = ds.loadOp,
          .storeOp = ds.storeOp,
          .clearValue = VkClearValue{.depthStencil = clearValue},
      };
      depthStencilAttachmentPtr = &depthStencilAttachment;
    }

    VK_ASSERT(pass.getExtentSourceId().used(), "Pass '{}' does not have an extent source set", pass.getName());
    auto img = images[pass.getExtentSourceId()];

    VkRenderingInfo renderInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea =
            VkRect2D{
                .offset = VkOffset2D{.x = 0, .y = 0},
                .extent = {.width = img.extent().x, .height = img.extent().y},
            },
        .layerCount = img.layers(),
        .colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size()),
        .pColorAttachments = colorAttachments.data(),
        .pDepthAttachment = depthStencilAttachmentPtr,
    };

    cmd.beginRendering(renderInfo);
  }

  void RenderGraph::destroy() {
    VK_ASSERT(renderer, "Renderer must be set before destroying the render graph.");
    auto& m = renderer->getMembers();

    vkDeviceWaitIdle(m.vkcore.device);

    for (auto& img : images)
      img.destroy(m.vkcore.allocator, m.vkcore.device);
    for (auto& buf : buffers)
      buf.destroy(m.vkcore.allocator);
  }
} // namespace kt::vkh