#pragma once

#include "keptech/graph/passInterface.hpp"
#include "keptech/maths/frustum.hpp"
#include "keptech/mesh.hpp"

namespace kt {
  struct GpuObject {
    glm::mat4 modelMatrix;
    uint32_t meshIndex;
    uint32_t materialIndex;
  };

  struct Object {
    glm::mat4 modelMatrix;
    Submesh submesh;
  };

  class DataPass : public RenderPassInterface {
  public:
    DataPass() = default;

    void setupDependencies(RenderPassBuilder& self, RenderGraphBuilder& graph) final;
    void setup(RenderGraph& graph, const rhi::DescriptorLayout&) final;

    void prepare(RenderGraph& graph) final;

    void addToGraph(RenderGraphBuilder& graph);

    void setEnvironmentMapIndex(uint32_t index) { envMapIndex = index; }

  private:
    size_t camIndex = 0;
    size_t objectsIndex = 0;
    std::vector<Object> objects{};
    maths::Frustum cameraFrustum{};
    uint32_t envMapIndex = ~0u;
  };
} // namespace kt