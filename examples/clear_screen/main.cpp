#include "keptech/app.hpp"
#include "keptech/core/layers/layerStack.hpp"
#include "layer.hpp"
#include <string>

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

kt::SetupInfo kt::configureApp() {
  return {
      .window = {.title = "Keptech Clear Example",
                 .width = WINDOW_WIDTH,
                 .height = WINDOW_HEIGHT,
                 .flags = WindowCreateFlagBits::Resizable},
      .renderer = {.applicationName = "Keptech Clear Example"},
  };
}

std::expected<void, std::string> kt::setupAppLayers(kt::LayerStack& layerStack, kt::Window& window, kt::RenderGraphBuilder& builder,
                                                    kt::rhi::RHI& rhi) {

  layerStack.emplaceLayer<ExampleLayer>(window, builder, rhi);

  return {};
}
