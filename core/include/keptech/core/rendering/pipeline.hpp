#pragma once

#include "image.hpp"
#include "keptech/core/bitflag.hpp"
#include "keptech/core/macros.hpp"
#include <vector>

namespace keptech::core::rendering {
  enum class ShaderStages : uint8_t {
    Vertex = BIT(0),
    Fragment = BIT(1),
    Compute = BIT(2),
  };
}

DEFINE_BITFLAG_ENUM_OPERATORS(keptech::core::rendering::ShaderStages)

namespace keptech::core::rendering {
  struct ShaderStage {
    ShaderStages stage;
    std::string_view name;
  };

  struct ShaderCreateInfo {
    uint8_t const* code = nullptr;
    size_t size = 0;
    std::vector<ShaderStage> stages = {
        {.stage = ShaderStages::Vertex, .name = "vert"},
        {.stage = ShaderStages::Fragment, .name = "frag"}};
  };

  struct AttachmentConfig {
    std::vector<Format> colorFormats = {};
    Format depthFormat = Format::Undefined;
    Format stencilFormat = Format::Undefined;
  };

  enum class Topology : uint8_t {
    TriangleList,
    TriangleStrip,
    LineList,
    LineStrip,
    PointList,
  };

  enum class PolygonMode : uint8_t {
    Fill,
    Line,
    Point,
  };

  enum class CullMode : uint8_t {
    None,
    Front,
    Back,
    FrontAndBack,
  };

  enum class FrontFace : uint8_t {
    Clockwise,
    CounterClockwise,
  };

  struct RasterizerConfig {
    PolygonMode polygonMode = PolygonMode::Fill;
    CullMode cullMode = CullMode::None;
    FrontFace frontFace = FrontFace::Clockwise;
  };

  enum class BlendFactor : uint8_t {
    Zero,
    One,
    SrcAlpha,
    OneMinusSrcAlpha,
  };

  struct BlendConfig {
    bool enableBlending = false;
    BlendFactor src = BlendFactor::SrcAlpha;
    BlendFactor dst = BlendFactor::OneMinusSrcAlpha;
  };

  struct PushConstantRange {
    uint32_t offset = 0;
    uint32_t size = 0;
    Bitflag<ShaderStages> stages =
        ShaderStages::Vertex | ShaderStages::Fragment;
  };

  struct LayoutConfig {
    std::vector<PushConstantRange> pushConstantRanges = {};
  };

  struct PipelineCreateInfo {
    std::vector<ShaderCreateInfo> shaders;
    AttachmentConfig attachments = {};
    Topology topology = Topology::TriangleList;
    RasterizerConfig rasterizer = {};
    BlendConfig blend = {};
    LayoutConfig layout = {};
  };
} // namespace keptech::core::rendering
