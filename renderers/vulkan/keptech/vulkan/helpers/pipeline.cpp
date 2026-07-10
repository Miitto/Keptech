#include "pipeline.hpp"
#include "vk-logger.hpp"

namespace kt::vkh {
  VkPipelineLayoutCreateInfo PipelineLayoutConfig::build() noexcept {
    return VkPipelineLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
        .pSetLayouts = setLayouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
        .pPushConstantRanges = pushConstantRanges.data(),
    };
  }

  VkPipelineRenderingCreateInfo RenderingConfig::build() noexcept {
    uint32_t colorAttachmentCount = static_cast<uint32_t>(colorAttachmentFormats.size());
    VkFormat* colorAttachmentFormatsPtr = colorAttachmentFormats.empty() ? nullptr : colorAttachmentFormats.data();
    return VkPipelineRenderingCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = pNext,
        .viewMask = viewMask,
        .colorAttachmentCount = colorAttachmentCount,
        .pColorAttachmentFormats = colorAttachmentFormatsPtr,
        .depthAttachmentFormat = depthAttachmentFormat,
        .stencilAttachmentFormat = stencilAttachmentFormat,
    };
  }

  VkGraphicsPipelineCreateInfo GraphicsPipelineConfig::build() noexcept {
    uint32_t vertexInputCount = static_cast<uint32_t>(_vertexInput.bindings.size());
    uint32_t vertexAttributeCount = static_cast<uint32_t>(_vertexInput.attributes.size());
    auto* vertexInputBindingsPtr = _vertexInput.bindings.empty() ? nullptr : _vertexInput.bindings.data();
    auto* vertexInputAttributesPtr = _vertexInput.attributes.empty() ? nullptr : _vertexInput.attributes.data();
    _internalVertexInputInfo = {
        .vertexBindingDescriptionCount = vertexInputCount,
        .pVertexBindingDescriptions = vertexInputBindingsPtr,
        .vertexAttributeDescriptionCount = vertexAttributeCount,
        .pVertexAttributeDescriptions = vertexInputAttributesPtr,
    };

    if (rendering.colorAttachmentFormats.size() > blendAttachments.size()) {
      size_t blendDiff = rendering.colorAttachmentFormats.size() - blendAttachments.size();
      for (size_t i = 0; i < blendDiff; ++i) {
        blendAttachments.push_back(VkPipelineColorBlendAttachmentState{
            .blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT});
      }
    }

    blending.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
    blending.pAttachments = blending.attachmentCount == 0 ? nullptr : blendAttachments.data();

    _internalRenderingInfo = rendering.build();

    _internalRenderingInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    for (auto& shader : _shaders) {
      shader.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    }
    _internalVertexInputInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

    VK_ASSERT(_shaders.size() > 0, "At least one shader stage must be provided for graphics pipeline.");

    return VkGraphicsPipelineCreateInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &_internalRenderingInfo.value(),
        .stageCount = static_cast<uint32_t>(_shaders.size()),
        .pStages = _shaders.data(),
        .pVertexInputState = &*_internalVertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewport,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencilState,
        .pColorBlendState = &blending,
        .pDynamicState = dynamicState,
        .layout = _layout,
    };
  }
} // namespace kt::vkh