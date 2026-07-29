#pragma once

#include <Volk/volk.h>
#include <span>

namespace kt::rdr {
  class CommandBuffer {
  public:
    CommandBuffer() = default;
    CommandBuffer(VkCommandBuffer cmdBuf) : cmdBuf(cmdBuf) {}

    const CommandBuffer& begin() const;

    const CommandBuffer& barrier(const VkDependencyInfo& dependencyInfo) const;
    const CommandBuffer& bindPipeline(VkPipeline pipeline) const;
    const CommandBuffer& bindComputePipeline(VkPipeline pipeline) const;

    const CommandBuffer& setViewportScissor(glm::uvec2 framebufferSize, glm::vec2 minMaxDepth = {0.f, 1.f},
                                            glm::vec2 offset = {0.f, 0.f}) const;

    const CommandBuffer& bindDescriptorSets(VkPipelineLayout layout, VkPipelineBindPoint bindPoint, uint32_t firstSet,
                                            std::span<const VkDescriptorSet> descriptorSets,
                                            std::span<const uint32_t> dynamicOffsets = {}) const;
    const CommandBuffer& bindVertexBuffers(std::span<const VkBuffer> buffers, std::span<const VkDeviceSize> offsets,
                                           uint32_t first = 0) const;
    const CommandBuffer& bindRendererVertexBuffers() const;
    const CommandBuffer& bindRendererVertexIndexBuffers() const;
    const CommandBuffer& bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) const;
    const CommandBuffer& pushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size,
                                       const void* pValues) const;
    template <typename T>
    const CommandBuffer& pushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, const T& value) const {
      return pushConstants(layout, stageFlags, offset, sizeof(T), &value);
    }

    const CommandBuffer& beginRendering(const VkRenderingInfo& renderingInfo) const;

    const CommandBuffer& draw(uint32_t vertexCount, uint32_t firstVertex = 0, uint32_t instanceCount = 1, uint32_t firstInstance = 0) const;
    const CommandBuffer& drawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t instanceCount = 1,
                                     uint32_t firstInstance = 0) const;
    const CommandBuffer& drawIndirect(VkBuffer buffer, uint32_t drawCount, VkDeviceSize offset = 0,
                                      uint32_t stride = sizeof(VkDrawIndirectCommand)) const;
    const CommandBuffer& drawIndirectCount(VkBuffer buffer, VkBuffer countBuffer, uint32_t drawCount, VkDeviceSize offset = 0,
                                           VkDeviceSize countBufferOffset = 0, uint32_t stride = sizeof(VkDrawIndirectCommand)) const;
    const CommandBuffer& drawIndexedIndirect(VkBuffer buffer, uint32_t drawCount, VkDeviceSize offset = 0,
                                             uint32_t stride = sizeof(VkDrawIndexedIndirectCommand)) const;
    const CommandBuffer& drawIndexedIndirectCount(VkBuffer buffer, VkBuffer countBuffer, uint32_t drawCount, VkDeviceSize offset = 0,
                                                  VkDeviceSize countBufferOffset = 0,
                                                  uint32_t stride = sizeof(VkDrawIndexedIndirectCommand)) const;
    const CommandBuffer& drawMeshTasks(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const;

    const CommandBuffer& endRendering() const;

    const CommandBuffer& dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const;
    const CommandBuffer& dispatchIndirect(VkBuffer buffer, VkDeviceSize offset = 0) const;

    const CommandBuffer& end() const;

    [[nodiscard]] operator VkCommandBuffer() const;
    [[nodiscard]] VkCommandBuffer get() const;
    [[nodiscard]] VkCommandBuffer operator*() const;

    const CommandBuffer& label(const VkDevice device, const std::string& name) const;

    const CommandBuffer& label(const VkDevice device, const char* name) const;

  private:
    VkCommandBuffer cmdBuf;
  };
} // namespace kt::rdr