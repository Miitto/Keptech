#pragma once

#include "keptech/cameras/orbitCamera.hpp"
#include "keptech/components/camera.hpp"
#include "keptech/components/transform.hpp"
#include "keptech/core/layers/layer.hpp"
#include "keptech/core/scene.hpp"
#include "keptech/core/window.hpp"
#include "keptech/rhi/rhi.hpp"
#include "pass.hpp"
#include <keptech/gltf/data.hpp>

class ExampleLayer : public kt::Layer {
public:
  ExampleLayer(kt::Window& window, kt::RenderGraphBuilder& builder, kt::rhi::RHI& rhi) : kt::Layer("Example Mesh Shader") {
    auto& scene = kt::Scene::active();

    auto monkeyMeshDataRes = kt::gltf::Data::fromFile(ASSET_DIR "meshes/monkey.glb");
    if (!monkeyMeshDataRes) {
      KT_ABORT("Failed to load monkey mesh: {}", monkeyMeshDataRes.error());
    }

    auto monkeyUploadRes = monkeyMeshDataRes.value().upload();
    if (!monkeyUploadRes) {
      KT_ABORT("Failed to upload monkey mesh: {}", monkeyUploadRes.error());
    }
    auto& monkeyUpload = monkeyUploadRes.value();

    rhi.waitCopyIdle();

    auto monkey = scene.createEntity("Monkey");
    monkeyUpload.scene.addToEcsScene(scene, monkey.getHandle());

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

    dataPass.addToGraph(builder);

    auto& meshPassB = builder.addPass("Mesh Shader");
    meshPassB.setInterface(&meshPass);

    builder.setBackbufferSource("color");
  }

  void onUpdate(kt::Timestep ts) final { orbitController.update(ts); }

  void onEvent(kt::Event& event, kt::Timestep ts) final {
    if (orbitController.handleEvent(event, ts))
      return;
  }

private:
  kt::cameras::OrbitCameraController orbitController{};

  kt::DataPass dataPass{};
  MeshPass meshPass{};
};