#include "pipeline.hpp"

#include "helpers/conversions.hpp"
#include "helpers/pipeline.hpp"
#include "macros.hpp"
#include "vk-logger.hpp"
#include <algorithm>
#include <keptech/shaders/shader.h>

namespace kt::vkh {

  std::expected<Shader, std::string> Shader::create(const VkDevice device, const shaders::Shader& shader) {
    VK_ASSERT(!shader.code.empty(), "Shader code is empty.");
    VK_ASSERT(!shader.stages.empty(), "Shader stages are empty.");
    VkShaderModule shaderModule = nullptr;
    VkShaderModuleCreateInfo shaderModuleCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shader.code.size() * sizeof(uint8_t),
        .pCode = reinterpret_cast<const uint32_t*>(shader.code.data()),
    };
    VK_MAKE(vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule),
            "Failed to create shader module for deferred pipeline.");
    VK_ASSERT(shaderModule != nullptr, "Shader module creation returned null.");

    std::vector<VkPipelineShaderStageCreateInfo> stages(shader.stages.size());
    for (size_t i = 0; i < shader.stages.size(); ++i) {
      stages[i] = VkPipelineShaderStageCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = from(shader.stages[i].stage),
          .module = shaderModule,
          .pName = shader.stages[i].name,
      };
    }

    return std::move(Shader{.module = shaderModule, .stages = std::move(stages)});
  }

  std::expected<Pipeline, std::string> Pipeline::createCompute(const VkDevice device, const Shader& shader, const VkPipelineLayout layout) {
    VK_ASSERT(layout != nullptr, "Pipeline layout is null.");
    VK_ASSERT(!shader.stages.empty(), "Shader stages are empty.");
    VK_ASSERT(shader.stages.size() == 1, "Compute Shader has more than one entry, don't know which to use.");

    VkComputePipelineCreateInfo pipelineCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = shader.stages[0],
        .layout = layout,
    };

    VkPipeline vkPipeline = nullptr;
    VK_MAKE(vkCreateComputePipelines(device, nullptr, 1, &pipelineCreateInfo, nullptr, &vkPipeline),
            "Failed to create compute pipeline for shader.");

    return Pipeline{
        .layout = layout,
        .pipeline = vkPipeline,
    };
  }

  std::expected<Pipeline, std::string> Pipeline::createGraphics(const VkDevice device, GraphicsPipelineConfig config) {
    auto vkConfig = config.build();

    VkPipeline vkPipeline = nullptr;
    VK_MAKE(vkCreateGraphicsPipelines(device, nullptr, 1, &vkConfig, nullptr, &vkPipeline),
            "Failed to create graphics pipeline for shader.");

    return Pipeline{
        .layout = config._layout,
        .pipeline = vkPipeline,
    };
  }

  std::expected<VkPipelineLayout, VkResult> Pipeline::createLayout(const VkDevice device, const PipelineLayoutConfig& plc) {
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(plc.setLayouts.size()),
        .pSetLayouts = plc.setLayouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(plc.pushConstantRanges.size()),
        .pPushConstantRanges = plc.pushConstantRanges.data(),
    };

    VkPipelineLayout layout = nullptr;
    auto result = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &layout);
    if (result != VK_SUCCESS) {
      return std::unexpected(result);
    }

    return layout;
  }

  VertexInput Shader::getVertexInput(const shaders::Shader& shader, std::vector<uint32_t> instanceBindings) {
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
    uint32_t binding = 0;
    uint32_t location = 0;
    for (auto& param : shader.vertexLayout) {
      uint32_t voffset = 0;
      for (auto& type : param) {
        VkVertexInputAttributeDescription attrDesc{
            .location = location++,
            .binding = binding,
            .format = from(type),
            .offset = voffset,
        };
        vertexAttributes.push_back(attrDesc);
        voffset += getSize(type);
      }
      ++binding;
    }
    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::ranges::sort(instanceBindings);
    uint32_t currentBinding = 0;
    for (auto& param : shader.vertexLayout) {
      size_t bindingStride = 0;
      for (auto& type : param) {
        bindingStride += getSize(type);
      }

      auto isInstance = instanceBindings.end() != std::ranges::find(instanceBindings, static_cast<uint32_t>(currentBinding));

      VkVertexInputBindingDescription bindingDesc{
          .binding = static_cast<uint32_t>(currentBinding++),
          .stride = static_cast<uint32_t>(bindingStride),
          .inputRate = isInstance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX,
      };
      vertexBindings.push_back(bindingDesc);
    }

    return {
        .bindings = vertexBindings,
        .attributes = vertexAttributes,
    };
  }

  void Shader::destroy(const VkDevice device) {
    if (module != nullptr) {
      vkDestroyShaderModule(device, module, nullptr);
      module = nullptr;
    }
  }
} // namespace kt::vkh