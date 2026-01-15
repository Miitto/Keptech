#include "imgui.h"
#include "keptech/cameras/orbitCamera.hpp"
#include "keptech/core/components/transform.hpp"
#include "keptech/core/events/event.hpp"
#include <keptech/app.hpp>

#include "imgui_internal.h"
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
}

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

struct Materials {
  keptech::MaterialHandle basic;
};

struct Meshes {
  keptech::MeshHandle triangle;
  keptech::MeshHandle monkey;
};

keptech::SetupInfo keptech::configureApp() {
  return {.window = {.title = "Material Editor",
                     .width = WINDOW_WIDTH,
                     .height = WINDOW_HEIGHT},
          .renderer = {.applicationName = "Material Editor"}};
}

class MaterialEditorLayer : public keptech::core::layers::Layer {
public:
  struct Resources {
    Meshes meshes;
    Materials materials;
  };

  MaterialEditorLayer(KEPTECH_RENDERER& renderer, keptech::core::Scene&& scene,
                      Resources&& resources)
      : keptech::core::layers::Layer("MaterialEditorLayer"), renderer(renderer),
        scene(std::move(scene)), resources(std::move(resources)),
        orbitController(this->scene.getActiveCamera()) {}

  void onUpdate(keptech::core::Timestep ts) override {
    {
      auto frame = keptech::gui::Frame("Stats");
      double fps = static_cast<double>(1000.f / ts);
      frame.text("FPS: %.1f", fps);

      frame.inputFloat("Sensitivity", orbitController.getSensitivity());
    }

    ImGuiID dockspace_id = ImGui::GetID("Editor Dock");
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    // Create settings
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
      ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
      ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);
      ImGuiID dock_id_left = 0;
      ImGuiID dock_id_main = dockspace_id;
      ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f,
                                  &dock_id_left, &dock_id_main);
      ImGuiID dock_id_left_top = 0;
      ImGuiID dock_id_left_bottom = 0;
      ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Up, 0.50f,
                                  &dock_id_left_top, &dock_id_left_bottom);
      ImGui::DockBuilderDockWindow("Game", dock_id_main);
      ImGui::DockBuilderDockWindow("Scene Tree", dock_id_left_top);
      ImGui::DockBuilderDockWindow("Properties", dock_id_left_bottom);
      ImGui::DockBuilderFinish(dockspace_id);
    }

    // Submit dockspace
    ImGui::DockSpaceOverViewport(dockspace_id, viewport,
                                 ImGuiDockNodeFlags_PassthruCentralNode);
    {
      auto scenePanel = keptech::gui::Frame("Scene Tree", nullptr,
                                            ImGuiWindowFlags_NoDecoration |
                                                ImGuiWindowFlags_NoMove);
      {
        auto child = scenePanel.child("Entities");
        auto view = scene.getEcs().view<keptech::components::Name>();
        for (auto [entity, name] : view.each()) {
          if (child.selectable(name->c_str(), entity == selectedEntity))
            selectedEntity = entity;
        }
      }
    }

    if (selectedEntity != keptech::ecs::INVALID_ENTITY_HANDLE) {
      auto propertiesPanel = keptech::gui::Frame("Properties", nullptr,
                                                 ImGuiWindowFlags_NoDecoration |
                                                     ImGuiWindowFlags_NoMove);

      auto& ecs = scene.getEcs();
      auto entity = keptech::ecs::Entity(selectedEntity, ecs);

      if (entity.hasAllComponents<keptech::components::Transform>()) {
        static keptech::core::Bitflag<
            keptech::components::Transform::TransformGuiFlags>
            flags = keptech::components::Transform::TransformGuiFlags::Editable;
        entity.getComponents<keptech::components::Transform>().guiPane(
            propertiesPanel, flags);
      }
    }

    renderer.submitScene(scene);
  }

  void onEvent(keptech::core::events::Event& event,
               keptech::core::Timestep ts) override {
    if (orbitController.handleEvent(event, ts))
      return;
  }

private:
  KEPTECH_RENDERER& renderer;
  keptech::core::Scene scene;
  Resources resources;
  keptech::cameras::OrbitCameraController orbitController;
  keptech::ecs::EntityHandle selectedEntity =
      keptech::ecs::INVALID_ENTITY_HANDLE;
};

std::expected<void, std::string>
keptech::setupAppLayers(core::layers::LayerStack& layerStack,
                        core::window::Window& window,
                        keptech::vkh::Renderer& renderer) {
  using Material = keptech::vkh::Material;

  auto materialRes = renderer.createMaterial({
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
  if (!materialRes) {
    return std::unexpected(fmt::format("Failed to create basic material: {}",
                                       materialRes.error()));
  }
  auto materials = Materials{
      .basic = materialRes.value(),
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
                                        .material = materials.basic});
  triangle.addComponent<keptech::components::Transform>();

  auto monkey = scene.createEntity("Monkey");
  monkey.addComponent<keptech::components::RenderObject>(
      keptech::components::RenderObject{.mesh = meshes.monkey,
                                        .material = materials.basic});
  monkey.addComponent<keptech::components::Transform>();

  auto camera = scene.createEntity("Camera");
  auto& camTransform = camera.addComponent<keptech::components::Transform>();
  auto& localTransform = camTransform.getLocalMut();
  localTransform.translate(glm::vec3(2.0f, 2.0f, 5.0f))
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
