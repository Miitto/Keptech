#pragma once

#include <cstdint>
#include <span>

#ifndef BIT
#define BIT(x) (1 << (x))
#endif

namespace keptech::shaders {
  enum class ShaderStages : uint8_t {
    Vertex = BIT(0),
    Fragment = BIT(1),
    Compute = BIT(2),
  };
  enum class RenderingMode : uint8_t { Deferred, Forward, Custom };

  struct ShaderStage {
    const char* name;
    ShaderStages stage;
  };

  struct Shader {
    const char* name;
    std::span<const uint8_t> code;
    RenderingMode mode;
    std::span<const ShaderStage> stages;
  };
} // namespace keptech::shaders
