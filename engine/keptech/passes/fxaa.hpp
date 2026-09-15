#pragma once

#include "keptech/graph/passInterface.hpp"
#include "keptech/rhi/pipeline.hpp"

namespace kt {
  class FxaaPass : public RenderPassInterface {
  public:
    struct FxaaOptions {
      float fixedLumaThreshold = 0.0833f;
      float relativeLumaThreshold = 0.166f;
      float blendFactor = 1.0f;
      uint32_t lumaInAlpha = false;
    };

    FxaaPass() = default;

    void setupDependencies(RenderPassBuilder& self, RenderGraphBuilder& graph) final;
    void setup(RenderGraph& graph, const rhi::DescriptorLayout&) final;

    void execute(RenderGraph& graph, rhi::CommandBuffer& cmdBuf, const rhi::DescriptorSet&, glm::uvec2 framebufferSize) final;

    void addToGraph(RenderGraphBuilder& graph, std::string srcTexName);

    FxaaOptions& getOptions() { return options; }

  private:
    size_t resultIndex = 0;
    std::string srcTexName;
    FxaaOptions options{};

    rhi::Pipeline pipeline;
  };
} // namespace kt