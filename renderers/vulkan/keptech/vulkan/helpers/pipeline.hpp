#pragma once

#include <Volk/volk.h>
#include <span>
#include <vector>

namespace kt::vkh {
  enum class CullMode : uint32_t { // NOLINT
    None = VK_CULL_MODE_NONE,
    Front = VK_CULL_MODE_FRONT_BIT,
    Back = VK_CULL_MODE_BACK_BIT,
    FrontAndBack = VK_CULL_MODE_FRONT_AND_BACK,
  };

  enum class FrontFace : uint32_t { // NOLINT
    Clockwise = VK_FRONT_FACE_CLOCKWISE,
    CounterClockwise = VK_FRONT_FACE_COUNTER_CLOCKWISE,
  };

  enum class PolygonMode : uint32_t { // NOLINT
    Fill = VK_POLYGON_MODE_FILL,
    Line = VK_POLYGON_MODE_LINE,
    Point = VK_POLYGON_MODE_POINT,
  };

  enum class PrimitiveTopology : uint32_t { // NOLINT
    PointList = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    LineList = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
    LineStrip = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
    TriangleList = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    TriangleStrip = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    TriangleFan = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
    LineListWithAdjacency = VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
    LineStripWithAdjacency = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
    TriangleListWithAdjacency = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY,
    TriangleStripWithAdjacency = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY,
    PatchList = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
  };

  enum class DepthCompareOp : uint32_t { // NOLINT
    Never = VK_COMPARE_OP_NEVER,
    Less = VK_COMPARE_OP_LESS,
    Equal = VK_COMPARE_OP_EQUAL,
    LessOrEqual = VK_COMPARE_OP_LESS_OR_EQUAL,
    Greater = VK_COMPARE_OP_GREATER,
    NotEqual = VK_COMPARE_OP_NOT_EQUAL,
    GreaterOrEqual = VK_COMPARE_OP_GREATER_OR_EQUAL,
    Always = VK_COMPARE_OP_ALWAYS,
  };

  enum class BlendLogicOp : uint32_t { // NOLINT
    Clear = VK_LOGIC_OP_CLEAR,
    And = VK_LOGIC_OP_AND,
    AndReverse = VK_LOGIC_OP_AND_REVERSE,
    Copy = VK_LOGIC_OP_COPY,
    AndInverted = VK_LOGIC_OP_AND_INVERTED,
    NoOp = VK_LOGIC_OP_NO_OP,
    Xor = VK_LOGIC_OP_XOR,
    Or = VK_LOGIC_OP_OR,
    Nor = VK_LOGIC_OP_NOR,
    Equivalent = VK_LOGIC_OP_EQUIVALENT,
    Invert = VK_LOGIC_OP_INVERT,
    OrReverse = VK_LOGIC_OP_OR_REVERSE,
    CopyInverted = VK_LOGIC_OP_COPY_INVERTED,
    OrInverted = VK_LOGIC_OP_OR_INVERTED,
    Nand = VK_LOGIC_OP_NAND,
    Set = VK_LOGIC_OP_SET,
    Disabled,
  };

  enum class BlendFactor : uint32_t { // NOLINT
    Zero = VK_BLEND_FACTOR_ZERO,
    One = VK_BLEND_FACTOR_ONE,
    SrcColor = VK_BLEND_FACTOR_SRC_COLOR,
    OneMinusSrcColor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    DstColor = VK_BLEND_FACTOR_DST_COLOR,
    OneMinusDstColor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    SrcAlpha = VK_BLEND_FACTOR_SRC_ALPHA,
    OneMinusSrcAlpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    DstAlpha = VK_BLEND_FACTOR_DST_ALPHA,
    OneMinusDstAlpha = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    ConstantColor = VK_BLEND_FACTOR_CONSTANT_COLOR,
    OneMinusConstantColor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
    ConstantAlpha = VK_BLEND_FACTOR_CONSTANT_ALPHA,
    OneMinusConstantAlpha = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
    SrcAlphaSaturate = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,
  };

  enum class BlendOp : uint32_t { // NOLINT
    Add = VK_BLEND_OP_ADD,
    Subtract = VK_BLEND_OP_SUBTRACT,
    ReverseSubtract = VK_BLEND_OP_REVERSE_SUBTRACT,
    Min = VK_BLEND_OP_MIN,
    Max = VK_BLEND_OP_MAX,
  };

  /// A class designed to help build a VkPipelineLayoutCreateInfo struct.
  class PipelineLayoutBuilder {
  public:
    PipelineLayoutBuilder();

    /// Add a descriptor set layout to the pipeline layout.
    PipelineLayoutBuilder& addDescriptorSetLayout(VkDescriptorSetLayout layout);
    /// Add a push constant range to the pipeline layout.
    PipelineLayoutBuilder& addPushConstantRange(VkPushConstantRange range);

    /// Add a push constant range to the pipeline layout with the given stage flags and offset. The size of the push constant range is
    /// determined by the size of the provided template types.
    template <typename... Args> PipelineLayoutBuilder& addPushConstantRange(VkShaderStageFlags stageFlags, uint32_t offset) {
      VkPushConstantRange range{
          .stageFlags = stageFlags,
          .offset = offset,
          .size = (sizeof(Args) + ... + 0),
      };
      return addPushConstantRange(range);
    }

    operator VkPipelineLayoutCreateInfo() const noexcept { return _info; }

    [[nodiscard]]
    const VkPipelineLayoutCreateInfo& info() const noexcept {
      return _info;
    }

    PipelineLayoutBuilder(const PipelineLayoutBuilder&);
    PipelineLayoutBuilder& operator=(const PipelineLayoutBuilder&);
    PipelineLayoutBuilder(PipelineLayoutBuilder&&) noexcept;
    PipelineLayoutBuilder& operator=(PipelineLayoutBuilder&&) noexcept;
    ~PipelineLayoutBuilder() = default;

  private:
    void syncDescriptors();
    void syncPushConstants();

    void syncAll() {
      syncDescriptors();
      syncPushConstants();
    }

    std::vector<VkDescriptorSetLayout> _setLayouts;
    std::vector<VkPushConstantRange> _pushConstantRanges;
    VkPipelineLayoutCreateInfo _info{};
  };

  /// A class designed to help build a VkGraphicsPipelineCreateInfo struct.
  class GraphicsPipelineBuilder {
  public:
    GraphicsPipelineBuilder();

    /// Set the pipeline layout for the graphics pipeline.
    GraphicsPipelineBuilder& layout(VkPipelineLayout layout);
    /// Set the number of viewports for the graphics pipeline (default 1).
    GraphicsPipelineBuilder& viewportCount(uint32_t count);
    /// Set the number of scissors for the graphics pipeline (default 1).
    GraphicsPipelineBuilder& scissorCount(uint32_t count);

    /// Set the cull mode for the graphics pipeline (default None).
    GraphicsPipelineBuilder& cullMode(CullMode mode);
    /// Set the front face for the graphics pipeline (default CounterClockwise).
    GraphicsPipelineBuilder& frontFace(FrontFace face);

    /// Set the polygon mode for the graphics pipeline (default Fill).
    GraphicsPipelineBuilder& polygonMode(PolygonMode mode);
    /// Set the primitive topology for the graphics pipeline (default TriangleList).
    GraphicsPipelineBuilder& primitiveTopology(PrimitiveTopology topology);

    /// Set the depth compare operation for the graphics pipeline (default Less).
    GraphicsPipelineBuilder& depthCompareOp(DepthCompareOp op);
    /// Set the depth test enable for the graphics pipeline (default disabled).
    GraphicsPipelineBuilder& depthTest(bool enable);
    /// Enable depth testing for the graphics pipeline.
    GraphicsPipelineBuilder& depthTest();
    // Disable depth testing for the graphics pipeline (default).
    GraphicsPipelineBuilder& noDepthTest();
    /// Set the depth write enable for the graphics pipeline (default disabled).
    GraphicsPipelineBuilder& depthWrite(bool enable);
    /// Enable depth writing for the graphics pipeline.
    GraphicsPipelineBuilder& depthWrite();
    /// Disable depth writing for the graphics pipeline (default).
    GraphicsPipelineBuilder& noDepthWrite();
    /// Set the depth clamp enable for the graphics pipeline (default disabled).
    GraphicsPipelineBuilder& depthClamp(bool enable);
    /// Enable depth clamping for the graphics pipeline.
    GraphicsPipelineBuilder& depthClamp();
    /// Disable depth clamping for the graphics pipeline (default).
    GraphicsPipelineBuilder& noDepthClamp();
    /// Sets the min and max depth bounds, and enables depth bounds testing.
    GraphicsPipelineBuilder& depthBounds(float min, float max);
    /// Disables depth bounds testing (default).
    GraphicsPipelineBuilder& noDepthBounds();

    /// Adds a shader stage to the graphics pipeline.
    GraphicsPipelineBuilder& addShaderStage(VkPipelineShaderStageCreateInfo stage);
    /// Appends shader stages to the graphics pipeline.
    GraphicsPipelineBuilder& addShaderStages(std::span<VkPipelineShaderStageCreateInfo> stages);

    /// Adds a vertex input binding to the graphics pipeline.
    GraphicsPipelineBuilder& addVertexInputBinding(VkVertexInputBindingDescription binding);
    /// Appends vertex input bindings to the graphics pipeline.
    GraphicsPipelineBuilder& addVertexInputBindings(std::span<VkVertexInputBindingDescription> bindings);
    /// Adds a vertex input attribute to the graphics pipeline.
    GraphicsPipelineBuilder& addVertexInputAttribute(VkVertexInputAttributeDescription attribute);
    /// Appends vertex input attributes to the graphics pipeline.
    GraphicsPipelineBuilder& addVertexInputAttributes(std::span<VkVertexInputAttributeDescription> attributes);

    /// Adds a dynamic state to the graphics pipeline.
    GraphicsPipelineBuilder& addDynamicState(VkDynamicState state);
    /// Appends dynamic states to the graphics pipeline.
    GraphicsPipelineBuilder& addDynamicStates(std::span<VkDynamicState> states);

    /// Adds a color attachment without blending to the graphics pipeline.
    /// @see addColorAttachmentFormat(VkFormat, BlendFactor, BlendFactor, BlendOp, BlendFactor, BlendFactor, BlendOp) to add a color
    /// attachment with blending.
    GraphicsPipelineBuilder& addColorAttachment(VkFormat format);
    /// Adds a color attachment with blending to the graphics pipeline.
    /// @see addColorAttachmentFormat(VkFormat) to add a color attachment without blending.
    GraphicsPipelineBuilder& addColorAttachment(VkFormat format, BlendFactor srcColor, BlendFactor dstColor, BlendOp colorOp,
                                                BlendFactor srcAlpha, BlendFactor dstAlpha, BlendOp alphaOp);

    struct ColorAttachmentInfo {
      VkFormat format = VK_FORMAT_UNDEFINED;
      bool blendEnabled = false;
      BlendFactor srcColor = BlendFactor::One;
      BlendFactor dstColor = BlendFactor::Zero;
      BlendOp colorOp = BlendOp::Add;
      BlendFactor srcAlpha = BlendFactor::One;
      BlendFactor dstAlpha = BlendFactor::Zero;
      BlendOp alphaOp = BlendOp::Add;
    };

    /// Appends color attachments to the graphics pipeline.
    GraphicsPipelineBuilder& addColorAttachments(std::span<ColorAttachmentInfo> formats);
    /// Sets the depth attachment format for the graphics pipeline (default VK_FORMAT_UNDEFINED).
    GraphicsPipelineBuilder& depthAttachment(VkFormat format);
    /// Sets the stencil attachment format for the graphics pipeline (default VK_FORMAT_UNDEFINED).
    GraphicsPipelineBuilder& stencilAttachment(VkFormat format);

    /// Sets the blend logic operation for the graphics pipeline. (default Disabled).
    /// Set to BlendLogicOp::Disabled to disable logic operations.
    GraphicsPipelineBuilder& blendLogicOp(BlendLogicOp op);

    operator VkGraphicsPipelineCreateInfo() const noexcept { return _info; }
    [[nodiscard]]
    const VkGraphicsPipelineCreateInfo& info() const noexcept {
      return _info;
    }

    GraphicsPipelineBuilder(const GraphicsPipelineBuilder&);
    GraphicsPipelineBuilder& operator=(const GraphicsPipelineBuilder&);
    GraphicsPipelineBuilder(GraphicsPipelineBuilder&&) noexcept;
    GraphicsPipelineBuilder& operator=(GraphicsPipelineBuilder&&) noexcept;
    ~GraphicsPipelineBuilder() = default;

  private:
    void syncDynamicState();
    void syncVertexBindingState();
    void syncVertexAttributeState();
    void syncColorAttachmentState();
    void syncShaderStages();

    void syncPtrs();

    void syncAll() {
      syncDynamicState();
      syncVertexBindingState();
      syncVertexAttributeState();
      syncColorAttachmentState();
      syncShaderStages();
      syncPtrs();
    }

    std::vector<VkDynamicState> _dynamicStates;
    std::vector<VkVertexInputBindingDescription> _bindings;
    std::vector<VkVertexInputAttributeDescription> _attributes;
    std::vector<VkFormat> _colorAttachmentFormats;
    std::vector<VkPipelineColorBlendAttachmentState> _colorAttachmentStates;
    VkPipelineRenderingCreateInfo _renderingCreateInfo{};
    std::vector<VkPipelineShaderStageCreateInfo> _stages;
    VkPipelineVertexInputStateCreateInfo _vertexInputState{};
    VkPipelineInputAssemblyStateCreateInfo _inputAssemblyState{};
    VkPipelineTessellationStateCreateInfo _tessellationState{};
    VkPipelineViewportStateCreateInfo _viewportState{};
    VkPipelineRasterizationStateCreateInfo _rasterizationState{};
    VkPipelineMultisampleStateCreateInfo _multisampleState{};
    VkPipelineDepthStencilStateCreateInfo _depthStencilState{};
    VkPipelineColorBlendStateCreateInfo _colorBlendState{};
    VkPipelineDynamicStateCreateInfo _dynamicState{};
    VkGraphicsPipelineCreateInfo _info{};
  };
} // namespace kt::vkh
