#pragma once

#include <glm/ext/vector_uint2.hpp>

namespace kt {
  namespace rhi {
    class RHI;
    class CommandBuffer;
    struct DepthClearValue;
    struct ColorClearValue;
    struct ResourceLayout;
    struct ResourceSet;
  } // namespace rhi
  class RenderPassBuilder;
  class RenderGraphBuilder;
  class RenderGraph;

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
    [[nodiscard]] virtual bool getClearDepthStencil(rhi::DepthClearValue* value) const { return true; }
    [[nodiscard]] virtual bool getClearColor(size_t attachmentIndex, rhi::ColorClearValue* value) const { return true; }

    /// Called once before the render graph is baked. The renderer should not be used to create resources in this function, only used to
    /// query information about the device and queues. Create resources in `setup()` instead.
    virtual void setupDependencies(RenderPassBuilder& self, RenderGraphBuilder& graph) {}

    /// Called once after every pass has run `setupDependencies()`. This is where you should validate that the pass has all the resources it
    /// needs to run. Return false to abort the render graph build. This is useful for passes that require certain resources to be present
    /// outside the graph, or just to provide a better error message.
    virtual bool validate(RenderPassBuilder& self, RenderGraphBuilder& graph) { return true; }

    /// Called once after the render graph has been built.
    virtual void setup(RenderGraph& graph
#ifdef KT_VULKAN
                       ,
                       ResourceLayout& resourceLayout
#endif
    ) {
    }

    /// Called before the pass is executed. This is where you should update any resources that are used by the pass.
    virtual void prepare(RenderGraph& graph) {}

    /// @brief Called when the pass is executed. This is where you should record the commands for the pass.
    /// @param cmd The command buffer to record commands to.
    /// @param descriptorSet The descriptor set for the pass. This is populated with the resources that were specified during setup.
    /// @param framebufferSize The size of the framebuffer for this pass. This is useful for setting the viewport and scissor.
    virtual void execute(RenderGraph& graph, rhi::CommandBuffer& cmd,
#ifdef KT_VULKAN
                         ResourceSet& resourceSet,
#endif
                         glm::uvec2 framebufferSize = {}) {
    }

    /// Called when the render graph is destroyed.
    virtual void shutdown(RenderGraph& graph) {}

#if __clang__
#pragma clang diagnostic pop
#endif
  };
} // namespace kt