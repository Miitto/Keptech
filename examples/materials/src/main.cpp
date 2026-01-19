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

  auto basicMaterialRes = renderer.createMaterial({
      .stage = Material::Stage::Forward,
      .pipelineConfig =
          {
              .shaders = {{
                  .code = shaders::basic,
                  .size = shaders::basic_size,
              }},
              .attachments =
                  {
                      .colorFormats =
                          {keptech::core::rendering::Texture::Format::Default},
                  },
              .layout =
                  {
                      .pushConstantRanges =
                          {
                              {
                                  .size = sizeof(vk::DeviceAddress),
                                  .stages = keptech::core::rendering::
                                      ShaderStages::Vertex,
                              },
                          },
                  },
          },
  });
  if (!basicMaterialRes) {
    return std::unexpected(fmt::format("Failed to create basic material: {}",
                                       basicMaterialRes.error()));
  }
  auto deferredMaterialRes = renderer.createMaterial({
      .stage = Material::Stage::Deferred,
      .pipelineConfig =
          {
              .shaders = {{
                  .code = shaders::deferred,
                  .size = shaders::deferred_size,
              }},
              .attachments = renderer.deferredPipelineAttachmentConfig(),
          },
  });
  if (!deferredMaterialRes) {
    return std::unexpected(fmt::format("Failed to create basic material: {}",
                                       deferredMaterialRes.error()));
  }
  auto materials = Materials{
      .basic = basicMaterialRes.value(),
      .deferred = deferredMaterialRes.value(),
  };
  SPDLOG_INFO("Created materials");

  using Vertex = keptech::core::rendering::Mesh::Vertex;
  using UnpackedVertex = keptech::core::rendering::Mesh::UnpackedVertex;

  std::vector<Vertex> triangleVertices = {
      UnpackedVertex{
          .position = {-0.5f, -0.5f, 0.0f},
          .uv = {0.0f, 0.0f},
          .normal = {0.0f, 0.0f, 1.0f},
          .color = {1.0f, 0.0f, 0.0f, 1.0f},
      },
      UnpackedVertex{
          .position = {0.5f, -0.5f, 0.0f},
          .uv = {1.0f, 0.0f},
          .normal = {0.0f, 0.0f, 1.0f},
          .color = {0.0f, 1.0f, 0.0f, 1.0f},
      },
      UnpackedVertex{
          .position = {0.0f, 0.5f, 0.0f},
          .uv = {0.5f, 1.0f},
          .normal = {0.0f, 0.0f, 1.0f},
          .color = {0.0f, 0.0f, 1.0f, 1.0f},
      },
  };

  auto triangleMeshRes =
      renderer.meshFromData({.name = "Triangle", .vertices = triangleVertices});
  if (!triangleMeshRes) {
    return std::unexpected(fmt::format("Failed to create triangle mesh: {}",
                                       triangleMeshRes.error()));
  }
  SPDLOG_INFO("Created triangle mesh");

  auto monkeyMeshRes = renderer.loadMesh(ASSET_DIR "meshes/monkey.glb");
  if (!monkeyMeshRes) {
    return std::unexpected(
        fmt::format("Failed to load monkey mesh: {}", monkeyMeshRes.error()));
  }
  SPDLOG_INFO("Loaded monkey mesh: {}", monkeyMeshRes.value().size());

  Meshes meshes{
      .triangle = triangleMeshRes.value(),
      .monkey = std::move(monkeyMeshRes.value().front()),
  };

  keptech::core::Scene scene;

  auto triangle = scene.createEntity("Triangle");
  triangle.addComponent<keptech::components::RenderObject>(
      keptech::components::RenderObject{.mesh = meshes.triangle,
                                        .material = materials.deferred});
  auto& triTrans = triangle.addComponent<keptech::components::Transform>();
  triTrans.getLocalMut().translate({0.f, 2.f, 0.f});

  auto monkey = scene.createEntity("Monkey");
  monkey.addComponent<keptech::components::RenderObject>(
      keptech::components::RenderObject{.mesh = meshes.monkey,
                                        .material = materials.deferred});
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

  MaterialEditorLayer::Resources resources{.meshes = meshes,
                                           .materials = materials};

  layerStack.emplaceLayer<MaterialEditorLayer>(renderer, std::move(scene),
                                               std::move(resources));

  return {};
}
