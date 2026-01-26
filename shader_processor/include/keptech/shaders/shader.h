#pragma once

#include <cstdint>
#include <span>
#include <vector>

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

  enum class DataType {
    Float,
    Float2,
    Float3,
    Float4,
    Int,
    Int2,
    Int3,
    Int4,
    Uint,
    Uint2,
    Uint3,
    Uint4,
    Float4x4,
    Sampler2D,
  };

  struct ShaderStage {
    const char* name;
    ShaderStages stage;
  };

  struct Shader {
    const char* name;
    std::span<const uint8_t> code;
    RenderingMode mode;
    std::span<const ShaderStage> stages;
    std::span<DataType> vertexLayout;
  };
} // namespace keptech::shaders
