#include "data.hpp"

#include "graph/graph.hpp"
#include "keptech/components/camera.hpp"
#include "keptech/components/transform.hpp"
#include "keptech/core/scene.hpp"
#include "keptech/graph/builder.hpp"
#include "keptech/mesh.hpp"

namespace kt {

  void DataPass::setupDependencies(RenderPassBuilder& self, RenderGraphBuilder&) {
    self.addMappedBuffer("kt::camera", sizeof(kt::components::Camera::Uniforms));
    self.addMappedBuffer("kt::objects", 1000 * sizeof(Object));
  }

  void DataPass::setup(RenderGraph& graph, const rhi::DescriptorLayout&) {
    camIndex = graph.getBufferIndex("kt::camera");
    objectsIndex = graph.getBufferIndex("kt::objects");

    graph.setUserData("kt::data::writtenObjects", &objects);
    graph.setUserData("kt::data::cameraFrustum", &cameraFrustum);
    graph.setUserData("kt::data::envMapIndex", &envMapIndex);
  }

  void DataPass::prepare(RenderGraph& graph) {
    auto& scene = Scene::active();
    auto view = scene.view<Mesh, components::Transform>();
    objects.clear();
    std::vector<GpuObject> gpuObjects;
    gpuObjects.reserve(view.size_hint());
    objects.reserve(view.size_hint());
    for (const auto& [entity, mesh, transform] : view.each()) {
      for (auto& submesh : mesh.getSubmeshes()) {
        gpuObjects.push_back({.modelMatrix = transform.getGlobal(), .meshIndex = submesh.id, .materialIndex = submesh.material.id});
        auto s = submesh;
        s.boundingSphere = s.boundingSphere.apply(transform.getGlobal());
        objects.push_back({.modelMatrix = transform.getGlobal(), .submesh = s});
      }
    }

    components::Camera::Uniforms camUniforms{};
    auto cam = scene.getActiveCamera();
    auto [camT, camC] = cam.getComponents<components::Transform, components::Camera>();
    camUniforms.invViewMatrix = camT.getGlobal();
    camUniforms.viewMatrix = glm::inverse(camUniforms.invViewMatrix);
    camC.recalculateProjectionMatrix();
    camUniforms.projectionMatrix = camC.getProjectionMatrix();
    camUniforms.invProjectionMatrix = glm::inverse(camUniforms.projectionMatrix);
    camUniforms.viewProjectionMatrix = camUniforms.projectionMatrix * camUniforms.viewMatrix;
    camUniforms.invViewProjectionMatrix = glm::inverse(camUniforms.viewProjectionMatrix);
    camUniforms.envMapIndex = envMapIndex;

    camUniforms.frustum = maths::Frustum::fromViewProjectionMatrix(camUniforms.viewProjectionMatrix);

    cameraFrustum = camUniforms.frustum;

    auto& camBuffer = graph.getFrameBuffer(camIndex);
    memcpy(camBuffer.mapping(), &camUniforms, sizeof(components::Camera::Uniforms));

    size_t objectBufferSize = gpuObjects.size() * sizeof(GpuObject);
    {
      auto& objectBuffer = graph.getFrameBuffer(objectsIndex);
      if (objectBuffer.size() < objectBufferSize) {
        graph.reallocateBuffer(objectsIndex, objectBufferSize, false);
      }
      memcpy(objectBuffer.mapping(), gpuObjects.data(), objectBufferSize);
    }
    auto& objectBuffer = graph.getFrameBuffer(objectsIndex);
    memcpy(objectBuffer.mapping(), gpuObjects.data(), objectBufferSize);
  }

  void DataPass::addToGraph(RenderGraphBuilder& graph) {
    auto& pass = graph.addPass("kt::data", QueueType::Cpu);
    pass.setInterface(this);
  }
} // namespace kt