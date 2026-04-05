#pragma once

#include "keptech/vulkan/structs.hpp"
#include <optional>
#include <span>
#include <vector>
#include <vulkan/vulkan.h>

namespace kt::vkh {

  class DynamicStateInfo {
    std::vector<VkDynamicState> dynamicStates;
    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;

  public:
    constexpr DynamicStateInfo(std::initializer_list<VkDynamicState> args) noexcept : dynamicStates{args} {
      dynamicStateCreateInfo = VkPipelineDynamicStateCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
          .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
          .pDynamicStates = dynamicStates.data(),
      };
    }

    operator VkPipelineDynamicStateCreateInfo() const noexcept { return dynamicStateCreateInfo; }

    operator VkPipelineDynamicStateCreateInfo*() noexcept { return &dynamicStateCreateInfo; }

    operator const VkPipelineDynamicStateCreateInfo*() const noexcept { return &dynamicStateCreateInfo; }
  };

  struct PipelineLayoutConfig {
    std::vector<VkDescriptorSetLayout> setLayouts = {};
    std::vector<VkPushConstantRange> pushConstantRanges = {};

    VkPipelineLayoutCreateInfo build() noexcept {
      return VkPipelineLayoutCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          .setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
          .pSetLayouts = setLayouts.data(),
          .pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
          .pPushConstantRanges = pushConstantRanges.data(),
      };
    }
  };

  struct RenderingConfig {
    void* pNext = nullptr;
    uint32_t viewMask = 0;
    std::vector<VkFormat> colorAttachmentFormats;
    VkFormat depthAttachmentFormat = VkFormat::VK_FORMAT_UNDEFINED;
    VkFormat stencilAttachmentFormat = VkFormat::VK_FORMAT_UNDEFINED;

    VkPipelineRenderingCreateInfo build() noexcept {
      return VkPipelineRenderingCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
          .pNext = pNext,
          .viewMask = viewMask,
          .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentFormats.size()),
          .pColorAttachmentFormats = colorAttachmentFormats.data(),
          .depthAttachmentFormat = depthAttachmentFormat,
          .stencilAttachmentFormat = stencilAttachmentFormat,
      };
    }
  };

  struct GraphicsPipelineConfig {
    RenderingConfig rendering{};
    std::span<VkPipelineShaderStageCreateInfo> shaders = {};
    VertexInput vertexInput{};
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    VkPipelineViewportStateCreateInfo viewport = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VkPolygonMode::VK_POLYGON_MODE_FILL,
        .cullMode = VkCullModeFlagBits::VK_CULL_MODE_NONE,
        .frontFace = VkFrontFace::VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasSlopeFactor = 1.0f,
        .lineWidth = 1.0f,
    };
    VkPipelineMultisampleStateCreateInfo multisampling = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                          .rasterizationSamples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
                                                          .sampleShadingEnable = VK_FALSE,
                                                          .minSampleShading = 1.0f};
    VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VkCompareOp::VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
        .stencilTestEnable = VK_FALSE,
    };
    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments = {};
    VkPipelineColorBlendStateCreateInfo blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
    };
    DynamicStateInfo dynamicState = {VkDynamicState::VK_DYNAMIC_STATE_VIEWPORT, VkDynamicState::VK_DYNAMIC_STATE_SCISSOR};

    std::optional<VkPipelineVertexInputStateCreateInfo> _internalVertexInputInfo = std::nullopt;

    std::optional<VkPipelineRenderingCreateInfo> _internalRenderingInfo = std::nullopt;

    VkGraphicsPipelineCreateInfo build() noexcept {
      _internalVertexInputInfo = {
          .vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInput.bindings.size()),
          .pVertexBindingDescriptions = vertexInput.bindings.data(),
          .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInput.attributes.size()),
          .pVertexAttributeDescriptions = vertexInput.attributes.data(),
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
      blending.pAttachments = blendAttachments.data();

      _internalRenderingInfo = rendering.build();

      _internalRenderingInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
      for (auto& shader : shaders) {
        shader.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      }
      _internalVertexInputInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
      inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
      viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
      rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
      multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
      depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
      blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

      return VkGraphicsPipelineCreateInfo{
          .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
          .pNext = &_internalRenderingInfo.value(),
          .stageCount = static_cast<uint32_t>(shaders.size()),
          .pStages = shaders.data(),
          .pVertexInputState = &*_internalVertexInputInfo,
          .pInputAssemblyState = &inputAssembly,
          .pViewportState = &viewport,
          .pRasterizationState = &rasterizer,
          .pMultisampleState = &multisampling,
          .pDepthStencilState = &depthStencilState,
          .pColorBlendState = &blending,
          .pDynamicState = dynamicState,
      };
    }
  };
} // namespace kt::vkh
