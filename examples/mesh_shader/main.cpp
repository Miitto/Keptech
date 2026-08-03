#include "layer.hpp"
#include <keptech/app.hpp>
#include <keptech/core/window.hpp>

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

kt::SetupInfo kt::configureApp() {
  return {
      .window = {.title = "Keptech Mesh Shader Example", .flags = WindowCreateFlagBits::Resizable},
      .renderer = {.applicationName = "Keptech Mesh Shader Example", .requiredCapabilities = kt::RendererCapabilities::MeshShader},
  };
}

std::expected<void, std::string> kt::setupAppLayers(kt::LayerStack& layerStack, kt::Window& window, kt::rdr::RenderGraphBuilder& builder,
                                                    kt::rdr::Renderer& renderer) {
  layerStack.emplaceLayer<ExampleLayer>(window, builder, renderer);
  return {};
}
