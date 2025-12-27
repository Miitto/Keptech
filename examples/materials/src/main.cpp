#include <keptech/app.hpp>

#include <keptech/core/rendering/material.hpp>
#include <keptech/core/rendering/renderObject.hpp>
#include <keptech/core/window.hpp>
#include <keptech/gui.h>
#include <keptech/vulkan/helpers/shader.hpp>
#include <keptech/vulkan/material.hpp>
#include <keptech/vulkan/renderer.hpp>
#include <spdlog/spdlog.h>

namespace shaders {
#include "shaders/basic.h"
}

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

template <class R> class App : public keptech::App {
public:
  struct Materials {
    using Material = R::MaterialHandle;

    Material basic;
  };

  struct Meshes {
    using Mesh = R::MeshHandle;

    Mesh triangle;
    Mesh monkey;
  };

  App(keptech::core::window::Window&& window, R& renderer, Materials& materials,
      Meshes& meshes)
      : keptech::App(std::move(window)), renderer(&renderer),
        materials(materials), meshes(meshes) {

    auto& ecs = keptech::ecs::ECS::get();
    auto& triangle = ecs.createEntity("Triangle");
    ecs.addComponent<keptech::components::Transform>(
        triangle, keptech::components::Transform());
    ecs.addComponent<keptech::core::rendering::RenderObject>(
        triangle, keptech::core::rendering::RenderObject{
                      .mesh = meshes.triangle,
                      .material = materials.basic,
                  });
  }

  void update() override {
    {
      keptech::gui::Frame frame("Demo");
      frame.text("Material Editor Example");
    }
  }

  void onEvent(const keptech::core::window::Window::Event& event) override {
    (void)event;
  }

private:
  R* renderer;

  Materials materials;
  Meshes meshes;
};

int main() {
  using App = App<keptech::vkh::Renderer>;

  keptech::core::window::init();

  {
    keptech::core::window::Window window("Material Editor", WINDOW_WIDTH,
                                         WINDOW_HEIGHT);

    auto renderer_res =
        keptech::vkh::Renderer::create("Material Editor", window);
    if (!renderer_res) {
      SPDLOG_CRITICAL("Failed to create Vulkan renderer: {}",
                      renderer_res.error());
      return -1;
    }
    auto& renderer = *renderer_res.value();

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
                            {keptech::core::rendering::Format::Default},
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
      SPDLOG_CRITICAL("Failed to create basic material: {}",
                      materialRes.error());
      return -1;
    }
    auto materials = App::Materials{
        .basic = materialRes.value(),
    };

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

    auto triangleMeshRes = renderer.meshFromData(
        {.name = "Triangle", .vertices = triangleVertices});
    if (!triangleMeshRes) {
      SPDLOG_CRITICAL("Failed to create triangle mesh: {}",
                      triangleMeshRes.error());
      return -1;
    }

    auto monkeyMeshRes = renderer.loadMesh(ASSET_DIR "meshes/monkey.glb");
    if (!monkeyMeshRes) {
      SPDLOG_CRITICAL("Failed to load monkey mesh: {}", monkeyMeshRes.error());
      return -1;
    }

    App::Meshes meshes{
        .triangle = triangleMeshRes.value(),
        .monkey = monkeyMeshRes.value().front(),
    };

    App app(std::move(window), renderer, materials, meshes);

    SPDLOG_INFO("Starting Material Editor");
    keptech::run(app, renderer);
  }

  keptech::core::window::shutdown();

  SPDLOG_INFO("Material Editor exited cleanly");

  return 0;
}
