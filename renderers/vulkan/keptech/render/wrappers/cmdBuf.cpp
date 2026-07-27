#include "cmdBuf.hpp"

#include "keptech/render/macros.hpp"

namespace kt::vkh {

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
    return *this;
  }
  const CommandBuffer& CommandBuffer::bindComputePipeline(VkPipeline pipeline) const {
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
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

  const CommandBuffer& CommandBuffer::beginRendering(const VkRenderingInfo& renderingInfo) const {
    vkCmdBeginRendering(cmdBuf, &renderingInfo);
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

} // namespace kt::vkh