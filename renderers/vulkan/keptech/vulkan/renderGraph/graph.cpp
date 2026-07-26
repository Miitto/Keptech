#include "graph.hpp"
#include "helpers/transitions.hpp"
#include "interface.hpp"
#include "renderer.hpp"
#include <vector>

namespace kt::vkh {
  void RenderGraph::execute() {
    VK_ASSERT(renderer, "Renderer must be set before executing the render graph.");

    renderer->startFrame();
    auto& m = renderer->getMembers();

    auto& perFrame = *m.frameInfo.perFrame;
    auto& sem = m.vkcore.mainSemaphore;
    VK_ASSERT(sem.semaphore != VK_NULL_HANDLE, "Timeline semaphore must be valid before executing the render graph.");

    auto graphicsCmds = m.frameInfo.perFrame->pools.graphics.allocate(m.vkcore.device,
                                                                      graphicsQueuePassCount + 1); // +1 For the end of frame swapchain work
    auto computeCmds = m.frameInfo.perFrame->pools.compute.allocate(m.vkcore.device, computeQueuePassCount);

    size_t passIndex = 0;
    size_t graphicsPassIndex = 0;
    size_t computePassIndex = 0;
    for (const auto& [groupIdx, group] : passGroups | std::views::enumerate) {
      size_t passEnd = passIndex + group.count;

      VkCommandBuffer vkCmd = nullptr;
      VkQueue queue = nullptr;

      switch (group.queue) {
      case QueueType::Graphics: {
        VK_TRACE("Executing graphics pass group {} on graphics queue with {} passes", groupIdx, group.count);

        auto& cmd = graphicsCmds[graphicsPassIndex++];
        vkCmd = cmd;
        queue = m.vkcore.queues.graphics.queue;
        cmd.label(m.vkcore.device, fmt::format("RenderGraph::execute() - Pass Group {}: Graphics Queue", groupIdx));

        cmd.begin();

        for (; passIndex < passEnd; ++passIndex) {
          auto& pass = passes[passIndex];
          VK_TRACE("Executing graphics pass '{}'", pass.getName());
          pass.prepare(*renderer);

          pipelineBarrier(pass.getBarriers().pre, cmd);

          executeGraphicsPass(passIndex, pass, cmd);

          pipelineBarrier(pass.getBarriers().post, cmd);
        }

        cmd.end();

      } break;
      case QueueType::Compute: {
        VK_TRACE("Executing compute pass group {} on graphics queue with {} passes", groupIdx, group.count);

        auto& cmd = graphicsCmds[graphicsPassIndex++];
        vkCmd = cmd;
        queue = m.vkcore.queues.graphics.queue;
        cmd.label(m.vkcore.device, fmt::format("RenderGraph::execute() - Pass Group {}: Compute Queue", groupIdx));

        cmd.begin();

        for (; passIndex < passEnd; ++passIndex) {
          auto& pass = passes[passIndex];
          VK_TRACE("Executing compute pass '{}'", pass.getName());
          pass.prepare(*renderer);

          pipelineBarrier(pass.getBarriers().pre, cmd);

          executeComputePass(passIndex, pass, cmd);

          pipelineBarrier(pass.getBarriers().post, cmd);
        }

        cmd.end();
      } break;
      case QueueType::AsyncCompute: {
        VK_TRACE("Executing async compute pass group {} on compute queue with {} passes", groupIdx, group.count);
        auto& cmd = computeCmds[computePassIndex++];
        vkCmd = cmd;
        queue = m.vkcore.queues.compute.queue;

        cmd.label(m.vkcore.device, fmt::format("RenderGraph::execute() - Pass Group {}: Async Compute Queue", groupIdx));
        cmd.begin();

        for (; passIndex < passEnd; ++passIndex) {
          auto& pass = passes[passIndex];
          VK_TRACE("Executing async compute pass '{}'", pass.getName());
          pass.prepare(*renderer);

          pipelineBarrier(pass.getBarriers().pre, cmd);
          executeComputePass(passIndex, pass, cmd);
          pipelineBarrier(pass.getBarriers().post, cmd);
        }

        cmd.end();
      }; break;
      }

      uint64_t signalValue = sem.value + static_cast<uint64_t>(groupIdx + 1);
      uint64_t waitValue = group.waitFor == ~0ull
                               ? 0
                               : sem.value + static_cast<uint64_t>(group.waitFor) +
                                     1; // Need add 1 since the signalled value for a pass group will be the pass group index + 1.

      VK_ASSERT(waitValue < signalValue, "Wait value must be less than signal value for timeline semaphore.");

      VkSemaphoreSubmitInfo timelineSemInfo{
          .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
          .semaphore = sem.semaphore,
          .value = signalValue,
          .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      };

      VK_TRACE("Submitting render graph command buffer for pass group {} on queue {}. Waiting for {} then signalling {}", groupIdx,
               static_cast<int>(group.queue), waitValue, signalValue);

      VkSemaphoreSubmitInfo waitSemInfo{
          .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
          .semaphore = sem.semaphore,
          .value = waitValue,
          .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      };

      VkCommandBufferSubmitInfo cmdBufInfo{
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
          .commandBuffer = vkCmd,
          .deviceMask = 0,
      };

      VkSubmitInfo2 submitInfo{
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
          .waitSemaphoreInfoCount = 1,
          .pWaitSemaphoreInfos = &waitSemInfo,
          .commandBufferInfoCount = 1,
          .pCommandBufferInfos = &cmdBufInfo,
          .signalSemaphoreInfoCount = 1,
          .pSignalSemaphoreInfos = &timelineSemInfo,
      };

      vkQueueSubmit2(queue, 1, &submitInfo, nullptr);
    }

    sem.value += static_cast<uint64_t>(passGroups.size());

    {
      // TODO: Swapchain resource may not end up as a color attachment, or even on the grapics queue.
      auto& cmdBuf = graphicsCmds.back();
      cmdBuf.label(m.vkcore.device, "RenderGraph::swapchain");
      cmdBuf.begin();
      auto& backbuffer = getBackbufferImage();
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

      // TODO: Attempt to alias the swapchain image with the backbuffer source to possibly remove this blit.
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

  void RenderGraph::executeGraphicsPass(size_t passIdx, RenderPass& pass, CommandBuffer& cmd) {
    beginRendering(pass, cmd);

    auto& img = resources.images[pass.getExtentSourceId()];
    auto set = passDescriptors[passIdx].sets[renderer->getMembers().frameInfo.index];

    pass.execute(cmd, set, img.extent());

    cmd.endRendering();
  }

  void RenderGraph::executeComputePass(size_t passIdx, RenderPass& pass, CommandBuffer& cmd) {
    auto set = passDescriptors[passIdx].sets[renderer->getMembers().frameInfo.index];
    pass.execute(cmd, set);
  }

  void RenderGraph::pipelineBarrier(const Barriers& barriers, const CommandBuffer& cmd) const {
    if (barriers.image.empty() && barriers.buffer.empty())
      return;

    std::vector<VkImageMemoryBarrier2> imageBarriers;
    imageBarriers.reserve(barriers.image.size());
    for (auto& barrier : barriers.image) {
      auto& img = resources.images[barrier.resourceId];
      VkImageMemoryBarrier2 barrierInfo{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
          .srcStageMask = barrier.srcStages,
          .srcAccessMask = barrier.srcAccess,
          .dstStageMask = barrier.dstStages,
          .dstAccessMask = barrier.dstAccess,
          .oldLayout = barrier.oldLayout,
          .newLayout = barrier.newLayout,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .image = img,
          .subresourceRange = img.getSubresourceRange(),
      };

      switch (barrier.handoff) {
      case QueueHandoff::No:
        break;
      case QueueHandoff::ToCompute:
        barrierInfo.srcQueueFamilyIndex = renderer->getMembers().vkcore.queues.graphics.index;
        barrierInfo.dstQueueFamilyIndex = renderer->getMembers().vkcore.queues.compute.index;
        break;
      case QueueHandoff::FromCompute:
        barrierInfo.srcQueueFamilyIndex = renderer->getMembers().vkcore.queues.compute.index;
        barrierInfo.dstQueueFamilyIndex = renderer->getMembers().vkcore.queues.graphics.index;
        break;
      }
      imageBarriers.push_back(barrierInfo);
    }
    std::vector<VkBufferMemoryBarrier2> bufferBarriers;
    bufferBarriers.reserve(barriers.buffer.size());
    for (auto& barrier : barriers.buffer) {
      auto& buf = resources.buffers[barrier.resourceId];
      VkBufferMemoryBarrier2 barrierInfo{
          .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
          .srcStageMask = barrier.srcStages,
          .srcAccessMask = barrier.srcAccess,
          .dstStageMask = barrier.dstStages,
          .dstAccessMask = barrier.dstAccess,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .buffer = buf,
          .offset = 0,
          .size = buf.size(),
      };

      switch (barrier.handoff) {
      case QueueHandoff::No:
        break;
      case QueueHandoff::ToCompute:
        barrierInfo.srcQueueFamilyIndex = renderer->getMembers().vkcore.queues.graphics.index;
        barrierInfo.dstQueueFamilyIndex = renderer->getMembers().vkcore.queues.compute.index;
        break;
      case QueueHandoff::FromCompute:
        barrierInfo.srcQueueFamilyIndex = renderer->getMembers().vkcore.queues.compute.index;
        barrierInfo.dstQueueFamilyIndex = renderer->getMembers().vkcore.queues.graphics.index;
        break;
      }
      bufferBarriers.push_back(barrierInfo);
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
      auto& img = resources.images[attachment.resourceId];
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
      VK_ASSERT(pass.getDepthStencilLayout() != VK_IMAGE_LAYOUT_UNDEFINED, "Pass '{}' has a depth-stencil attachment but no layout set",
                pass.getName());
      auto& img = resources.images[ds.resourceId];
      VkClearDepthStencilValue clearValue{};
      pass.getClearDepthStencil(&clearValue);
      depthStencilAttachment = VkRenderingAttachmentInfo{
          .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
          .imageView = img,
          .imageLayout = pass.getDepthStencilLayout(),
          .loadOp = ds.loadOp,
          .storeOp = ds.storeOp,
          .clearValue = VkClearValue{.depthStencil = clearValue},
      };
      depthStencilAttachmentPtr = &depthStencilAttachment;
    }

    VK_ASSERT(pass.getExtentSourceId().used(), "Pass '{}' does not have an extent source set", pass.getName());
    auto& img = resources.images[pass.getExtentSourceId()];

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

    for (auto& pass : passes) {
      pass.shutdown(*renderer);
    }

    // Just incase the passes have enqueued work on the device that needs to be completed before destroying resources.
    vkDeviceWaitIdle(m.vkcore.device);

    for (auto& d : passDescriptors) {
      vkDestroyDescriptorSetLayout(m.vkcore.device, d.layout, nullptr);
    }
    vkDestroyDescriptorPool(m.vkcore.device, descriptorPool, nullptr);

    for (auto& img : resources.images)
      img.destroy();
    for (auto& buf : resources.buffers)
      buf.destroy();
  }

  void RenderGraph::log() const {
#define LOG(...) VK_DEBUG(__VA_ARGS__) // NOLINT

    LOG("RenderGraph:");
    LOG("  Pass Groups: {}", passGroups.size());
    for (const auto& [idx, group] : passGroups | std::views::enumerate) {
      LOG("    {}: Queue: {}, Count: {}, WaitFor: {}", idx, group.queue, group.count,
          group.waitFor == ~0ull ? "None" : std::to_string(group.waitFor));
    }
    LOG("");
  }
} // namespace kt::vkh