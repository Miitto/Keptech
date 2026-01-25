#pragma once

#include "keptech/core/bitflag.hpp"
#include "texture.hpp"
#include <keptech/shaders/shader.h>
#include <spdlog/fmt/bundled/format.h>
#include <string_view>
#include <variant>
#include <vector>

DEFINE_BITFLAG_ENUM_OPERATORS(keptech::shaders::ShaderStages)

namespace keptech {
  struct AttachmentConfig {
    std::vector<TextureFormat> colorFormats = {};
    TextureFormat depthFormat = TextureFormat::Undefined;
    TextureFormat stencilFormat = TextureFormat::Undefined;
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

  enum class InstanceDataType : uint8_t {
    TextureIndex,
  };

  struct LayoutConfig {
    bool useVertexBuffer = true;
    bool useModelMatrix = true;
    std::vector<SetLayout> setLayouts = {};
    std::vector<PushConstantRange> pushConstantRanges = {};
    std::vector<InstanceDataType> instanceDataTypes = {};
  };

  struct PipelineCreateInfo {
    const keptech::shaders::Shader& shader; // NOLINT
    AttachmentConfig attachments = {};
    Topology topology = Topology::TriangleList;
    RasterizerConfig rasterizer = {};
    BlendConfig blend = {};
    DepthConfig depth{};
    LayoutConfig layout = {};
  };

  namespace _priv {
    struct PipelineHandleDifferentiator {};
  } // namespace _priv

  enum class PipelineStage : uint8_t { Deferred, Opaque, Transparent };

  class IPipeline {
  public:
  private:
#ifdef KT_ADD_RESOURCE_INFO
    std::string name;
    PipelineStage stage;
    shaders::RenderingMode mode;
#endif
    std::vector<InstanceDataType> instanceDataTypes = {};
  };

  using UPipelinePtr = std::unique_ptr<IPipeline>;
  using SPipelinePtr = std::shared_ptr<IPipeline>;
} // namespace keptech

template <>
struct fmt::formatter<keptech::PipelineStage>
    : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const keptech::PipelineStage stage, FormatContext& ctx) const {
    using S = keptech::PipelineStage;
    std::string_view name = "";
    switch (stage) {
    case S::Deferred:
      name = "Deferred";
      break;
    case S::Opaque:
      name = "Opaque";
      break;
    case S::Transparent:
      name = "Transparent";
      break;
    }
    return fmt::formatter<std::string_view>::format(name, ctx);
  }
};

template <>
struct fmt::formatter<keptech::InstanceDataType>
    : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const keptech::InstanceDataType t, FormatContext& ctx) const {
    using S = keptech::InstanceDataType;
    std::string_view name = "";
    switch (t) {
    case S::TextureIndex:
      name = "TextureIndex";
      break;
    }
    return fmt::formatter<std::string_view>::format(name, ctx);
  }
};
