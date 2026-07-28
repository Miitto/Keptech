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
    /// The capabilities that the renderer must support. If the renderer does not support these capabilities, initialization will fail.
    Bitflag<RendererCapabilities> requiredCapabilities = {};
    /// These capabilities will be enabled if the renderer supports them. If the renderer does not support these capabilities,
    /// initialization will still succeed, but the capabilities will not be enabled.
    Bitflag<RendererCapabilities> requestedCapabilities = {};
  };

} // namespace kt

DEFINE_BITFLAG_ENUM_OPERATORS(kt::RendererCapabilities)