#pragma once

#include "keptech/core/bitflag.hpp"
#include "keptech/core/macros.hpp"

namespace kt {
  enum class RHICapabilities : uint8_t {
    /// Support for mesh and task shaders.
    MeshShader = BIT(0),
  };

  struct RendererCreateInfo {
    const char* applicationName = "Keptech App";
    /// The capabilities that the renderer must support. If the renderer does not support these capabilities, initialization will fail.
    Bitflag<RHICapabilities> requiredCapabilities = {};
    /// These capabilities will be enabled if the renderer supports them. If the renderer does not support these capabilities,
    /// initialization will still succeed, but the capabilities will not be enabled.
    Bitflag<RHICapabilities> requestedCapabilities = {};
  };

} // namespace kt

DEFINE_BITFLAG_ENUM_OPERATORS(kt::RHICapabilities)