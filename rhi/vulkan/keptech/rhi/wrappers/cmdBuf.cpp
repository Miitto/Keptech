#include "cmdBuf.hpp"

#include "keptech/rhi/macros.hpp"
#include "keptech/rhi/rhi.hpp"

namespace kt::rhi {

  const CommandBuffer& CommandBuffer::begin() const {
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(cmdBuf, &beginInfo), "Failed to begin command buffer");
    return *this;
  }

  const CommandBuffer& CommandBuffer::barrier(const VkDependencyInfo& dependencyInfo) const {
    vkCmdPipelineBarrier2(cmdBuf, &dependencyInfo);
    return *this;
  }
  const CommandBuffer& CommandBuffer::bindPipeline(VkPipeline pipeline) const {
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    RHI::get().registerPipelineSwitch();
    return *this;
  }
  const CommandBuffer& CommandBuffer::bindComputePipeline(VkPipeline pipeline) const {
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    RHI::get().registerPipelineSwitch();
    return *this;
  }

  const CommandBuffer& CommandBuffer::setViewportScissor(glm::uvec2 framebufferSize, glm::vec2 minMaxDepth, glm::vec2 offset) const {
    VkViewport viewport{
        .x = offset.x,
        .y = offset.y,
        .width = static_cast<float>(framebufferSize.x),
        .height = static_cast<float>(framebufferSize.y),
        .minDepth = minMaxDepth.x,
        .maxDepth = minMaxDepth.y,
    };
    vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

    VkRect2D scissor{
        .offset = {.x = static_cast<int32_t>(offset.x), .y = static_cast<int32_t>(offset.y)},
        .extent = {.width = framebufferSize.x, .height = framebufferSize.y},
    };
    vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
    return *this;
  }

  const CommandBuffer& CommandBuffer::bindDescriptorSets(VkPipelineLayout layout, VkPipelineBindPoint bindPoint, uint32_t firstSet,
                                                         std::span<const VkDescriptorSet> descriptorSets,
                                                         std::span<const uint32_t> dynamicOffsets) const {
    vkCmdBindDescriptorSets(cmdBuf, bindPoint, layout, firstSet, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(),
                            static_cast<uint32_t>(dynamicOffsets.size()), dynamicOffsets.data());
    return *this;
  }

  const CommandBuffer& CommandBuffer::bindVertexBuffers(std::span<const VkBuffer> buffers, std::span<const VkDeviceSize> offsets,
                                                        uint32_t first) const {
    vkCmdBindVertexBuffers(cmdBuf, first, static_cast<uint32_t>(buffers.size()), buffers.data(), offsets.data());
    return *this;
  }

  const CommandBuffer& CommandBuffer::bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) const {
    vkCmdBindIndexBuffer(cmdBuf, buffer, offset, indexType);
    return *this;
  }

  const CommandBuffer& CommandBuffer::bindRendererVertexBuffers() const {
    auto& r = RHI::get();
    constexpr std::array<VkDeviceSize, 2> offsets = {0, 0};
    vkCmdBindVertexBuffers(cmdBuf, 0, 2, r.getVertexBuffers().data(), offsets.data());
    return *this;
  }

  const CommandBuffer& CommandBuffer::bindRendererVertexIndexBuffers() const {
    auto& r = RHI::get();
    constexpr std::array<VkDeviceSize, 2> offsets = {0, 0};
    vkCmdBindVertexBuffers(cmdBuf, 0, 2, r.getVertexBuffers().data(), offsets.data());
    vkCmdBindIndexBuffer(cmdBuf, r.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
    return *this;
  }

  const CommandBuffer& CommandBuffer::pushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size,
                                                    const void* pValues) const {
    vkCmdPushConstants(cmdBuf, layout, stageFlags, offset, size, pValues);
    return *this;
  }

  const CommandBuffer& CommandBuffer::beginRendering(const VkRenderingInfo& renderingInfo) const {
    vkCmdBeginRendering(cmdBuf, &renderingInfo);
    RHI::get().registerRenderPass();
    return *this;
  }

  const CommandBuffer& CommandBuffer::draw(uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount,
                                           uint32_t firstInstance) const {
    vkCmdDraw(cmdBuf, vertexCount, instanceCount, firstVertex, firstInstance);
    RHI::get().registerDrawCall(static_cast<size_t>(vertexCount), static_cast<size_t>(vertexCount) / 3);
    return *this;
  }
  const CommandBuffer& CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t instanceCount,
                                                  uint32_t firstInstance) const {
    vkCmdDrawIndexed(cmdBuf, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    RHI::get().registerDrawCall(indexCount, static_cast<size_t>(indexCount) / 3);
    return *this;
  }
  const CommandBuffer& CommandBuffer::drawIndirect(VkBuffer buffer, uint32_t drawCount, VkDeviceSize offset, uint32_t stride) const {
    vkCmdDrawIndirect(cmdBuf, buffer, offset, drawCount, stride);
    return *this;
  }
  const CommandBuffer& CommandBuffer::drawIndirectCount(VkBuffer buffer, VkBuffer countBuffer, uint32_t drawCount, VkDeviceSize offset,
                                                        VkDeviceSize countBufferOffset, uint32_t stride) const {
    vkCmdDrawIndirectCount(cmdBuf, buffer, offset, countBuffer, countBufferOffset, drawCount, stride);
    return *this;
  }
  const CommandBuffer& CommandBuffer::drawIndexedIndirect(VkBuffer buffer, uint32_t drawCount, VkDeviceSize offset, uint32_t stride) const {
    vkCmdDrawIndexedIndirect(cmdBuf, buffer, offset, drawCount, stride);
    return *this;
  }
  const CommandBuffer& CommandBuffer::drawIndexedIndirectCount(VkBuffer buffer, VkBuffer countBuffer, uint32_t drawCount,
                                                               VkDeviceSize offset, VkDeviceSize countBufferOffset, uint32_t stride) const {
    vkCmdDrawIndexedIndirectCount(cmdBuf, buffer, offset, countBuffer, countBufferOffset, drawCount, stride);
    return *this;
  }
  const CommandBuffer& CommandBuffer::drawMeshTasks(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const {
    vkCmdDrawMeshTasksEXT(cmdBuf, groupCountX, groupCountY, groupCountZ);
    return *this;
  }

  const CommandBuffer& CommandBuffer::endRendering() const {
    vkCmdEndRendering(cmdBuf);
    return *this;
  }
  const CommandBuffer& CommandBuffer::end() const {
    VK_CHECK(vkEndCommandBuffer(cmdBuf), "Failed to end command buffer");
    return *this;
  }

  const CommandBuffer& CommandBuffer::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const {
    vkCmdDispatch(cmdBuf, groupCountX, groupCountY, groupCountZ);
    RHI::get().registerComputeDispatch();
    return *this;
  }

  const CommandBuffer& CommandBuffer::dispatchIndirect(VkBuffer buffer, VkDeviceSize offset) const {
    vkCmdDispatchIndirect(cmdBuf, buffer, offset);
    RHI::get().registerComputeDispatch();
    return *this;
  }

  [[nodiscard]] CommandBuffer::operator VkCommandBuffer() const { return cmdBuf; }
  [[nodiscard]] VkCommandBuffer CommandBuffer::get() const { return cmdBuf; }
  [[nodiscard]] VkCommandBuffer CommandBuffer::operator*() const { return cmdBuf; }

  const CommandBuffer& CommandBuffer::label(const VkDevice device, const std::string& name) const {
    label(device, name.c_str());
    return *this;
  }
  const CommandBuffer& CommandBuffer::label(const VkDevice device, const char* name) const {
#ifndef NDEBUG
    VkDebugUtilsObjectNameInfoEXT nameInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_COMMAND_BUFFER,
        .objectHandle = reinterpret_cast<uint64_t>(cmdBuf),
        .pObjectName = name,
    };
    vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
#endif
    return *this;
  }

} // namespace kt::rhi