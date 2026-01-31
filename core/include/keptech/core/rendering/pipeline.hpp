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

  enum class VertexInputRate : uint8_t { Vertex, Instance };

  struct LayoutConfig {
    bool useVertexBuffer = true;
    bool useModelMatrix = true;
    std::vector<SetLayout> setLayouts{};
    std::vector<PushConstantRange> pushConstantRanges{};
    std::vector<uint32_t> vertexInstanceBindings{};
    std::vector<shaders::DataType> instanceDataTypes{};
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

  namespace InstanceDataType {
    enum E : uint8_t { Texture, Float, Float2 };
  }
  using InstanceData = std::variant<TexPtr, float, glm::vec2>;
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
struct fmt::formatter<keptech::shaders::RenderingMode>
    : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const keptech::shaders::RenderingMode mode,
              FormatContext& ctx) const {
    using S = keptech::shaders::RenderingMode;
    std::string_view name = "";
    switch (mode) {
    case S::Deferred:
      name = "Deferred";
      break;
    case S::Forward:
      name = "Forward";
      break;
    case keptech::shaders::RenderingMode::Custom:
      name = "Custom";
      break;
    }
    return fmt::formatter<std::string_view>::format(name, ctx);
  }
};

namespace keptech {
  class IPipeline {
  public:
    [[nodiscard]] PipelineStage getStage() const { return stage; }
    [[nodiscard]] const std::vector<shaders::DataType>&
    getInstanceDataTypes() const {
      return instanceDataTypes;
    }

    void setStage(PipelineStage newStage) {
      stage = newStage;
#ifndef NDEBUG
#ifdef KT_ADD_RESOURCE_INFO
      if (stage == PipelineStage::Deferred &&
          mode != shaders::RenderingMode::Deferred) {
        throw std::runtime_error(
            fmt::format("Pipeline '{}' set to Deferred stage but has mode "
                        "'{}'",
                        name, mode));
      }

      if (mode == shaders::RenderingMode::Deferred &&
          stage != PipelineStage::Deferred) {
        throw std::runtime_error(
            fmt::format("Pipeline '{}' has Deferred mode but is set to stage "
                        "'{}'",
                        name, stage));
      }
#endif
#endif
    }

#ifdef KT_ADD_RESOURCE_INFO
    void setName(const std::string& newName) { name = newName; }
    [[nodiscard]] const std::string& getDebugName() const { return name; }
    [[nodiscard]] shaders::RenderingMode getRenderingMode() const {
      return mode;
    }
#endif

  protected:
#ifdef KT_ADD_RESOURCE_INFO
    std::string name;
    shaders::RenderingMode mode;
#endif
    PipelineStage stage;
    std::vector<shaders::DataType> instanceDataTypes = {};
  };

  using PipelinePtr = std::shared_ptr<IPipeline>;

  struct Material {
    PipelinePtr pipeline{nullptr};
    std::vector<InstanceData> instanceData{};
  };

  using MaterialPtr = std::shared_ptr<Material>;
} // namespace keptech
