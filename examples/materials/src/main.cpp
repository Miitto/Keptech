#include <keptech/app.hpp>

#include <keptech/core/window.hpp>
#include <keptech/gui.h>
#include <keptech/vulkan/helpers/shader.hpp>
#include <keptech/vulkan/material.hpp>
#include <keptech/vulkan/renderObject.hpp>
#include <keptech/vulkan/renderer.hpp>
#include <span>
#include <spdlog/spdlog.h>

namespace shaders {
#include "shaders/basic.h"
}

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

template <class R>
  requires(keptech::core::renderer::Renderer<R>)
class App : public keptech::App {
public:
  struct Materials {
    using Material = R::Material;

    Material basic;
  };

  struct Meshes {
    using Mesh = R::Mesh;

    Mesh triangle;
  };

  App(keptech::core::window::Window&& window, R* renderer,
      Materials&& materials, Meshes& meshes)
      : keptech::App(std::move(window)), renderer(renderer),
        materials(std::move(materials)), meshes(std::move(meshes)) {}

  void update() override {
    {
      keptech::gui::Frame frame("Demo");
      frame.text("Material Editor Example");
    }

    renderer->addRenderObject(&renderObject);
  }

  void onEvent(const keptech::core::window::Window::Event& event) override {
    // Handle window events here
  }

private:
  R* renderer;

  Materials materials;
  Meshes meshes;

  keptech::vkh::RenderObject renderObject{
      .mesh = meshes.triangle,
      .material = &materials.basic,
  };
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

    auto renderer = std::move(renderer_res.value());

    using Material = keptech::vkh::Material;

    auto basicShaderRes =
        renderer.createShader(shaders::basic, shaders::basic_size);
    if (!basicShaderRes) {
      SPDLOG_CRITICAL("Failed to create basic shader: {}",
                      basicShaderRes.error());
      return -1;
    }
    auto basicShader = std::move(basicShaderRes.value());

    auto basicShaderStages = basicShader.vertFrag();

    auto materialRes = renderer.createMaterial(
        Material::Stage::Forward,
        keptech::vkh::GraphicsPipelineConfig{
            .rendering = {.colorAttachmentFormats =
                              {renderer.getSwapchainImageFormat()}},
            .shaders = {basicShaderStages},
            .layout = {.pushConstantRanges = {{vk::PushConstantRange{
                           .stageFlags = vk::ShaderStageFlagBits::eVertex,
                           .offset = 0,
                           .size = sizeof(vk::DeviceAddress),
                       }}}},
        });
    if (!materialRes) {
      SPDLOG_CRITICAL("Failed to create basic material: {}",
                      materialRes.error());
      return -1;
    }
    auto materials = App::Materials{
        .basic = std::move(materialRes.value()),
    };

    using Vertex = keptech::core::Vertex;
    using UnpackedVertex = keptech::core::UnpackedVertex;

    std::array<Vertex, 3> triangleVertices = {
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

    auto triangleMeshRes = renderer.meshFromData(triangleVertices, {});
    if (!triangleMeshRes) {
      SPDLOG_CRITICAL("Failed to create triangle mesh: {}",
                      triangleMeshRes.error());
      return -1;
    }
    App::Meshes meshes{
        .triangle = triangleMeshRes.value(),
    };

    App app(std::move(window), &renderer, std::move(materials), meshes);

    SPDLOG_INFO("Starting Material Editor");
    keptech::run(app, renderer);
  }

  keptech::core::window::shutdown();

  return 0;
}
