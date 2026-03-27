#pragma once

#include "keptech/renderer.hpp"
#include <keptech/core/layers/layerStack.hpp>
#include <keptech/core/window.hpp>

#ifdef KEPTECH_RENDERER_VULKAN
#include <keptech/vulkan/renderer.hpp>
#endif

namespace kt {
  struct SetupInfo {
    core::window::CreateInfo window = {};
    RendererCreateInfo renderer = {};
  };

  // To be defined by the application
  [[nodiscard]] SetupInfo configureApp();
  std::expected<void, std::string>
  setupAppLayers(core::layers::LayerStack& layerStack,
                 core::window::Window& window, Renderer& renderer);
} // namespace kt
