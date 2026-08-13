#pragma once

#include "keptech/core/window.hpp"
#include "keptech/rhi/rendererCreateInfo.hpp"
#include <string>

namespace std {
  template <typename T, typename U> class expected;
}

namespace kt {
  class LayerStack;
  class Window;
  class RenderGraphBuilder;

  namespace rhi {
    class RHI;
  }

  struct SetupInfo {
    WindowCreateInfo window = {};
    RendererCreateInfo renderer = {};
  };

  /// A user defined function that configures the application. This function gets called once during engine startup to provide the engine
  /// with information about the application, such as the window title and size, and the renderer settings.
  /// @example
  /// ```cpp
  /// kt::SetupInfo kt::configureApp() {
  ///   return {
  ///       .window = {.title = "My Application", .width = 1280, .height = 720},
  ///       .renderer = {.applicationName = "My Application", .requiredCapabilities = kt::RendererCapabilities::MeshShader},
  ///   };
  /// }
  /// ```
  [[nodiscard]] SetupInfo configureApp();

  /// A user defined function that sets up the application layers. This function gets called once during engine startup after the renderer
  /// has been initialized. The user can add layers to the layer stack. Each layer is a self-contained unit of functionality that can handle
  /// events and update. This function also provides access to the render graph builder and renderer, which can be used to set up the render
  /// graph and load resources.
  /// Layers are updated in the order they are added to the layer stack, but events are propagated through the layers in reverse order.
  /// @example
  /// ```cpp
  /// std::expected<void, std::string> setupAppLayers(kt::LayerStack& layerStack, kt::Window& window, kt::rhi::RenderGraphBuilder& builder,
  /// kt::rhi::Renderer& renderer) {
  ///   // GameLayer will be updated first, but UiLayer will receive events first.
  ///   layerStack.emplaceLayer<GameLayer>(window, builder, renderer);
  ///   layerStack.emplaceLayer<UiLayer>();
  ///   return {};
  /// }
  /// ```
  [[nodiscard]] std::expected<void, std::string> setupAppLayers(LayerStack& layerStack, Window& window, RenderGraphBuilder& builder,
                                                                rhi::RHI& renderer);
} // namespace kt
