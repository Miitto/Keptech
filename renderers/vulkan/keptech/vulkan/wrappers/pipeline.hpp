#pragma once

#include "keptech/vulkan/helpers/pipeline.hpp"
#include <Volk/volk.h>
#include <expected>
#include <string>
#include <vector>

namespace kt::shaders {
  struct Shader;
}

namespace kt::vkh {
  struct PipelineLayoutConfig;
  struct GraphicsPipelineConfig;
  struct VertexInput;

  struct Shader {
    VkShaderModule module;
    std::vector<VkPipelineShaderStageCreateInfo> stages;

    static std::expected<Shader, std::string> create(const VkDevice device, const shaders::Shader& shader);
    static VertexInput getVertexInput(const shaders::Shader& shader, std::vector<uint32_t> instanceBindings = {});
    operator std::span<VkPipelineShaderStageCreateInfo>() noexcept { return stages; }

    void destroy(const VkDevice device);
  };

  struct ComputePipelineCreateInfo {
    VkPipelineLayout layout = nullptr;
    Shader shader;
  };

  struct Pipeline {
    VkPipelineLayout layout;
    VkPipeline pipeline;

    static std::expected<VkPipelineLayout, VkResult> createLayout(const VkDevice device, const PipelineLayoutConfig& plc);
    static std::expected<Pipeline, std::string> createCompute(const VkDevice device, const Shader& shader, const VkPipelineLayout layout);
    static std::expected<Pipeline, std::string> createGraphics(const VkDevice device, GraphicsPipelineConfig config);

    template <uint32_t N>
    static std::expected<std::array<Pipeline, N>, std::string> createGraphics(const VkDevice device,
                                                                              std::array<GraphicsPipelineConfig, N> configs) {
      std::array<VkPipeline, N> vkpipelines;
      std::array<VkGraphicsPipelineCreateInfo, N> vkConfigs;
      for (size_t i = 0; i < N; ++i) {
        vkConfigs[i] = configs[i].build();
      }
      VK_MAKE(vkCreateGraphicsPipelines(device, nullptr, N, vkConfigs.data(), nullptr, vkpipelines.data()),
              "Failed to create graphics pipelines.");

      std::array<Pipeline, N> pipelines;
      for (size_t i = 0; i < N; ++i) {
        pipelines[i] = Pipeline{
            .layout = configs[i]._layout,
            .pipeline = vkpipelines[i],
        };
      }

      return pipelines;
    }
  };
} // namespace kt::vkh