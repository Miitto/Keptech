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

  struct GraphicsPipelineConfig {
    struct VertexInput {
      std::span<vk::VertexInputBindingDescription> bindings;
      std::span<vk::VertexInputAttributeDescription> attributes;
    };

    vk::PipelineRenderingCreateInfo rendering;
    std::span<vk::PipelineShaderStageCreateInfo> shaders;
    VertexInput vertexInput;
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    vk::PipelineViewportStateCreateInfo viewport;
    vk::PipelineRasterizationStateCreateInfo rasterizer;
    vk::PipelineMultisampleStateCreateInfo multisampling;
    std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments;
    vk::PipelineColorBlendStateCreateInfo blending;
    DynamicStateInfo dynamicState;
    vk::PipelineLayout layout;

    std::optional<vk::PipelineVertexInputStateCreateInfo>
        _internalVertexInputInfo = std::nullopt;

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

      return vk::GraphicsPipelineCreateInfo{
          .pNext = &rendering,
          .stageCount = static_cast<uint32_t>(shaders.size()),
          .pStages = shaders.data(),
          .pVertexInputState = &*_internalVertexInputInfo,
          .pInputAssemblyState = &inputAssembly,
          .pViewportState = &viewport,
          .pRasterizationState = &rasterizer,
          .pMultisampleState = &multisampling,
          .pColorBlendState = &blending,
          .pDynamicState = dynamicState,
          .layout = layout,
      };
    }
  };

} // namespace keptech::vkh
