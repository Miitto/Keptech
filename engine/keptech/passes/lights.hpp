#pragma once

#include "keptech/graph/passInterface.hpp"
#include "keptech/maths/frustum.hpp"
#include "keptech/rhi/pipeline.hpp"

namespace kt {
  class LightPass : public RenderPassInterface {
  public:
    LightPass() = default;

    void setupDependencies(RenderPassBuilder& self, RenderGraphBuilder& graph) final;
    bool validate(RenderPassBuilder& self, RenderGraphBuilder& graph) final;
    void setup(RenderGraph& graph, const rhi::DescriptorLayout&) final;

    void prepare(RenderGraph& graph) final;
    void execute(RenderGraph& graph, rhi::CommandBuffer& cmdBuf, const rhi::DescriptorSet&, glm::uvec2 framebufferSize) final;

    void addToGraph(RenderGraphBuilder& graph);

    struct GpuPointLight {
      glm::vec3 position;
      float radius;
      glm::vec3 color;
      float intensity;
    };

  private:
    size_t lightingIndex = 0;
    size_t lightBufferIndex = 0;

    size_t lightCount = 0;

    const maths::Frustum* cameraFrustum = nullptr;
    rhi::Pipeline pipeline;
  };
} // namespace kt