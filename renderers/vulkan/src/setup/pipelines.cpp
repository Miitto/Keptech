#include "setup.hpp"

#include "helpers/conversions.hpp"
#include "keptech/vulkan/helpers/formatting.hpp"
#include "keptech/vulkan/helpers/pipeline.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include <keptech/shaders/shader.h>
#include <vulkan/vulkan.h>

namespace shaders {
  namespace {
#include "shaders/keptech/basic.h"
#include "shaders/keptech/deferred.h"
  } // namespace
} // namespace shaders

namespace kt::vkh::setup {

  namespace {

    struct Shader {
      VkShaderModule module;
      std::vector<VkPipelineShaderStageCreateInfo> stages;
    };

    std::expected<Shader, std::string> getShader(const shaders::Shader& shader, const VkDevice device) {
      VkShaderModule shaderModule = nullptr;
      VkShaderModuleCreateInfo shaderModuleCreateInfo{
          .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
          .codeSize = shader.code.size() * sizeof(uint8_t),
          .pCode = reinterpret_cast<const uint32_t*>(shader.code.data()),
      };
      VK_MAKE(vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule),
              "Failed to create shader module for deferred pipeline.");

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

    VertexInput getVertexInputFromShader(const shaders::Shader& shader, std::vector<uint32_t> instanceBindings) {
      std::vector<VkVertexInputAttributeDescription> vertexAttributes;
      uint32_t binding = 0;
      for (auto& param : shader.vertexLayout) {
        uint32_t voffset = 0;
        uint32_t location = 0;
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

  } // namespace
  std::expected<Pipeline, std::string> createBasicPipeline(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                           const VkDescriptorSetLayout globalLayout) {
    VK_DEBUG("Creating basic pipeline");
    VKH_MAKE(shader, getShader(::shaders::basic, vkcore.device.logical), "Failed to create shader for basic pipeline.");

    GraphicsPipelineConfig config{
        .rendering = {.colorAttachmentFormats = {formats.swapchain}},
        .shaders = shader.stages,
        .vertexInput = getVertexInputFromShader(::shaders::basic, {}),
    };

    PipelineLayoutConfig layoutConfig{
        .setLayouts = {},
        .pushConstantRanges = {{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(glm::mat4) * 2,
        }},
    };

    auto vkLayoutInfo = layoutConfig.build();

    VkPipelineLayout vkLayout = nullptr;
    VK_MAKE(vkCreatePipelineLayout(vkcore.device.logical, &vkLayoutInfo, nullptr, &vkLayout),
            "Failed to create pipeline layout for basic pipeline.");

    auto vkConfig = config.build();
    vkConfig.layout = vkLayout;
    VkPipeline vkPipeline = nullptr;
    VK_MAKE(vkCreateGraphicsPipelines(vkcore.device.logical, nullptr, 1, &vkConfig, nullptr, &vkPipeline),
            "Failed to create graphics pipeline for basic pipeline.");

    vkDestroyShaderModule(vkcore.device.logical, shader.module, nullptr);

    return Pipeline{
        .layout = vkLayout,
        .pipeline = vkPipeline,
    };
  }

  std::expected<Pipeline, std::string> createDeferredPipeline(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                              const VkDescriptorSetLayout globalLayout) {
    VK_DEBUG("Creating deferred pipeline");
    VKH_MAKE(shader, getShader(::shaders::deferred, vkcore.device.logical), "Failed to create shader for deferred pipeline.");

    GraphicsPipelineConfig config{
        .rendering =
            {
                .colorAttachmentFormats =
                    {
                        formats.render.albedo,
                        formats.render.normal,
                        formats.render.emissive,
                        formats.render.metRought,
                    },
                .depthAttachmentFormat = formats.render.depth,
            },
        .shaders = shader.stages,
        .vertexInput = getVertexInputFromShader(::shaders::deferred, {}),
        .rasterizer =
            {
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .lineWidth = 1.f,
            },
        .depthStencilState =
            {
                .depthBoundsTestEnable = VK_TRUE,
                .depthTestEnable = VK_TRUE,
                .depthCompareOp = VK_COMPARE_OP_LESS,
                .depthWriteEnable = VK_TRUE,
                .minDepthBounds = 0.f,
                .maxDepthBounds = 1.f,
            },
    };

    PipelineLayoutConfig layoutConfig{
        .setLayouts = {globalLayout},
        .pushConstantRanges = {{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(glm::mat4) + sizeof(VkDeviceAddress),
        }},
    };

    auto vkLayoutInfo = layoutConfig.build();
    auto vkConfig = config.build();

    Pipeline pipeline{};
    VK_MAKE(vkCreatePipelineLayout(vkcore.device.logical, &vkLayoutInfo, nullptr, &pipeline.layout),
            "Failed to create pipeline layout for deferred pipeline.");
    vkConfig.layout = pipeline.layout;
    VK_MAKE(vkCreateGraphicsPipelines(vkcore.device.logical, nullptr, 1, &vkConfig, nullptr, &pipeline.pipeline),
            "Failed to create graphics pipeline for deferred pipeline.");

    vkDestroyShaderModule(vkcore.device.logical, shader.module, nullptr);

    return pipeline;
  } // namespace

  std::expected<Renderer::Pipelines, std::string> createPipelines(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                                  const VkDescriptorSetLayout globalLayout) {
    VKH_MAKE(basic, createBasicPipeline(vkcore, formats, globalLayout), "Failed to create basic pipeline.");
    VKH_MAKE(deferred, createDeferredPipeline(vkcore, formats, globalLayout), "Failed to create deferred pipeline.");

    return Renderer::Pipelines{
        .basic = basic,
        .deferred = deferred,
    };
  }
} // namespace kt::vkh::setup
