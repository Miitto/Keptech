#pragma once

#include "keptech/graph/passInterface.hpp"
#include "keptech/passes/data.hpp"
#include "keptech/rhi/pipeline.hpp"

namespace kt {
  class GeometryPass : public RenderPassInterface {
  public:
    GeometryPass() = default;

    void setupDependencies(RenderPassBuilder& self, RenderGraphBuilder& graph) final;
    bool validate(RenderPassBuilder& self, RenderGraphBuilder& graph) final;
    void setup(RenderGraph& graph) final;

    void prepare(RenderGraph& graph) final;
    void execute(RenderGraph& graph, rhi::CommandBuffer& cmdBuf, rhi::DescriptorSet&, glm::uvec2 framebufferSize) final;

    void addToGraph(RenderGraphBuilder& graph);

  private:
    size_t albedoIndex = 0;
    size_t normalIndex = 0;
    size_t materialIndex = 0;
    size_t emissiveIndex = 0;
    size_t depthIndex = 0;
    size_t drawCommandsIndex = 0;

    size_t objectsIndex = 0;
    size_t cameraIndex = 0;

    const std::vector<Object>* writtenObjects = nullptr;
    const maths::Frustum* cameraFrustum = nullptr;
    uint32_t drawCommandCount = 0;

    rhi::Pipeline pipeline;
  };
} // namespace kt