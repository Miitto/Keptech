#pragma once

#include <cstdint>
#include <span>
#include <spdlog/fmt/bundled/format.h>
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
  enum class RenderingMode : uint8_t {
    Deferred,
    Forward,
    DeferredLighting,
    Custom,
  };

  enum class DataType : uint8_t {
    None,
    Void,
    Bool,
    F16,
    F32,
    F64,
    F16_2,
    F32_2,
    F64_2,
    F16_3,
    F32_3,
    F64_3,
    F16_4,
    F32_4,
    F64_4,
    I8,
    I16,
    I32,
    I64,
    I8_2,
    I16_2,
    I32_2,
    I64_2,
    I8_3,
    I16_3,
    I32_3,
    I64_3,
    I8_4,
    I16_4,
    I32_4,
    I64_4,
    U8,
    U16,
    U32,
    U64,
    U8_2,
    U16_2,
    U32_2,
    U64_2,
    U8_3,
    U16_3,
    U32_3,
    U64_3,
    U8_4,
    U16_4,
    U32_4,
    U64_4,
    F32_4x4,
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
    std::span<const std::span<const DataType>> vertexLayout;
  };
} // namespace keptech::shaders

template <>
struct fmt::formatter<keptech::shaders::DataType>
    : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const keptech::shaders::DataType t, FormatContext& ctx) const {
    using S = keptech::shaders::DataType;
    std::string_view name = "";

#define N(_n)                                                                  \
  case S::_n:                                                                  \
    name = #_n;                                                                \
    break;

    switch (t) {
      N(None)
      N(Void)
      N(Bool)
      N(F16)
      N(F32)
      N(F64)
      N(F16_2)
      N(F32_2)
      N(F64_2)
      N(F16_3)
      N(F32_3)
      N(F64_3)
      N(F16_4)
      N(F32_4)
      N(F64_4)
      N(I8)
      N(I16)
      N(I32)
      N(I64)
      N(I8_2)
      N(I16_2)
      N(I32_2)
      N(I64_2)
      N(I8_3)
      N(I16_3)
      N(I32_3)
      N(I64_3)
      N(I8_4)
      N(I16_4)
      N(I32_4)
      N(I64_4)
      N(U8)
      N(U16)
      N(U32)
      N(U64)
      N(U8_2)
      N(U16_2)
      N(U32_2)
      N(U64_2)
      N(U8_3)
      N(U16_3)
      N(U32_3)
      N(U64_3)
      N(U8_4)
      N(U16_4)
      N(U32_4)
      N(U64_4)
      N(F32_4x4)
      N(Sampler2D)
    }
#undef N
    return fmt::formatter<std::string_view>::format(name, ctx);
  }
};
