#pragma once

#include "keptech/core/bitflag.hpp"
#include "keptech/core/macros.hpp"
#include "texture.hpp"
#include <keptech/shaders/shader.h>
#include <string_view>
#include <vector>

namespace keptech::core::rendering {}

DEFINE_BITFLAG_ENUM_OPERATORS(keptech::shaders::ShaderStages)

namespace keptech::core::rendering {
  struct AttachmentConfig {
    std::vector<Texture::Format> colorFormats = {};
    Texture::Format depthFormat = Texture::Format::Undefined;
    Texture::Format stencilFormat = Texture::Format::Undefined;
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

  enum class DepthCompareOp : uint8_t {
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    NotEqual,
    Always,
    Never,
  };

  struct DepthConfig {
    std::optional<DepthCompareOp> depthCompareOp = DepthCompareOp::Less;
    bool depthWrite = true;
  };

  struct PushConstantRange {
    uint32_t offset = 0;
    uint32_t size = 0;
    Bitflag<shaders::ShaderStages> stages =
        shaders::ShaderStages::Vertex | shaders::ShaderStages::Fragment;
  };

  enum class DescriptorType : uint8_t {
    Sampler,
    CombinedImageSampler,
    SampledImage,
    StorageImage,
    UniformTexelBuffer,
    StorageTexelBuffer,
    UniformBuffer,
    StorageBuffer,
    UniformBufferDynamic,
    StorageBufferDynamic,
    InputAttachment,
  };

  struct SetLayout {
    uint32_t binding;
    DescriptorType type;
    Bitflag<shaders::ShaderStages> stages =
        shaders::ShaderStages::Vertex | shaders::ShaderStages::Fragment;
  };

  struct LayoutConfig {
    bool useVertexBuffer = true;
    bool useModelMatrix = true;
    std::vector<SetLayout> setLayouts = {};
    std::vector<PushConstantRange> pushConstantRanges = {};
  };

  struct PipelineCreateInfo {
    AttachmentConfig attachments = {};
    Topology topology = Topology::TriangleList;
    RasterizerConfig rasterizer = {};
    BlendConfig blend = {};
    DepthConfig depth{};
    LayoutConfig layout = {};
  };
} // namespace keptech::core::rendering
