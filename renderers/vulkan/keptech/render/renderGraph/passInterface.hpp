#pragma once

namespace kt::rdr {
  class Renderer;
  class RenderPassBuilder;
  class RenderGraphBuilder;
  class RenderGraph;
  class CommandBuffer;

  class RenderPassInterface {
  public:
    RenderPassInterface() = default;
    RenderPassInterface(const RenderPassInterface&) = default;
    RenderPassInterface(RenderPassInterface&&) = default;
    RenderPassInterface& operator=(const RenderPassInterface&) = default;
    RenderPassInterface& operator=(RenderPassInterface&&) = default;
    virtual ~RenderPassInterface() = default;

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif

    [[nodiscard]] virtual bool needRenderPass() const { return true; }
    [[nodiscard]] virtual bool getClearDepthStencil(VkClearDepthStencilValue* value) const {
      if (value)
        *value = {};
      return true;
    }
    [[nodiscard]] virtual bool getClearColor(size_t attachmentIndex, VkClearColorValue* value) const {
      if (value)
        *value = {};
      return true;
    }

    /// Called once before the render graph is baked. The renderer should not be used to create resources in this function, only used to
    /// query information about the device and queues. Create resources in `setup()` instead.
    virtual void setupDependencies(RenderPassBuilder& self, RenderGraphBuilder& graph, const Renderer& renderer) {}

    /// Called once after the render graph has been built.
    virtual void setup(Renderer& renderer, VkDescriptorSetLayout descriptorSetLayout) {}

    /// Called before the pass is executed. This is where you should update any resources that are used by the pass.
    virtual void prepare(RenderGraph& graph, Renderer& renderer) {}
    /// @brief Called when the pass is executed. This is where you should record the commands for the pass.
    /// @param cmd The command buffer to record commands to.
    /// @param descriptorSet The descriptor set for the pass. This is populated with the resources that were specified during setup.
    /// @param framebufferSize The size of the framebuffer for this pass. This is useful for setting the viewport and scissor.
    virtual void execute(const CommandBuffer& cmd, VkDescriptorSet descriptorSet, glm::uvec2 framebufferSize = {}) {}

    /// Called when the render graph is destroyed.
    virtual void shutdown(Renderer& renderer) {}

#if __clang__
#pragma clang diagnostic pop
#endif
  };
} // namespace kt::rdr