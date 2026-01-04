#pragma once

#include <keptech/core/layers/layerStack.hpp>
#include <keptech/core/renderer.hpp>
#include <keptech/core/window.hpp>

#ifdef KEPTECH_RENDERER_VULKAN
#include <keptech/vulkan/renderer.hpp>
#endif

namespace keptech {
  struct SetupInfo {
    core::window::CreateInfo window = {};
    core::renderer::CreateInfo renderer = {};
  };

  // To be defined by the application
  [[nodiscard]] SetupInfo configureApp();
  std::expected<void, std::string>
  setupAppLayers(core::layers::LayerStack& layerStack,
                 core::window::Window& window, KEPTECH_RENDERER& renderer);
} // namespace keptech
