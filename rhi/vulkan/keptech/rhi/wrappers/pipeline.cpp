#include "pipeline.hpp"

#include "helpers/conversions.hpp"
#include "rhi.hpp"
#include "vk-logger.hpp"
#include "wrappers/device.hpp"
#include <algorithm>
#include <keptech/shaders/shader.h>

namespace kt::rhi {

  kt::Result<Shader, VkResult, VK_SUCCESS> Shader::create(const shaders::Shader& shader) {
    VK_ASSERT(!shader.code.empty(), "Shader code is empty.");
    VK_ASSERT(!shader.stages.empty(), "Shader stages are empty.");
    VkShaderModule shaderModule = nullptr;
    VkShaderModuleCreateInfo shaderModuleCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shader.code.size() * sizeof(uint8_t),
        .pCode = reinterpret_cast<const uint32_t*>(shader.code.data()),
    };
    auto res = vkCreateShaderModule(RHI::get().getDevice(), &shaderModuleCreateInfo, nullptr, &shaderModule);
    if (res != VK_SUCCESS)
      return {res};

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

    return Shader{.module = shaderModule, .stages = std::move(stages)};
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

  void Shader::destroy() {
    if (module != nullptr) {
      vkDestroyShaderModule(RHI::get().getDevice(), module, nullptr);
      module = nullptr;
    }
  }
} // namespace kt::rhi