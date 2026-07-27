#pragma once

#include "keptech/core/macros.hpp"
#include "keptech/core/scene.hpp"
#include <concepts>
#include <expected>
#include <keptech/core/window.hpp>
#include <string>

namespace kt {
  enum class RendererCapabilities : uint8_t {
    /// Support for mesh and task shaders.
    MeshShader = BIT(0),
  };

  struct RendererCreateInfo {
    const char* applicationName = "Keptech App";
    Bitflag<RendererCapabilities> capabilities = {};
  };

  template <typename T>
  concept CRenderer = requires(T a, const RendererCreateInfo& ci, const core::window::Window& w, Scene& scene) {
    { T::init(ci, w) } -> std::same_as<std::expected<void, std::string>>;
    { a.newFrame() } -> std::same_as<void>;
  };

} // namespace kt

DEFINE_BITFLAG_ENUM_OPERATORS(kt::RendererCapabilities)