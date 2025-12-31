#include <keptech/app.hpp>

#include <keptech/core/components/renderObject.hpp>
#include <keptech/core/rendering/material.hpp>
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

struct Materials {
  keptech::core::SlotMapSmartHandle basic;
};

struct Meshes {
  keptech::core::SlotMapSmartHandle triangle;
  keptech::core::SlotMapSmartHandle monkey;
};
int main() {
  struct Resources {
    Meshes meshes;
    Materials materials;
  };

  auto setup = [](keptech::core::window::Window& window,
                  keptech::vkh::Renderer& renderer)
      -> std::expected<Resources, std::string> {
    using Material = keptech::vkh::Material;

    (void)window;

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

    auto triangleMeshRes = renderer.meshFromData(
        {.name = "Triangle", .vertices = triangleVertices});
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
    SPDLOG_INFO("Loaded monkey mesh");

    Meshes meshes{
        .triangle = triangleMeshRes.value(),
        .monkey = monkeyMeshRes.value().front(),
    };

    auto& ecs = keptech::ecs::ECS::get();

    auto triangle = ecs.createEntity("Triangle");
    ecs.addComponent<keptech::components::RenderObject>(
        triangle, {.mesh = meshes.triangle, .material = materials.basic});
    ecs.addComponent<keptech::components::Transform>(triangle, {});

    auto camera = ecs.createEntity("Camera");
    keptech::core::cameras::Camera camObj{
        keptech::core::cameras::Camera::ProjectionType::Perspective};
    camObj.sizeToWindow(WINDOW_WIDTH, WINDOW_HEIGHT)
        .setPosition({0.f, 0.f, 2.f})
        .setFovY(90.f);
    ecs.addComponent<keptech::core::cameras::Camera>(camera, std::move(camObj));

    return Resources{.meshes = meshes, .materials = materials};
  };

  SPDLOG_INFO("Starting Material Editor");
  bool success = keptech::run<keptech::vkh::Renderer, Resources>(
      {.title = "Material Editor",
       .width = WINDOW_WIDTH,
       .height = WINDOW_HEIGHT},
      {.applicationName = "Material Editor"}, setup,
      [](auto& window, auto event, auto& resources) {});

  keptech::core::window::shutdown();

  if (!success) {
    SPDLOG_CRITICAL("Material Editor exited with errors");
    return -1;
  }

  SPDLOG_INFO("Material Editor exited cleanly");

  return 0;
}
