#pragma once

#include <Volk/volk.h>
#include <span>

namespace kt::vkh {
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

    const CommandBuffer& beginRendering(const VkRenderingInfo& renderingInfo) const;
    const CommandBuffer& endRendering() const;

    const CommandBuffer& end() const;

    [[nodiscard]] operator VkCommandBuffer() const;
    [[nodiscard]] VkCommandBuffer get() const;
    [[nodiscard]] VkCommandBuffer operator*() const;

    const CommandBuffer& label(const VkDevice device, const std::string& name) const;

    const CommandBuffer& label(const VkDevice device, const char* name) const;

  private:
    VkCommandBuffer cmdBuf;
  };
} // namespace kt::vkh