#pragma once

#include "keptech/graph/passInterface.hpp"
#include "keptech/rhi/pipeline.hpp"

namespace kt {
  class LightCombinePass : public RenderPassInterface {
  public:
    LightCombinePass() = default;

    void setupDependencies(RenderPassBuilder& self, RenderGraphBuilder& graph) final;
    void setup(RenderGraph& graph, const rhi::DescriptorLayout&) final;

    void execute(RenderGraph& graph, rhi::CommandBuffer& cmdBuf, const rhi::DescriptorSet&, glm::uvec2 framebufferSize) final;

    void addToGraph(RenderGraphBuilder& graph);

  private:
    size_t lightTexIndex = 0;

    rhi::Pipeline pipeline;
  };
} // namespace kt