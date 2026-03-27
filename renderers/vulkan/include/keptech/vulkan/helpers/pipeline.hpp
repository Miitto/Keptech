#pragma once

#include <optional>
#include <span>
#include <vector>
#include <vulkan/vulkan.h>

namespace kt::vkh {

  class DynamicStateInfo {
    std::vector<VkDynamicState> dynamicStates;
    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;

  public:
    constexpr DynamicStateInfo(
        std::initializer_list<VkDynamicState> args) noexcept
        : dynamicStates{args} {
      dynamicStateCreateInfo = VkPipelineDynamicStateCreateInfo{
          .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
          .pDynamicStates = dynamicStates.data()};
    }

    operator VkPipelineDynamicStateCreateInfo() const noexcept {
      return dynamicStateCreateInfo;
    }

    operator VkPipelineDynamicStateCreateInfo*() noexcept {
      return &dynamicStateCreateInfo;
    }

    operator const VkPipelineDynamicStateCreateInfo*() const noexcept {
      return &dynamicStateCreateInfo;
    }
  };

  struct PipelineLayoutConfig {
    std::vector<VkDescriptorSetLayout> setLayouts = {};
    std::vector<VkPushConstantRange> pushConstantRanges = {};

    VkPipelineLayoutCreateInfo build() noexcept {
      return VkPipelineLayoutCreateInfo{
          .setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
          .pSetLayouts = setLayouts.data(),
          .pushConstantRangeCount =
              static_cast<uint32_t>(pushConstantRanges.size()),
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
          .pNext = pNext,
          .viewMask = viewMask,
          .colorAttachmentCount =
              static_cast<uint32_t>(colorAttachmentFormats.size()),
          .pColorAttachmentFormats = colorAttachmentFormats.data(),
          .depthAttachmentFormat = depthAttachmentFormat,
          .stencilAttachmentFormat = stencilAttachmentFormat,
      };
    }
  };

  struct GraphicsPipelineConfig {
    struct VertexInput {
      std::span<VkVertexInputBindingDescription> bindings = {};
      std::span<VkVertexInputAttributeDescription> attributes = {};
    };

    RenderingConfig rendering = {};
    std::span<VkPipelineShaderStageCreateInfo> shaders = {};
    VertexInput vertexInput = {};
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .topology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo viewport = {.viewportCount = 1,
                                                  .scissorCount = 1};
    VkPipelineRasterizationStateCreateInfo rasterizer = {
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
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .rasterizationSamples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f};
    VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VkCompareOp::VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
        .stencilTestEnable = VK_FALSE,
    };
    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments = {};
    VkPipelineColorBlendStateCreateInfo blending = {.logicOpEnable = VK_FALSE};
    DynamicStateInfo dynamicState = {VkDynamicState::VK_DYNAMIC_STATE_VIEWPORT,
                                     VkDynamicState::VK_DYNAMIC_STATE_SCISSOR};
    PipelineLayoutConfig layout = {};

    std::optional<VkPipelineVertexInputStateCreateInfo>
        _internalVertexInputInfo = std::nullopt;

    std::optional<VkPipelineRenderingCreateInfo> _internalRenderingInfo =
        std::nullopt;

    VkGraphicsPipelineCreateInfo build() noexcept {
      _internalVertexInputInfo = {
          .vertexBindingDescriptionCount =
              static_cast<uint32_t>(vertexInput.bindings.size()),
          .pVertexBindingDescriptions = vertexInput.bindings.data(),
          .vertexAttributeDescriptionCount =
              static_cast<uint32_t>(vertexInput.attributes.size()),
          .pVertexAttributeDescriptions = vertexInput.attributes.data(),
      };

      blending.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
      blending.pAttachments = blendAttachments.data();

      _internalRenderingInfo = rendering.build();

      return VkGraphicsPipelineCreateInfo{
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
