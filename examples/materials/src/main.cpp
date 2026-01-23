#include "keptech/core/components/transform.hpp"
#include <keptech/app.hpp>

#include "editorLayer.hpp"
#include "keptech/core/rendering/pipeline.hpp"
#include "keptech/ecs/entity.hpp"
#include <expected>
#include <keptech/components.hpp>
#include <keptech/core/gui.h>
#include <keptech/core/window.hpp>
#include <keptech/keptech.hpp>
#include <keptech/vulkan.hpp>
#include <utility>

namespace shaders {
#include "shaders/basic.h"
#include "shaders/deferred.h"
} // namespace shaders

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

keptech::SetupInfo keptech::configureApp() {
  return {.window = {.title = "Material Editor",
                     .width = WINDOW_WIDTH,
                     .height = WINDOW_HEIGHT},
          .renderer = {.applicationName = "Material Editor"}};
}

std::expected<void, std::string>
keptech::setupAppLayers(core::layers::LayerStack& layerStack,
                        core::window::Window& window,
                        keptech::vkh::Renderer& renderer) {
  using Material = keptech::vkh::Material;

  MaterialEditorLayer::initMeta();

  auto basicMaterialRes = renderer.createMaterial({
      .shader = ::shaders::basic,
  });
  if (!basicMaterialRes) {
    return std::unexpected(fmt::format("Failed to create basic material: {}",
                                       basicMaterialRes.error()));
  }

  auto deferredMaterialRes = renderer.createMaterial({
      .shader = ::shaders::deferred,
      .pipelineConfig =
          {

          },
  });
  if (!deferredMaterialRes) {
    return std::unexpected(fmt::format("Failed to create basic material: {}",
                                       deferredMaterialRes.error()));
  }
  auto& deferred = deferredMaterialRes.value();

  SPDLOG_INFO("Created materials");

  using Vertex = keptech::core::rendering::Mesh::Vertex;

  std::vector<Vertex> triangleVertices = {
      {
          .position = {-0.5f, -0.5f, 0.0f},
          .uvX = 0.f,
          .normal = {0.0f, 0.0f, 1.0f},
          .uvY = 0.f,
          .color = {1.0f, 0.0f, 0.0f, 1.0f},
      },
      {
          .position = {0.5f, -0.5f, 0.0f},
          .uvX = 1.f,
          .normal = {0.0f, 0.0f, 1.0f},
          .uvY = 0.f,
          .color = {0.0f, 1.0f, 0.0f, 1.0f},
      },
      {
          .position = {0.0f, 0.5f, 0.0f},
          .uvX = 0.5f,
          .normal = {0.0f, 0.0f, 1.0f},
          .uvY = 1.f,
          .color = {0.0f, 0.0f, 1.0f, 1.0f},
      },
  };

  auto triangleMeshRes =
      renderer.meshFromData({.name = "Triangle", .vertices = triangleVertices});
  if (!triangleMeshRes) {
    return std::unexpected(fmt::format("Failed to create triangle mesh: {}",
                                       triangleMeshRes.error()));
  }

  auto monkeyMeshRes = renderer.loadMesh(ASSET_DIR "meshes/monkey.glb");
  if (!monkeyMeshRes) {
    return std::unexpected(
        fmt::format("Failed to load monkey mesh: {}", monkeyMeshRes.error()));
  }
  SPDLOG_INFO("Loaded monkey mesh: {}", monkeyMeshRes.value().size());

  keptech::core::Scene scene;

  auto monkey = scene.createEntity("Monkey");
  monkey.addComponent<keptech::components::Mesh>(monkeyMeshRes.value()[0]);
  monkey.addComponent<keptech::components::Material>(deferred);
  monkey.addComponent<keptech::components::Transform>();

  auto camera = scene.createEntity("Camera");
  auto& camTransform = camera.addComponent<keptech::components::Transform>();
  auto& localTransform = camTransform.getLocalMut();
  localTransform.translate(glm::vec3(0.0f, 0.0f, 5.0f))
      .lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

  auto& camComp = camera.addComponent<keptech::components::Camera>(
      keptech::components::PerspectiveType::Standard,
      keptech::components::Camera::Params::Common{},
      keptech::components::Camera::Params::Perspective{.fovY =
                                                           glm::radians(90.f)});
  camComp.sizeToWindowSize(window.getRenderSize());

  scene.useCamera(camera);

  layerStack.emplaceLayer<MaterialEditorLayer>(renderer, std::move(scene));

  return {};
}
