#pragma once

#include "keptech/graph/passInterface.hpp"
#include "keptech/maths/frustum.hpp"
#include "keptech/maths/sphere.hpp"
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
    void setup(RenderGraph& graph) final;

    void prepare(RenderGraph& graph) final;

    void addToGraph(RenderGraphBuilder& graph);

  private:
    size_t camIndex;
    size_t objectsIndex;
    std::vector<Object> objects;
    maths::Frustum cameraFrustum;
  };
} // namespace kt