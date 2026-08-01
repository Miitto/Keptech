#include "graph.hpp"
#include "helpers/transitions.hpp"
#include "interface.hpp"
#include "passInterface.hpp"
#include "profile.hpp"
#include "renderer.hpp"
#include "wrappers/bufferCreateInfo.hpp"
#include "wrappers/imageCreateInfo.hpp"
#include <vector>

namespace kt::rdr {
  void RenderGraph::execute() {
    KT_PROFILE_FUNCTION

    auto& renderer = Renderer::get();

    uint8_t frameIndex = renderer.getFrameIndex();

    imagesToDrop[frameIndex].clear();
    buffersToDrop[frameIndex].clear();

    updateDescriptors();

    engineCameraFrustum = renderer.startFrame();
    auto& m = renderer.getMembers();

    auto& sem = m.vkcore.mainSemaphore;
    VK_ASSERT(sem.semaphore != VK_NULL_HANDLE, "Timeline semaphore must be valid before executing the render graph.");

    auto graphicsCmds = m.frameInfo.perFrame->pools.graphics.allocate(
        m.vkcore.device,
        static_cast<uint32_t>(graphicsQueuePassCount + 1)); // +1 For the end of frame swapchain work
    auto computeCmds = m.frameInfo.perFrame->pools.compute.allocate(m.vkcore.device, static_cast<uint32_t>(computeQueuePassCount));

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
        {
          KT_VK_ZONE(m.tracyGraphicsContext, cmd, fmt::format("Graphics Pass Group {}", groupIdx));

          for (; passIndex < passEnd; ++passIndex) {
            auto& pass = passes[passIndex];
            VK_TRACE("Executing graphics pass '{}'", pass.getName());
            pass.prepare(renderer);

            pipelineBarrier(pass.getBarriers().pre, cmd);

            executeGraphicsPass(passIndex, pass, cmd);

            pipelineBarrier(pass.getBarriers().post, cmd);
          }
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

        {
          KT_VK_ZONE(m.tracyGraphicsContext, cmd, fmt::format("Compute Pass Group {}", groupIdx));

          for (; passIndex < passEnd; ++passIndex) {
            auto& pass = passes[passIndex];
            VK_TRACE("Executing compute pass '{}'", pass.getName());
            pass.prepare(renderer);

            pipelineBarrier(pass.getBarriers().pre, cmd);

            executeComputePass(passIndex, pass, cmd);

            pipelineBarrier(pass.getBarriers().post, cmd);
          }
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

        {
          KT_VK_ZONE(m.tracyComputeContext, cmd, fmt::format("Async Compute Pass Group {}", groupIdx));

          for (; passIndex < passEnd; ++passIndex) {
            auto& pass = passes[passIndex];
            VK_TRACE("Executing async compute pass '{}'", pass.getName());
            pass.prepare(renderer);

            pipelineBarrier(pass.getBarriers().pre, cmd);
            executeComputePass(passIndex, pass, cmd);
            pipelineBarrier(pass.getBarriers().post, cmd);
          }
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
                                                            ImageType::Color,
                                                            ImageLayout::RenderTarget,
                                                            ImageLayout::TransferSrc,
                                                        }),
                                       layoutTransition(swapchainImg,
                                                        {
                                                            ImageType::Color,
                                                            ImageLayout::Undefined,
                                                            ImageLayout::TransferDst,
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

      renderer.endFrame(cmdBuf);
    }
  }

  void RenderGraph::executeGraphicsPass(size_t passIdx, RenderPass& pass, CommandBuffer& cmd) {
    KT_PROFILE_FUNCTION
    if (pass.getAutoBeginRendering())
      beginRendering(pass, cmd);

    glm::uvec3 extent{};
    if (pass.getExtentSourceId().used())
      extent = resources.images[pass.getExtentSourceId()].extent();

    auto set = passDescriptors[passIdx].sets[Renderer::get().getMembers().frameInfo.index];

    pass.execute(cmd, set, extent);

    if (pass.getAutoBeginRendering())
      cmd.endRendering();
  }

  void RenderGraph::executeComputePass(size_t passIdx, RenderPass& pass, CommandBuffer& cmd) {
    KT_PROFILE_FUNCTION
    auto set = passDescriptors[passIdx].sets[Renderer::get().getMembers().frameInfo.index];
    pass.execute(cmd, set);
  }

  void RenderGraph::pipelineBarrier(const Barriers& barriers, const CommandBuffer& cmd) const {
    if (barriers.image.empty() && barriers.buffer.empty())
      return;
    KT_PROFILE_FUNCTION

    auto& renderer = Renderer::get();

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
        barrierInfo.srcQueueFamilyIndex = renderer.getMembers().vkcore.queues.graphics.index;
        barrierInfo.dstQueueFamilyIndex = renderer.getMembers().vkcore.queues.compute.index;
        break;
      case QueueHandoff::FromCompute:
        barrierInfo.srcQueueFamilyIndex = renderer.getMembers().vkcore.queues.compute.index;
        barrierInfo.dstQueueFamilyIndex = renderer.getMembers().vkcore.queues.graphics.index;
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
        barrierInfo.srcQueueFamilyIndex = renderer.getMembers().vkcore.queues.graphics.index;
        barrierInfo.dstQueueFamilyIndex = renderer.getMembers().vkcore.queues.compute.index;
        break;
      case QueueHandoff::FromCompute:
        barrierInfo.srcQueueFamilyIndex = renderer.getMembers().vkcore.queues.compute.index;
        barrierInfo.dstQueueFamilyIndex = renderer.getMembers().vkcore.queues.graphics.index;
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
    KT_PROFILE_FUNCTION
    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    colorAttachments.reserve(pass.getColorAttachments().size());
    for (const auto& [idx, attachment] : pass.getColorAttachments() | std::views::enumerate) {
      auto& img = resources.images[attachment.resourceId];
      VkClearColorValue clearValue{};
      pass.getClearColor(static_cast<uint32_t>(idx), &clearValue);
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
    auto& renderer = Renderer::get();
    auto& m = renderer.getMembers();

    vkDeviceWaitIdle(m.vkcore.device);

    for (auto& pass : passes) {
      pass.shutdown(renderer);
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

  void RenderGraph::setBackbufferSource(const std::string& name) {
    auto it = resources.nameToImage.find(name);
    VK_REQUIRE(it != resources.nameToImage.end(), "Backbuffer source '{}' not found in render graph", name);
    backbufferSourceIndex = it->second;
    VK_DEBUG("Backbuffer source set to '{}' (index {})", name, backbufferSourceIndex);
  }

  [[nodiscard]] const Image& RenderGraph::getBackbufferImage() const {
    VK_REQUIRE(backbufferSourceIndex < resources.images.size(), "Backbuffer source index {} is out of bounds (size: {})",
               backbufferSourceIndex, resources.images.size());
    return resources.images[backbufferSourceIndex];
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

  RenderGraph::RenderGraph(std::vector<PassGroup>&& passGroups, std::vector<RenderPass>&& passes, Resources&& resources,
                           VkDescriptorPool descriptorPool, std::vector<Descriptors>&& descriptors)
      : passGroups(std::move(passGroups)), passes(std::move(passes)), resources(std::move(resources)), descriptorPool(descriptorPool),
        passDescriptors(std::move(descriptors)) {
    for (const auto& group : this->passGroups) {
      if (group.queue == QueueType::Graphics) {
        graphicsQueuePassCount += group.count;
      } else if (group.queue == QueueType::AsyncCompute) {
        computeQueuePassCount += group.count;
      } // Shouldn't be any normal compute queue groups as they have been compacted into the graphics queue groups.
    }

    for (auto& pass : this->passes) {
      pass.setGraph(*this);
    }
  }

  [[nodiscard]] const std::vector<RenderPass>& RenderGraph::getPasses() const { return passes; }
  [[nodiscard]] const std::vector<bool>& RenderGraph::getPhysicalImageHasHistory() const { return resources.physicalImageHasHistory; }
  [[nodiscard]] size_t RenderGraph::getImageIndex(const std::string& name) const {
    auto it = resources.nameToImage.find(name);
    VK_ASSERT(it != resources.nameToImage.end(), "Image resource with name '{}' not found in render graph", name);
    return it->second;
  }
  [[nodiscard]] size_t RenderGraph::getBufferIndex(const std::string& name) const {
    auto it = resources.nameToBuffer.find(name);
    VK_ASSERT(it != resources.nameToBuffer.end(), "Buffer resource with name '{}' not found in render graph", name);
    return it->second;
  }
  [[nodiscard]] const Image& RenderGraph::getImage(size_t index) const {
    VK_ASSERT(index < resources.images.size(), "Image index {} is out of bounds (size: {})", index, resources.images.size());
    return resources.images[index];
  }
  [[nodiscard]] const Buffer& RenderGraph::getBuffer(size_t index) const {
    VK_ASSERT(index < resources.buffers.size(), "Buffer index {} is out of bounds (size: {})", index, resources.buffers.size());
    return resources.buffers[index];
  }
  const Buffer& RenderGraph::getFrameBuffer(size_t index) const {
    VK_ASSERT(index < resources.buffers.size(), "Buffer index {} is out of bounds (size: {})", index, resources.buffers.size());
    VK_ASSERT(resources.buffers[index].isMapped(), "Called getFrameBuffer on a buffer that is not per-frame.");
    auto& buf = resources.buffers[index + Renderer::get().getFrameIndex()];
    return buf;
  }

  const Buffer& RenderGraph::reallocateBuffer(size_t index, size_t newSize, bool copyOldData) {
    VK_ASSERT(index < resources.buffers.size(), "Buffer index {} is out of bounds (size: {})", index, resources.buffers.size());
    auto& buf = resources.buffers[index];
    auto newBufRes = Buffer::create({
        newSize,
        buf.getUsage(),
        buf.getAllocationFlags(),
        VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO,
        buf.getName().c_str(),
    });
    if (!newBufRes) {
      VK_ABORT("Failed to reallocate buffer '{}': {}", buf.getName(), newBufRes.error());
    }

    if (copyOldData) {
      VK_ASSERT(buf.isMapped(),
                "Cannot copy old data from buffer '{}' because it is not mapped. You will need to explicitly manage the data transfer.",
                buf.getName());
      std::memcpy(newBufRes.value().mapping(), buf.mapping(), std::min(buf.size(), newSize));
    }

    buffersToDrop[Renderer::get().getLastFrameIndex()].push_back(std::move(buf));
    resources.buffers[index] = std::move(newBufRes.value());
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      buffersToUpdate[i].push_back(index);
    }
    updateDescriptors();

    return buffersToDrop[Renderer::get().getLastFrameIndex()].back();
  }

  const Buffer& RenderGraph::reallocatePerFrameBuffer(size_t gindex, size_t newSize, bool copyOldData) {
    VK_ASSERT(gindex < resources.buffers.size(), "Buffer index {} is out of bounds (size: {})", gindex, resources.buffers.size());
    VK_REQUIRE(resources.buffers[gindex].isMapped(), "Buffer at index {} is not a per-frame buffer.", gindex);

    size_t index = gindex + Renderer::get().getFrameIndex();

    auto& buf = resources.buffers[index];

    auto newBufRes = Buffer::create({
        newSize,
        buf.getUsage(),
        buf.getAllocationFlags(),
        VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO,
        buf.getName().c_str(),
    });
    if (!newBufRes) {
      VK_ABORT("Failed to reallocate per-frame buffer '{}': {}", buf.getName(), newBufRes.error());
    }

    if (copyOldData) {
      std::memcpy(newBufRes.value().mapping(), buf.mapping(), std::min(buf.size(), newSize));
    }

    buffersToDrop[Renderer::get().getLastFrameIndex()].push_back(std::move(buf));
    resources.buffers[index] = std::move(newBufRes.value());
    buffersToUpdate[Renderer::get().getFrameIndex()].push_back(index);

    updateDescriptors();

    return buffersToDrop[Renderer::get().getLastFrameIndex()].back();
  }

  VkImageLayout RenderPass::getDepthStencilLayout() const { return depthStencilLayout; }

  void RenderPass::setDepthStencilLayout(VkImageLayout layout) { depthStencilLayout = layout; }

  PhysResourceId RenderPass::getExtentSourceId() const { return extentSourceId; }

  void RenderPass::setExtentSourceId(PhysResourceId id) { extentSourceId = id; }

  const RenderAttachment& RenderPass::getDepthStencilAttachment() const { return depthStencilAttachment; }
  void RenderPass::setGraph(RenderGraph& g) { graph = &g; }
  void RenderPass::setInterface(RenderPassInterface* i) { interface = i; }
  void RenderPass::setBuildCallback(PassExecuteCb&& cb) { buildCb = std::move(cb); }
  void RenderPass::setGetClearDepthStencilCallback(std::function<bool(VkClearDepthStencilValue*)>&& cb) {
    getClearDepthStencilCb = std::move(cb);
  }
  void RenderPass::setGetClearColorCallback(std::function<bool(uint32_t, VkClearColorValue*)>&& cb) { getClearColorCb = std::move(cb); }
  void RenderPass::setup(Renderer& renderer, VkDescriptorSetLayout descriptorSetLayout) {
    if (interface)
      interface->setup(*graph, renderer, descriptorSetLayout);
  }
  void RenderPass::prepare(Renderer& renderer) {
    if (interface)
      interface->prepare(*graph, renderer);
  }
  void RenderPass::execute(const CommandBuffer& cmd, VkDescriptorSet descriptorSet, glm::uvec3 framebufferSize) {
    if (interface) {
      interface->execute(*graph, cmd, descriptorSet, framebufferSize);
    } else if (buildCb) {
      buildCb(cmd, descriptorSet, framebufferSize);
    }
  }
  void RenderPass::shutdown(Renderer& renderer) {
    if (interface)
      interface->shutdown(*graph, renderer);
  }
  bool RenderPass::getClearColor(uint32_t attachmentIndex, VkClearColorValue* value) const {
    if (interface)
      return interface->getClearColor(attachmentIndex, value);
    else if (getClearColorCb)
      return getClearColorCb(attachmentIndex, value);

    return false;
  }
  bool RenderPass::getClearDepthStencil(VkClearDepthStencilValue* value) const {
    if (interface)
      return interface->getClearDepthStencil(value);
    else if (getClearDepthStencilCb)
      return getClearDepthStencilCb(value);

    return false;
  }
  [[nodiscard]] const PrePostBarriers& RenderPass::getBarriers() const { return barriers; }
  [[nodiscard]] const std::string& RenderPass::getName() const { return name; }
  [[nodiscard]] QueueType RenderPass::getQueue() const { return queue; }
  [[nodiscard]] const std::vector<RenderAttachment>& RenderPass::getColorAttachments() const { return colorAttachments; }

  void RenderGraph::onResolutionChanged(const glm::uvec2& newResolution) {
    KT_PROFILE_FUNCTION

    VK_TRACE("RenderGraph::onResolutionChanged() - New Resolution: {}x{}", newResolution.x, newResolution.y);

    for (auto& resImage : resources.resolutionRelativeImages) {
      auto& img = resources.images[resImage.index];

      glm::uvec3 newExtent = {static_cast<float>(newResolution.x) * resImage.ratio.x,
                              static_cast<float>(newResolution.y) * resImage.ratio.y, 1};

      VkExtent3D extent3D{.width = newExtent.x, .height = newExtent.y, .depth = 1};

      auto newImageRes =
          Image::create({VK_IMAGE_TYPE_2D, img.format(), extent3D, img.getUsage(), img.mips(), img.layers(), img.getName().c_str()});
      if (!newImageRes) {
        VK_ABORT("Failed to create resolution relative image '{}': {}", img.getName(), newImageRes.error());
      }

      imagesToDrop[Renderer::get().getLastFrameIndex()].push_back(std::move(img));
      resources.images[resImage.index] = std::move(newImageRes.value());

      for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        imagesToUpdate[i].push_back(resImage.index);
      }
    }
  }

  void RenderGraph::onSwapchainSizeChanged(const glm::uvec2& newSize) {
    KT_PROFILE_FUNCTION

    VK_TRACE("RenderGraph::onSwapchainSizeChanged() - New Size: {}x{}", newSize.x, newSize.y);

    for (auto& resImage : resources.swapchainRelativeImages) {
      auto& img = resources.images[resImage.index];

      glm::uvec3 newExtent = {static_cast<float>(newSize.x) * resImage.ratio.x, static_cast<float>(newSize.y) * resImage.ratio.y, 1};

      VkExtent3D extent3D{.width = newExtent.x, .height = newExtent.y, .depth = 1};

      auto newImageRes =
          Image::create({VK_IMAGE_TYPE_2D, img.format(), extent3D, img.getUsage(), img.mips(), img.layers(), img.getName().c_str()});
      if (!newImageRes) {
        VK_ABORT("Failed to create swapchain relative image '{}': {}", img.getName(), newImageRes.error());
      }

      imagesToDrop[Renderer::get().getLastFrameIndex()].push_back(std::move(img));
      resources.images[resImage.index] = std::move(newImageRes.value());

      for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        imagesToUpdate[i].push_back(resImage.index);
      }
    }
  }

  void RenderGraph::updateDescriptors() {
    KT_PROFILE_FUNCTION
    auto& r = Renderer::get();
    uint8_t frameIndex = r.getFrameIndex();

    std::vector<VkDescriptorImageInfo> imageInfos;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;

    // Precalc so that the vector doesnt move and invalidate pointers
    size_t imgCount = 0;
    size_t bufCount = 0;
    size_t writeCount = 0;

    for (auto imgIdx : imagesToUpdate[frameIndex]) {
      auto& used = resources.imageUsedInPass[imgIdx];

      imgCount += used.size();
      writeCount += used.size();
    }

    for (auto bufIdx : buffersToUpdate[frameIndex]) {
      auto& used = resources.bufferUsedInPass[bufIdx];

      bufCount += used.size();
      writeCount += used.size();
    }

    if (writeCount == 0)
      return;

    imageInfos.reserve(imgCount);
    bufferInfos.reserve(bufCount);
    writes.reserve(writeCount);

    for (auto imgIdx : imagesToUpdate[frameIndex]) {
      auto& img = resources.images[imgIdx];
      auto& used = resources.imageUsedInPass[imgIdx];

      for (auto& u : used) {
        VkDescriptorImageInfo imageInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = img,
            .imageLayout = u.layout,
        };
        imageInfos.push_back(imageInfo);
        VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = passDescriptors[u.passIndex].sets[frameIndex],
            .dstBinding = u.binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = u.descriptorType,
            .pImageInfo = &imageInfos.back(),
        };
        writes.push_back(write);
      }
    }

    for (auto bufIdx : buffersToUpdate[frameIndex]) {
      auto& buf = resources.buffers[bufIdx];
      auto& used = resources.bufferUsedInPass[bufIdx];

      for (auto& u : used) {
        VkDescriptorBufferInfo bufferInfo{
            .buffer = buf,
            .offset = 0,
            .range = buf.size(),
        };
        bufferInfos.push_back(bufferInfo);
        VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = passDescriptors[u.passIndex].sets[frameIndex],
            .dstBinding = u.binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = u.descriptorType,
            .pBufferInfo = &bufferInfos.back(),
        };
        writes.push_back(write);
      }
    }

    vkUpdateDescriptorSets(r.getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    imagesToUpdate[frameIndex].clear();
    buffersToUpdate[frameIndex].clear();
  }
} // namespace kt::rdr