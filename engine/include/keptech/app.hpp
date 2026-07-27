#pragma once

#include <keptech/core/layers/layerStack.hpp>
#include <keptech/core/window.hpp>
#include <keptech/rendering/interface.hpp>
#include <keptech/rendering/renderer.hpp>

namespace kt {
  struct SetupInfo {
    core::window::CreateInfo window = {};
    RendererCreateInfo renderer = {};
  };

  // To be defined by the application
  [[nodiscard]] SetupInfo configureApp();
  std::expected<void, std::string> setupAppLayers(core::layers::LayerStack& layerStack, core::window::Window& window,
                                                  RenderGraphBuilder& builder, Renderer& renderer);
} // namespace kt
