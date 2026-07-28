#pragma once

#include "keptech/render/interface.hpp"
#include "keptech/render/renderer.hpp"
#include "keptech/render/rendererCreateInfo.hpp"
#include <keptech/core/layers/layerStack.hpp>
#include <keptech/core/window.hpp>

namespace kt {
  struct SetupInfo {
    WindowCreateInfo window = {};
    RendererCreateInfo renderer = {};
  };

  // To be defined by the application
  [[nodiscard]] SetupInfo configureApp();
  std::expected<void, std::string> setupAppLayers(LayerStack& layerStack, Window& window, rdr::RenderGraphBuilder& builder,
                                                  rdr::Renderer& renderer);
} // namespace kt
