#include "pipeline.hpp"

namespace kt::vkh {
  PipelineLayoutBuilder::PipelineLayoutBuilder() : _info{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO} {}

  PipelineLayoutBuilder& PipelineLayoutBuilder::addDescriptorSetLayout(VkDescriptorSetLayout layout) {
    _setLayouts.push_back(layout);
    syncDescriptors();
    return *this;
  }

  PipelineLayoutBuilder& PipelineLayoutBuilder::addPushConstantRange(VkPushConstantRange range) {
    _pushConstantRanges.push_back(range);
    syncPushConstants();
    return *this;
  }

  PipelineLayoutBuilder::PipelineLayoutBuilder(const PipelineLayoutBuilder& other)
      : _setLayouts{other._setLayouts}, _pushConstantRanges{other._pushConstantRanges}, _info{other._info} {
    syncAll();
  }

  PipelineLayoutBuilder& PipelineLayoutBuilder::operator=(const PipelineLayoutBuilder& other) {
    if (this == &other) {
      return *this;
    }

    _setLayouts = other._setLayouts;
    _pushConstantRanges = other._pushConstantRanges;
    _info = other._info;

    syncAll();

    return *this;
  }

  PipelineLayoutBuilder::PipelineLayoutBuilder(PipelineLayoutBuilder&& other) noexcept
      : _setLayouts{std::move(other._setLayouts)}, _pushConstantRanges{std::move(other._pushConstantRanges)}, _info{other._info} {
    syncAll();
  }

  PipelineLayoutBuilder& PipelineLayoutBuilder::operator=(PipelineLayoutBuilder&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    _setLayouts = std::move(other._setLayouts);
    _pushConstantRanges = std::move(other._pushConstantRanges);
    _info = other._info;

    syncAll();

    return *this;
  }

  void PipelineLayoutBuilder::syncDescriptors() {
    _info.setLayoutCount = static_cast<uint32_t>(_setLayouts.size());
    _info.pSetLayouts = _setLayouts.data();
  }

  void PipelineLayoutBuilder::syncPushConstants() {
    _info.pushConstantRangeCount = static_cast<uint32_t>(_pushConstantRanges.size());
    _info.pPushConstantRanges = _pushConstantRanges.data();
  }

  GraphicsPipelineBuilder::GraphicsPipelineBuilder()
      : _dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}, _bindings{}, _attributes{}, _colorAttachmentFormats{},
        _colorAttachmentStates{},
        _renderingCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        },
        _stages{},
        _vertexInputState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        },
        _inputAssemblyState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        },
        _tessellationState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
        },
        _viewportState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        },
        _rasterizationState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .lineWidth = 1.0f,
        },
        _multisampleState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .minSampleShading = 1.0f,
        },
        _depthStencilState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        },
        _colorBlendState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        },
        _dynamicState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = static_cast<uint32_t>(_dynamicStates.size()),
            .pDynamicStates = _dynamicStates.data(),
        },
        _info{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &_renderingCreateInfo,
            .pVertexInputState = &_vertexInputState,
            .pInputAssemblyState = &_inputAssemblyState,
            .pTessellationState = &_tessellationState,
            .pViewportState = &_viewportState,
            .pRasterizationState = &_rasterizationState,
            .pMultisampleState = &_multisampleState,
            .pDepthStencilState = &_depthStencilState,
            .pColorBlendState = &_colorBlendState,
            .pDynamicState = &_dynamicState,
        } {}

  GraphicsPipelineBuilder& GraphicsPipelineBuilder::layout(VkPipelineLayout layout) {
    _info.layout = layout;
    return *this;
  }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::viewportCount(uint32_t count) {
    _viewportState.viewportCount = count;
    return *this;
  }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::scissorCount(uint32_t count) {
    _viewportState.scissorCount = count;
    return *this;
  }

  GraphicsPipelineBuilder& GraphicsPipelineBuilder::cullMode(CullMode mode) {
    _rasterizationState.cullMode = static_cast<VkCullModeFlags>(mode);
    return *this;
  }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::frontFace(FrontFace face) {
    _rasterizationState.frontFace = static_cast<VkFrontFace>(face);
    return *this;
  }

  GraphicsPipelineBuilder& GraphicsPipelineBuilder::polygonMode(PolygonMode mode) {
    _rasterizationState.polygonMode = static_cast<VkPolygonMode>(mode);
    return *this;
  }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::primitiveTopology(PrimitiveTopology topology) {
    _inputAssemblyState.topology = static_cast<VkPrimitiveTopology>(topology);
    return *this;
  }

  GraphicsPipelineBuilder& GraphicsPipelineBuilder::depthCompareOp(DepthCompareOp op) {
    _depthStencilState.depthCompareOp = static_cast<VkCompareOp>(op);
    return *this;
  }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::depthTest(bool enable) {
    _depthStencilState.depthTestEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
  }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::depthTest() { return depthTest(true); }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::noDepthTest() { return depthTest(false); }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::depthWrite(bool enable) {
    _depthStencilState.depthWriteEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
  }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::depthWrite() { return depthWrite(true); }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::noDepthWrite() { return depthWrite(false); }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::depthClamp(bool enable) {
    _rasterizationState.depthClampEnable = enable ? VK_TRUE : VK_FALSE;
    return *this;
  }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::depthClamp() { return depthClamp(true); }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::noDepthClamp() { return depthClamp(false); }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::depthBounds(float min, float max) {
    _depthStencilState.minDepthBounds = min;
    _depthStencilState.maxDepthBounds = max;
    return *this;
  }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::noDepthBounds() {
    _depthStencilState.minDepthBounds = 0.0f;
    _depthStencilState.maxDepthBounds = 1.0f;
    return *this;
  }

  GraphicsPipelineBuilder& GraphicsPipelineBuilder::addShaderStage(VkPipelineShaderStageCreateInfo stage) {
    _stages.push_back(stage);
    syncShaderStages();
    return *this;
  }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::addShaderStages(std::span<VkPipelineShaderStageCreateInfo> stages) {
    _stages.insert(_stages.end(), stages.begin(), stages.end());
    syncShaderStages();
    return *this;
  }

  GraphicsPipelineBuilder& GraphicsPipelineBuilder::addVertexInputBinding(VkVertexInputBindingDescription binding) {
    _bindings.push_back(binding);
    syncVertexBindingState();
    return *this;
  }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::addVertexInputBindings(std::span<VkVertexInputBindingDescription> bindings) {
    _bindings.insert(_bindings.end(), bindings.begin(), bindings.end());
    syncVertexBindingState();
    return *this;
  }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::addVertexInputAttribute(VkVertexInputAttributeDescription attribute) {
    _attributes.push_back(attribute);
    syncVertexAttributeState();
    return *this;
  }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::addVertexInputAttributes(std::span<VkVertexInputAttributeDescription> attributes) {
    _attributes.insert(_attributes.end(), attributes.begin(), attributes.end());
    syncVertexAttributeState();
    return *this;
  }

  GraphicsPipelineBuilder& GraphicsPipelineBuilder::addDynamicState(VkDynamicState state) {
    _dynamicStates.push_back(state);
    syncDynamicState();
    return *this;
  }

  GraphicsPipelineBuilder& GraphicsPipelineBuilder::addDynamicStates(std::span<VkDynamicState> states) {
    _dynamicStates.insert(_dynamicStates.end(), states.begin(), states.end());
    syncDynamicState();
    return *this;
  }

  GraphicsPipelineBuilder& GraphicsPipelineBuilder::addColorAttachment(VkFormat format) {
    _colorAttachmentFormats.push_back(format);
    _colorAttachmentStates.push_back({
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    });

    syncColorAttachmentState();

    return *this;
  }

  GraphicsPipelineBuilder& GraphicsPipelineBuilder::addColorAttachment(VkFormat format, BlendFactor srcColor, BlendFactor dstColor,
                                                                       BlendOp colorOp, BlendFactor srcAlpha, BlendFactor dstAlpha,
                                                                       BlendOp alphaOp) {
    _colorAttachmentFormats.push_back(format);

    _colorAttachmentStates.push_back({
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = static_cast<VkBlendFactor>(srcColor),
        .dstColorBlendFactor = static_cast<VkBlendFactor>(dstColor),
        .colorBlendOp = static_cast<VkBlendOp>(colorOp),
        .srcAlphaBlendFactor = static_cast<VkBlendFactor>(srcAlpha),
        .dstAlphaBlendFactor = static_cast<VkBlendFactor>(dstAlpha),
        .alphaBlendOp = static_cast<VkBlendOp>(alphaOp),
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    });
    syncColorAttachmentState();

    return *this;
  }

  GraphicsPipelineBuilder& GraphicsPipelineBuilder::addColorAttachments(std::span<ColorAttachmentInfo> formats) {
    for (const auto& format : formats) {
      _colorAttachmentFormats.push_back(format.format);
      _colorAttachmentStates.push_back({
          .blendEnable = format.blendEnabled ? VK_TRUE : VK_FALSE,
          .srcColorBlendFactor = static_cast<VkBlendFactor>(format.srcColor),
          .dstColorBlendFactor = static_cast<VkBlendFactor>(format.dstColor),
          .colorBlendOp = static_cast<VkBlendOp>(format.colorOp),
          .srcAlphaBlendFactor = static_cast<VkBlendFactor>(format.srcAlpha),
          .dstAlphaBlendFactor = static_cast<VkBlendFactor>(format.dstAlpha),
          .alphaBlendOp = static_cast<VkBlendOp>(format.alphaOp),
          .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      });
    }

    syncColorAttachmentState();

    return *this;
  }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::depthAttachment(VkFormat format) {
    _renderingCreateInfo.depthAttachmentFormat = format;
    return *this;
  }
  GraphicsPipelineBuilder& GraphicsPipelineBuilder::stencilAttachment(VkFormat format) {
    _renderingCreateInfo.stencilAttachmentFormat = format;
    return *this;
  }

  GraphicsPipelineBuilder& GraphicsPipelineBuilder::blendLogicOp(BlendLogicOp op) {
    if (op == BlendLogicOp::Disabled) {
      _colorBlendState.logicOpEnable = VK_FALSE;
      _colorBlendState.logicOp = VK_LOGIC_OP_CLEAR;
      return *this;
    }
    _colorBlendState.logicOpEnable = VK_TRUE;
    _colorBlendState.logicOp = static_cast<VkLogicOp>(op);
    return *this;
  }

  void GraphicsPipelineBuilder::syncDynamicState() {
    _dynamicState.dynamicStateCount = static_cast<uint32_t>(_dynamicStates.size());
    _dynamicState.pDynamicStates = _dynamicStates.data();
  }

  void GraphicsPipelineBuilder::syncVertexBindingState() {
    _vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(_bindings.size());
    _vertexInputState.pVertexBindingDescriptions = _bindings.data();
  }

  void GraphicsPipelineBuilder::syncVertexAttributeState() {
    _vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(_attributes.size());
    _vertexInputState.pVertexAttributeDescriptions = _attributes.data();
  }

  void GraphicsPipelineBuilder::syncColorAttachmentState() {
    _renderingCreateInfo.colorAttachmentCount = static_cast<uint32_t>(_colorAttachmentFormats.size());
    _renderingCreateInfo.pColorAttachmentFormats = _colorAttachmentFormats.data();

    _colorBlendState.attachmentCount = static_cast<uint32_t>(_colorAttachmentStates.size());
    _colorBlendState.pAttachments = _colorAttachmentStates.data();
  }

  void GraphicsPipelineBuilder::syncShaderStages() {
    _info.stageCount = static_cast<uint32_t>(_stages.size());
    _info.pStages = _stages.data();
  }

  void GraphicsPipelineBuilder::syncPtrs() {
    _info.pNext = &_renderingCreateInfo;
    _info.pVertexInputState = &_vertexInputState;
    _info.pInputAssemblyState = &_inputAssemblyState;
    _info.pTessellationState = &_tessellationState;
    _info.pViewportState = &_viewportState;
    _info.pRasterizationState = &_rasterizationState;
    _info.pMultisampleState = &_multisampleState;
    _info.pDepthStencilState = &_depthStencilState;
    _info.pColorBlendState = &_colorBlendState;
    _info.pDynamicState = &_dynamicState;
  }

  GraphicsPipelineBuilder::GraphicsPipelineBuilder(const GraphicsPipelineBuilder& other)
      : _dynamicStates{other._dynamicStates}, _bindings{other._bindings}, _attributes{other._attributes},
        _colorAttachmentFormats{other._colorAttachmentFormats}, _colorAttachmentStates{other._colorAttachmentStates},
        _renderingCreateInfo{other._renderingCreateInfo}, _stages{other._stages}, _vertexInputState{other._vertexInputState},
        _inputAssemblyState{other._inputAssemblyState}, _tessellationState{other._tessellationState}, _viewportState{other._viewportState},
        _rasterizationState{other._rasterizationState}, _multisampleState{other._multisampleState},
        _depthStencilState{other._depthStencilState}, _colorBlendState{other._colorBlendState}, _dynamicState{other._dynamicState},
        _info{other._info} {
    syncAll();
  }

  GraphicsPipelineBuilder& GraphicsPipelineBuilder::operator=(const GraphicsPipelineBuilder& other) {
    if (this == &other) {
      return *this;
    }

    _dynamicStates = other._dynamicStates;
    _bindings = other._bindings;
    _attributes = other._attributes;
    _colorAttachmentFormats = other._colorAttachmentFormats;
    _colorAttachmentStates = other._colorAttachmentStates;
    _renderingCreateInfo = other._renderingCreateInfo;
    _stages = other._stages;
    _vertexInputState = other._vertexInputState;
    _inputAssemblyState = other._inputAssemblyState;
    _tessellationState = other._tessellationState;
    _viewportState = other._viewportState;
    _rasterizationState = other._rasterizationState;
    _multisampleState = other._multisampleState;
    _depthStencilState = other._depthStencilState;
    _colorBlendState = other._colorBlendState;
    _dynamicState = other._dynamicState;
    _info = other._info;

    syncAll();

    return *this;
  }

  GraphicsPipelineBuilder::GraphicsPipelineBuilder(GraphicsPipelineBuilder&& other) noexcept
      : _dynamicStates{std::move(other._dynamicStates)}, _bindings{std::move(other._bindings)}, _attributes{std::move(other._attributes)},
        _colorAttachmentFormats{std::move(other._colorAttachmentFormats)}, _colorAttachmentStates{std::move(other._colorAttachmentStates)},
        _renderingCreateInfo{other._renderingCreateInfo}, _stages{std::move(other._stages)}, _vertexInputState{other._vertexInputState},
        _inputAssemblyState{other._inputAssemblyState}, _tessellationState{other._tessellationState}, _viewportState{other._viewportState},
        _rasterizationState{other._rasterizationState}, _multisampleState{other._multisampleState},
        _depthStencilState{other._depthStencilState}, _colorBlendState{other._colorBlendState}, _dynamicState{other._dynamicState},
        _info{other._info} {
    syncAll();
  }

  GraphicsPipelineBuilder& GraphicsPipelineBuilder::operator=(GraphicsPipelineBuilder&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    _dynamicStates = std::move(other._dynamicStates);
    _bindings = std::move(other._bindings);
    _attributes = std::move(other._attributes);
    _colorAttachmentFormats = std::move(other._colorAttachmentFormats);
    _colorAttachmentStates = std::move(other._colorAttachmentStates);
    _renderingCreateInfo = other._renderingCreateInfo;
    _stages = std::move(other._stages);
    _vertexInputState = other._vertexInputState;
    _inputAssemblyState = other._inputAssemblyState;
    _tessellationState = other._tessellationState;
    _viewportState = other._viewportState;
    _rasterizationState = other._rasterizationState;
    _multisampleState = other._multisampleState;
    _depthStencilState = other._depthStencilState;
    _colorBlendState = other._colorBlendState;
    _dynamicState = other._dynamicState;
    _info = other._info;

    syncAll();

    return *this;
  }
} // namespace kt::vkh