#pragma once

#include <cstdint>
#include <spdlog/fmt/bundled/format.h>


#ifndef BIT
#define BIT(x) (1 << (x))
#endif

namespace kt::shaders {
  enum class ShaderStages : uint8_t {
    Vertex = BIT(0),
    Fragment = BIT(1),
    Geometry = BIT(2),
    Compute = BIT(3),
    Mesh = BIT(4),
    Task = BIT(5),
  };

  struct ShaderStage {
    const char* name;
    ShaderStages stage;
  };
} // namespace kt::shaders

template <> struct fmt::formatter<kt::shaders::ShaderStages> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::shaders::ShaderStages& stage, fmt::format_context& ctx) const;
};