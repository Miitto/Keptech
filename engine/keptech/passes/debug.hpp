#pragma once

#include "keptech/graph/passInterface.hpp"
#include <string>

namespace kt {
  class DebugPass : public RenderPassInterface {
  public:
    void setupDependencies(RenderPassBuilder& self, RenderGraphBuilder& graph) final;
    void setup(RenderGraph& graph, const rhi::DescriptorLayout&) final;

    void execute(RenderGraph& graph, rhi::CommandBuffer& cmdBuf, const rhi::DescriptorSet&, glm::uvec2 framebufferSize) final;

    void addToGraph(RenderGraphBuilder& graph);

  private:
    std::vector<size_t> renderTargets;
    size_t debugImageIndex = 0;
    size_t debugView = 0;
    std::string backbufferSource;
  };
} // namespace kt