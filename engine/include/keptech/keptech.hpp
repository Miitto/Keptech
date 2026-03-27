#pragma once

#include <keptech/core/gui.h>
#include <keptech/core/rendering/mesh.hpp>
#include <keptech/core/window.hpp>
#include <spdlog/spdlog.h>

namespace kt {
  using Window = core::window::Window;

#ifdef KEPTECH_RENDERER_VULKAN
  using Renderer = kt::vkh::Renderer;
#endif
} // namespace kt
