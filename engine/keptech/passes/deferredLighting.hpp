#pragma once

#include "keptech/graph/passInterface.hpp"
#include "keptech/passes/data.hpp"
#include "keptech/rhi/pipeline.hpp"

namespace kt {
  class DeferredLightingPass : public RenderPassInterface {
  public:
    DeferredLightingPass() = default;

    void setupDependencies(RenderPassBuilder& self, RenderGraphBuilder& graph) final;
    bool validate(RenderPassBuilder& self, RenderGraphBuilder& graph) final;
    void setup(RenderGraph& graph) final;

    void prepare(RenderGraph& graph) final;
    void execute(RenderGraph& graph, rhi::CommandBuffer& cmdBuf, rhi::DescriptorSet&, glm::uvec2 framebufferSize) final;

    void addToGraph(RenderGraphBuilder& graph);

    struct GpuLight {
      glm::vec3 position;
      float radius;
      glm::vec3 color;
      float intensity;
    };

  private:
    size_t diffuseIndex = 0;
    size_t specularIndex = 0;

    size_t lightCount = 0;

    const maths::Frustum* cameraFrustum = nullptr;
    rhi::Pipeline pipeline;
  };
} // namespace kt