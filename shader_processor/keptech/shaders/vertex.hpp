#pragma once

#include "keptech/shaders/dataType.hpp"
#include <cstdint>
#include <spdlog/fmt/bundled/format.h>
#include <string>
#include <vector>

namespace kt::shaders {
  enum class PrimitiveTopology : uint8_t { TriangleList, TriangleStrip };

  enum class CullMode : uint8_t { None, Front, Back, FrontAndBack };

  enum class InputRate : uint8_t { Vertex, Instance };

  struct VertexLayoutEntry {
    DataType type;
    std::string semantic;
    size_t semanticIndex = 0;
    size_t vIndex = 0;
  };

  struct VertexBuffer {
    std::vector<VertexLayoutEntry> layout;
    InputRate inputRate = InputRate::Vertex;
  };

  struct Vertex {
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    CullMode cullMode = CullMode::Back;
    std::vector<VertexBuffer> layout;
  };
} // namespace kt::shaders

template <> struct fmt::formatter<kt::shaders::PrimitiveTopology> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::shaders::PrimitiveTopology& pt, fmt::format_context& ctx) const;
};

template <> struct fmt::formatter<kt::shaders::CullMode> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::shaders::CullMode& cm, fmt::format_context& ctx) const;
};

template <> struct fmt::formatter<kt::shaders::InputRate> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::shaders::InputRate& ir, fmt::format_context& ctx) const;
};