#pragma once

#include "keptech/core/result.hpp"
#include <Volk/volk.h>
#include <span>
#include <vector>

namespace kt::shaders {
  struct Shader;
}

namespace kt::rhi {
  struct VertexInput {
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
  };

  struct Shader {
    VkShaderModule module;
    std::vector<VkPipelineShaderStageCreateInfo> stages;

    static kt::Result<Shader, VkResult, VK_SUCCESS> create(const shaders::Shader& shader);
    static VertexInput getVertexInput(const shaders::Shader& shader, std::vector<uint32_t> instanceBindings = {});
    operator std::span<VkPipelineShaderStageCreateInfo>() noexcept { return stages; }

    void destroy();
  };

  struct ComputePipelineCreateInfo {
    VkPipelineLayout layout = nullptr;
    Shader shader;
  };

  struct Pipeline {
    VkPipelineLayout layout;
    VkPipeline pipeline;

    operator VkPipeline() const noexcept { return pipeline; }
    operator VkPipelineLayout() const noexcept { return layout; }
  };
} // namespace kt::rhi