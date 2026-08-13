#include "keptech/app.hpp"
#include "layer.hpp"

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

kt::SetupInfo kt::configureApp() {
  return {
      .window = {.title = "Keptech Basic Example",
                 .width = WINDOW_WIDTH,
                 .height = WINDOW_HEIGHT,
                 .flags = WindowCreateFlagBits::Resizable},
      .renderer = {.applicationName = "Keptech Basic Example"},
  };
}

std::expected<void, std::string> kt::setupAppLayers(kt::LayerStack& layerStack, kt::Window& window, kt::rhi::RenderGraphBuilder& builder,
                                                    kt::rhi::Renderer& renderer) {

  layerStack.emplaceLayer<ExampleLayer>(window, builder, renderer);

  return {};
}
