#pragma once

#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {

  class DynamicStateInfo {
    std::vector<vk::DynamicState> dynamicStates;
    vk::PipelineDynamicStateCreateInfo dynamicStateCreateInfo;

  public:
    constexpr DynamicStateInfo(
        std::initializer_list<vk::DynamicState> args) noexcept
        : dynamicStates{args} {
      dynamicStateCreateInfo = vk::PipelineDynamicStateCreateInfo{
          .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
          .pDynamicStates = dynamicStates.data()};
    }

    operator vk::PipelineDynamicStateCreateInfo() const noexcept {
      return dynamicStateCreateInfo;
    }

    operator vk::PipelineDynamicStateCreateInfo*() noexcept {
      return &dynamicStateCreateInfo;
    }

    operator const vk::PipelineDynamicStateCreateInfo*() const noexcept {
      return &dynamicStateCreateInfo;
    }
  };

  struct PipelineLayoutConfig {
    std::vector<vk::DescriptorSetLayout> setLayouts = {};
    std::vector<vk::PushConstantRange> pushConstantRanges = {};

    vk::PipelineLayoutCreateInfo build() noexcept {
      return vk::PipelineLayoutCreateInfo{
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
    std::vector<vk::Format> colorAttachmentFormats;
    vk::Format depthAttachmentFormat = vk::Format::eUndefined;
    vk::Format stencilAttachmentFormat = vk::Format::eUndefined;

    vk::PipelineRenderingCreateInfo build() noexcept {
      return vk::PipelineRenderingCreateInfo{
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
      std::span<vk::VertexInputBindingDescription> bindings = {};
      std::span<vk::VertexInputAttributeDescription> attributes = {};
    };

    RenderingConfig rendering = {};
    std::span<vk::PipelineShaderStageCreateInfo> shaders = {};
    VertexInput vertexInput = {};
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly = {
        .topology = vk::PrimitiveTopology::eTriangleList};
    vk::PipelineViewportStateCreateInfo viewport = {.viewportCount = 1,
                                                    .scissorCount = 1};
    vk::PipelineRasterizationStateCreateInfo rasterizer = {
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eClockwise,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasSlopeFactor = 1.0f,
        .lineWidth = 1.0f,
    };
    vk::PipelineMultisampleStateCreateInfo multisampling = {
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f};
    std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments = {
        {.blendEnable = VK_FALSE,
         .colorWriteMask =
             vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
             vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA}};
    vk::PipelineColorBlendStateCreateInfo blending = {.logicOpEnable =
                                                          VK_FALSE};
    DynamicStateInfo dynamicState = {vk::DynamicState::eViewport,
                                     vk::DynamicState::eScissor};
    PipelineLayoutConfig layout = {};

    std::optional<vk::PipelineVertexInputStateCreateInfo>
        _internalVertexInputInfo = std::nullopt;

    std::optional<vk::PipelineRenderingCreateInfo> _internalRenderingInfo =
        std::nullopt;

    vk::GraphicsPipelineCreateInfo build() noexcept {
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

      return vk::GraphicsPipelineCreateInfo{
          .pNext = &_internalRenderingInfo.value(),
          .stageCount = static_cast<uint32_t>(shaders.size()),
          .pStages = shaders.data(),
          .pVertexInputState = &*_internalVertexInputInfo,
          .pInputAssemblyState = &inputAssembly,
          .pViewportState = &viewport,
          .pRasterizationState = &rasterizer,
          .pMultisampleState = &multisampling,
          .pColorBlendState = &blending,
          .pDynamicState = dynamicState,
      };
    }
  };

} // namespace keptech::vkh
