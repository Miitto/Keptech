#pragma once

#include "keptech/graph/passInterface.hpp"
#include "keptech/rhi/pipeline.hpp"

namespace kt {
  class SkyboxPass : public RenderPassInterface {
  public:
    SkyboxPass() = default;

    void setupDependencies(RenderPassBuilder& self, RenderGraphBuilder& graph) final;
    void setup(RenderGraph& graph, const rhi::DescriptorLayout&) final;

    void execute(RenderGraph& graph, rhi::CommandBuffer& cmdBuf, const rhi::DescriptorSet&, glm::uvec2 framebufferSize) final;

    void addToGraph(RenderGraphBuilder& graph, std::string srcTexName);

  private:
    size_t resultIndex = 0;
    std::string srcTexName;

    rhi::Pipeline pipeline;
  };
} // namespace kt