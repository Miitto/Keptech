#pragma once

#include "keptech/cameras/orbitCamera.hpp"
#include "keptech/components/camera.hpp"
#include "keptech/components/transform.hpp"
#include "keptech/core/layers/layer.hpp"
#include "keptech/core/scene.hpp"
#include "keptech/core/window.hpp"
#include "keptech/rhi/rhi.hpp"
#include "pass.hpp"

class ExampleLayer : public kt::Layer {
public:
  ExampleLayer(kt::Window& window, kt::rhi::RenderGraphBuilder& builder, kt::rhi::Renderer& renderer) : kt::Layer("Example Mesh Shader") {
    auto& scene = kt::Scene::active();

    auto monkeyMeshRes = renderer.loadMesh(ASSET_DIR "meshes/monkey.glb");
    if (!monkeyMeshRes) {
      KT_ABORT("Failed to load monkey mesh: {}", monkeyMeshRes.error());
    }
    auto monkey = scene.createEntity("Monkey");

    monkeyMeshRes->addToEcsScene(scene, monkey.getHandle());

    auto camera = scene.createEntity("Camera");
    camera.addComponent<kt::components::Transform>()
        .getLocalMut()
        .translate(glm::vec3(12.0f, 4.0f, 4.0f))
        .lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    auto& camComp =
        camera.addComponent<kt::components::Camera>(kt::components::PerspectiveType::Standard, kt::components::Camera::Params::Common{},
                                                    kt::components::Camera::Params::Perspective{
                                                        .fovY = glm::radians(90.f),
                                                    });
    camComp.sizeToWindowSize(window.getRenderSize());

    scene.useCamera(camera);

    orbitController = kt::cameras::OrbitCameraController(camera, 3, true);

    auto& meshPass = builder.addPass("Mesh Shader");
    meshPass.setInterface(&geometryPass);
  }

  void onUpdate(kt::Timestep ts) final { orbitController.update(ts); }

  void onEvent(kt::Event& event, kt::Timestep ts) final {
    if (orbitController.handleEvent(event, ts))
      return;
  }

private:
  kt::cameras::OrbitCameraController orbitController{};

  GeometryPass geometryPass{};
};