#pragma once

#include "keptech/core/bitflag.hpp"
#include "texture.hpp"
#include <keptech/shaders/shader.h>
#include <spdlog/fmt/bundled/format.h>
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
    uint32_t extraInstanceDataSize = 0;
  };

  struct PipelineCreateInfo {
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

  struct Pipeline {
    using Handle = core::SlotMapHandle<_priv::PipelineHandleDifferentiator>;
    using SmartHandle =
        core::SlotMapSmartHandle<_priv::PipelineHandleDifferentiator>;

    struct CreateInfo {
      const keptech::shaders::Shader& shader; // NOLINT
      PipelineCreateInfo pipelineConfig{};
    };

    enum class Stage : uint8_t { Deferred, Opaque, Transparent };

    std::string name;
    Stage stage;
    shaders::RenderingMode mode;
  };
} // namespace keptech::core::rendering

template <>
struct fmt::formatter<keptech::core::rendering::Pipeline::Stage>
    : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const keptech::core::rendering::Pipeline::Stage stage,
              FormatContext& ctx) const {
    using S = keptech::core::rendering::Pipeline::Stage;
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
