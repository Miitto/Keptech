#pragma once

#include <cstdint>
#include <spdlog/fmt/bundled/format.h>

namespace kt::shaders {
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
}

template <> struct fmt::formatter<kt::shaders::DataType> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::shaders::DataType& dt, fmt::format_context& ctx) const;
};